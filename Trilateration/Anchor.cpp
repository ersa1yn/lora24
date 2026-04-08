/*

| # | contents |
+ - + -------------- + ----------- + -------- +
| 0 | pkt type [0:1] | BW_ID [2:3] | SF [4:7] |
+ - + -------------- + ----------- + -------- +
+ - + ----------------- + ---------------- +
| 1 | sweepCount [8:11] | reserved [12:15] |
+ - + ----------------- + ---------------- +
+ - + ---------------- + 
| 2 | src ID LSB [0:7] | 
+ - + ---------------- + 
+ - + ---------------- +
| 3 | dst ID LSB [0:7] |
+ - + ---------------- +

Limited to 4 bytes for now

+ - + ------------- +
| 4 | CHNL ID [0:7] | <= RESERVED; NOT IN USE
+ - + ------------- +
+ - + ---------------- + ---------------- +
| 5 | src ID MSB [0:3] | dst ID MSB [4:7] | <= RESERVED; NOT IN USE
+ - + ---------------- + ---------------- +

ptk type:
0 - ranging request
1 - master ping; "I am done, your turn"
2, 3 - reserved

BW_ID:
0 - 406250 Hz 
1 - 812500 Hz
2 - 1625000 Hz
3 - 203125 Hz (N/A for this project)

SF: 5 - 12; 0-4 & 13-15 Not supported by Hardware

CHN_ID: 40 channels corresponding to BLE chnls; check "rangingCorrection.h"

src/dst ID: currently only 8 lower bits are used, 256 devices; 
            extendable to cover 4k devices

sweepCount: number of ranging done, multiply by 40

*/

#include "Anchor.h"

Preferences prefs;
uint32_t run_id;

float freqError;
uint32_t rawRng[DEFAULT_SZ];
uint8_t rngRSSI[DEFAULT_SZ];

uint16_t rngValid;
uint16_t rngTimedOut;
uint16_t rngFail;

volatile bool receivedFlag = false;
void setFlag(void) { receivedFlag = true; }

LinkContext linkCtx = { radio, receivedFlag, DEVICE_ID, LED_BUILTIN, true };

void initRunId() {
    // namespace "ranging", read-write (false = RW)
    if (!prefs.begin("ranging", false)) {
        Serial.println("NVS open failed");
        while (true) delay(1000);
    }

    // Load existing value; default = 0 if not present yet
    run_id = prefs.getULong("run_id", 0);

    // Reserve 0 if you want; start at 1 for first real run
    if (run_id == 0) {
        run_id = 1;
        prefs.putULong("run_id", run_id);
    }

    Serial.print("Loaded run_id: ");
    Serial.println(run_id);
}

// Call when a FULL BWxSF sweep is completed successfully
void advanceRunId() {
    run_id++;
    prefs.putULong("run_id", run_id);  // commit to flash
    Serial.print("Advanced run_id to: ");
    Serial.println(run_id);
}

void waitTurnPhase() {
    radio.setBandwidth(DEFAULT_BW);
    radio.setSpreadingFactor(DEFAULT_SF);
    radio.setFrequency(DEFAULT_RF);

    ControlPacket rx;

    while (true) {
        LinkResult temp = awaitAndSendAck(linkCtx, rx, PacketType::MasterDone, parentOf(DEVICE_ID));
        
        // Only main-master is connected to laptop, i.e. Arduino IDE's Serial Output
        if (VERBOSE) {    
            Serial.print(F("Wait Turn: "));
            printLinkResult(temp);
        }

        if (temp == LinkResult::Ok) break;
    }

    // move to next configuration; 
    // experiment only - will be changed for production
    if (MAIN_MASTER) {
        if (rx.sf == 10) {
            if (rx.bwId == 2) {
                rx.bwId = 0; 
                rx.sf = 5;
                advanceRunId();
            }
            else {rx.bwId++; rx.sf = 5;}
        } else {rx.sf++;}
    }
    
    configureSlavePhase(rx);
}

void configureSlavePhase(ControlPacket rx) {
    radio.setBandwidth(DEFAULT_BW);
    radio.setSpreadingFactor(DEFAULT_SF);
    radio.setFrequency(DEFAULT_RF);

    ControlPacket tx = rx;
    tx.type = PacketType::RangingRequest;
    tx.srcId = DEVICE_ID;
    // experiment only - in production target should be flexible
    tx.dstId = TARGET_ID;

    while (true) {
        LinkResult temp = sendAndAwaitAck(linkCtx, tx);
        
        // Only main-master is connected to laptop, i.e. Arduino IDE's Serial Output
        if (VERBOSE) {
            Serial.print(F("Configure Slave: "));
            printLinkResult(temp);   
        }

        if (temp == LinkResult::Ok) break;
    }

    freqError = radio.getFrequencyError();
    
    if (MAIN_MASTER) {
        Serial.println(F("Received Slave's Acknowledgment"));

        Serial.print(F("RSSI:\t\t"));
        Serial.print(radio.getRSSI());
        Serial.println(F(" dBm"));

        Serial.print(F("SNR:\t\t"));
        Serial.print(radio.getSNR());
        Serial.println(F(" dB"));

        Serial.print(F("Frequency Error:\t"));
        Serial.print(freqError);
        Serial.println(F(" Hz"));
    }

    rangingPhase(rx);
}

uint8_t rangingRSSI() {
    uint8_t data[1] = { 0 };
    int state = radio.readRegister(RADIOLIB_SX128X_REG_RANGING_RSSI, data, 1); // 0x0964

    return data[0];
}

void rangingPhase(ControlPacket rx) {
    rngValid = 0;
    rngTimedOut = 0;
    rngFail = 0;

    if (rx.sweepCount > 6) rx.sweepCount = 6;

    for (int i = 0; i < rx.sweepCount * TOTAL_CHNL; i++) {
        rawRng[i] = 0;
        rngRSSI[i] = 0;
    }
    
    if (VERBOSE) {
        Serial.println(F("Ranging ... "));
    }

    radio.setBandwidth(static_cast<float>(BW[rx.bwId]) / 1000.0);
    radio.setSpreadingFactor(rx.sf);

    for (int i = 0; i < rx.sweepCount; i++) {    
        int chnCounter = 0;
        while (chnCounter < TOTAL_CHNL) {
            radio.setFrequency(CHANNELS[chnCounter++]);

            digitalWrite(LED_BUILTIN, HIGH);
            state = radio.range(true, TARGET_ID/*, RNG_CALIB*/);
            digitalWrite(LED_BUILTIN, LOW);

            if (state == RADIOLIB_ERR_NONE) {            
                rawRng[rngValid] = radio.getRangingResultRaw();
                rngRSSI[rngValid] = rangingRSSI();

                rngValid++;
            } else if (state == RADIOLIB_ERR_RANGING_TIMEOUT) {
                rngTimedOut++;
            } else {
                rngFail++;
            }

            if (VERBOSE) {
                Serial.print(" ");
                Serial.print(chnCounter);
            }
        }
    }

    if (VERBOSE) {
        Serial.print(F("Ranging Done! BandWidth: "));
        Serial.print(BW[rx.bwId]);
        Serial.print(F(" ; SF: "));
        Serial.print(rx.sf);
        Serial.println(F(" ;\nPackets stats:"));
        Serial.print(F("Valid:\t"));
        Serial.println(rngValid);
        Serial.print(F("TimedOut:\t"));
        Serial.println(rngTimedOut);
        Serial.print(F("Failed:\t"));
        Serial.println(rngFail);
    }

    passTurnPhase(rx);
}

void passTurnPhase(ControlPacket rx) {
    radio.setBandwidth(DEFAULT_BW);
    radio.setSpreadingFactor(DEFAULT_SF);
    radio.setFrequency(DEFAULT_RF);    
    
    ControlPacket tx = rx;
    tx.type = PacketType::MasterDone;
    tx.srcId = DEVICE_ID;
    tx.dstId = childOf(DEVICE_ID);

    while (true) {
        LinkResult temp = sendAndAwaitAck(linkCtx, tx);
        
        // Only main-master is connected to laptop, i.e. Arduino IDE's Serial Output
        if (VERBOSE) {
            Serial.print(F("Pass Turn: "));
            printLinkResult(temp);
        }

        if (temp == LinkResult::Ok) break;
    }

    while (!dataSendPhase(rx));
}

void connectWiFiIfNeeded() {
    if (WiFi.status() == WL_CONNECTED) return;
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
    }
}

static bool appendJsonf(char* out, size_t cap, size_t* pos, const char* fmt, ...) {
  if (!out || !pos || *pos >= cap) return false;

  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(out + *pos, cap - *pos, fmt, args);
  va_end(args);

  if (n < 0) return false;
  size_t written = (size_t)n;
  if (written >= (cap - *pos)) {
    *pos = cap;
    return false;
  }

  *pos += written;
  return true;
}

bool buildRangingJsonBuffer( char* out, size_t cap, size_t* outLen, 
    uint8_t bwId, uint8_t sf, uint8_t sweepCount) {
  if (!out || !outLen || cap == 0) return false;

  size_t pos = 0;
  uint8_t bwSf = (uint8_t)(((bwId & 0x0F) << 4) | (sf & 0x0F));

  if (!appendJsonf(out, cap, &pos,
    "{\"run_id\":%lu,\"device_id\":%u,\"target_id\":%u,"
    "\"bw_sf\":%u,\"freqError\":%.6f,\"sweepCount\":%u,"
    "\"valid\":%u,\"timeout\":%u,\"fail\":%u,\"raw_rng\":[",
    (unsigned long)run_id,
    (unsigned)DEVICE_ID,
    (unsigned)TARGET_ID,
    (unsigned)bwSf,
    (double)freqError,
    (unsigned)sweepCount,
    (unsigned)rngValid,
    (unsigned)rngTimedOut,
    (unsigned)rngFail)) return false;

  for (uint16_t i = 0; i < rngValid; i++) {
    if (!appendJsonf(out, cap, &pos, "%s%lu",
      (i ? "," : ""), (unsigned long)rawRng[i])) return false;
  }

  if (!appendJsonf(out, cap, &pos, "],\"rng_rssi\":[")) return false;

  for (uint16_t i = 0; i < rngValid; i++) {
    if (!appendJsonf(out, cap, &pos, "%s%u",
      (i ? "," : ""), (unsigned)rngRSSI[i])) return false;
  }

  if (!appendJsonf(out, cap, &pos, "]}")) return false;

  *outLen = pos;
  return true;
}

bool dataSendPhase(ControlPacket rx) {
    connectWiFiIfNeeded();

    char payload[6144];
    size_t payloadLen = 0;

    if (!buildRangingJsonBuffer(payload, sizeof(payload), &payloadLen,
        rx.bwId, rx.sf, rx.sweepCount)) {
        
        Serial.println(F("JSON build failed: buffer too small"));
        return false;
    }

    HTTPClient http;
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");

    int code = http.POST((uint8_t*)payload, payloadLen);
    if (code > 0) {
        String resp = http.getString();
        Serial.printf("POST %d\n", code);
        Serial.println(resp);
        http.end();
        return (code >= 200 && code < 300);
    } else {
        Serial.printf("POST failed: %s\n", http.errorToString(code).c_str());
        http.end();
        return false;
    }
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(9600);

    connectWiFiIfNeeded();
    // initialize SX1280 with default settings
    Serial.print(F("Initializing ... "));
    int state = radio.begin();

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success!"));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true) { delay(10); }
    }
    
    // set the function that will be called
    // when new packet is received
    radio.setPacketReceivedAction(setFlag);

    initRunId();
}

bool main_master_start = false;

void loop() {
    if (MAIN_MASTER && !main_master_start) {
        ControlPacket rx = {PacketType::RangingRequest, 0, 5, 3, DEVICE_ID, TARGET_ID};
        configureSlavePhase(rx);
        main_master_start = true;
    }

    waitTurnPhase();

    Serial.println(F("Going to another circle ... \n"));
}
