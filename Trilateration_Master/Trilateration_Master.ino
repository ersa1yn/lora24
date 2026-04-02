/*

| # | contents |
+ - + -------------- + ----------- + -------- +
| 0 | pkt type [0:1] | BW_ID [2:3] | SF [4:7] |
+ - + -------------- + ----------- + -------- +
+ - + ------------- +
| 1 | CHNL ID [0:7] | <= RESERVED; NOT IN USE
+ - + ------------- +
+ - + ---------------- + ---------------- +
| 2 | src ID MSB [0:3] | dst ID MSB [4:7] | <= RESERVED; NOT IN USE
+ - + ---------------- + ---------------- +
+ - + ---------------- + 
| 3 | src ID LSB [0:7] | 
+ - + ---------------- + 
+ - + ---------------- +
| 4 | dst ID LSB [0:7] |
+ - + ---------------- +
+ - + ----------------- + ---------------- +
| 5 | sweepCount [8:11] | reserved [12:15] |
+ - + ----------------- + ---------------- +

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

#include <RadioLib.h>
#define RADIOLIB_LOW_LEVEL (1)

#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>

#include "../rangingCorrection/rangingCorrection.h"

// ======== CONFIG ========

const bool     MAIN_MASTER = true;
const uint16_t DEVICE_ID = 0x001; // master counted forwards
const uint16_t TARGET_ID = 0x0ff; // slave counted backwards

const float   DEFAULT_BW = 812.5;
const uint8_t DEFAULT_SF = 7;
const float   DEFAULT_RF = 2400.0;
const int     DEFAULT_SZ = 256;
const uint8_t DEFAULT_MULT = 3;

SX1280 radio = new Module(33, 26, 27, 25);

const char* WIFI_SSID = "yers";
const char* WIFI_PASS = "12345678";
// Laptop IP on WiFi hotspot:
const char* SERVER_URL = "http://10.42.0.1:5000/reading";
// Unique per device
const char* DEVICE_NAME = "feather-master-01";

uint32_t BW[3] = { 406250, 812500, 1625000 };

uint16_t anchor_parent[] = {
    0x003, 0x001, 0x002,
};

uint16_t anchor_child[] = {
    0x002, 0x003, 0x001,
};

// ======== CONFIG END ========

int state;

Preferences prefs;
uint32_t run_id = 0;

double FREQ_ERROR = 0x0;
uint32_t rawRng[DEFAULT_SZ];
uint8_t rngRSSI[DEFAULT_SZ];

float distance[DEFAULT_SZ];
float calibClkDrift[DEFAULT_SZ];
float calibLNAGain[DEFAULT_SZ];
float calibFinal[DEFAULT_SZ];

uint16_t rngCounter;
uint16_t rngValid;
uint16_t rngTimedOut;
uint16_t rngFail;

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

bool receivedFlag = false;
void setFlag(void) {
    receivedFlag = true;
}

void waitTurnPhase() {
    radio.setBandwidth(DEFAULT_BW);
    radio.setSpreadingFactor(DEFAULT_SF);
    radio.setFrequency(DEFAULT_RF);

    // set the function that will be called
    // when new packet is received
    radio.setPacketReceivedAction(setFlag);

    uint8_t packetRecipe[8];
    
    while (true) {
        //=========================================
        // MASTER LISTENING LORA PACKET FROM PARENT MASTER
        //=========================================

        digitalWrite(LED_BUILTIN, HIGH);
        Serial.print(F("[COMM] Waiting LoRa Packet "));
        receivedFlag = false;
        state = radio.startReceive();
        do {
            Serial.print(F("."));
            if (state != RADIOLIB_ERR_NONE) {
                Serial.print(F("failed, code "));
                Serial.println(state);
                while (true) { delay(10); }
            }

            delay(500);
        } while (!receivedFlag);
        
        // done listening
        Serial.println(F("\nsuccess!"));
        digitalWrite(LED_BUILTIN, LOW);
        
        // read data
        int numBytes = radio.getPacketLength();
        if (numBytes > 6) numBytes = 6;
        state = radio.readData(packetRecipe, numBytes);

        // check for CRC or whatnot
        if (state == RADIOLIB_ERR_CRC_MISMATCH) {
            Serial.println(F("CRC error!"));
            continue;
        } else {
            Serial.print(F("failed, code "));
            Serial.println(state);
            continue;
        }
        
        // check if packet was addressed to the device
        uint16_t target = static_cast<uint16_t>(packetRecipe[4])    // currently limited
        /* | (static_cast<uint16_t>(packetRecipe[2]) & 0xF << 8)*/; // +4 bits
        if (target == anchor_parent[DEVICE_ID]) break;

        Serial.println(F("Received someone else LoRa packet"));
    }

    //=========================================
    // MASTER TRANSMITTING LORA RESPONSE TO PARENT MASTER
    //=========================================
    
    // For ACK, change src ID with DEVICE_ID, and dst ID with packet's src ID
    uint8_t payload[6] = { 
        packetRecipe[0],
        packetRecipe[1],
        0x0,
        DEVICE_ID,
        packetRecipe[3],
        packetRecipe[5]
    };

    state = radio.transmit(payload, 6);

    // Serial.println(F("[COMM] Sent LoRa Packet Response"));

    //=========================================
    // RECEIVED PACKET'S LOG MESSAGE 
    //=========================================

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("[SX1280] Received packet!"));

        Serial.print(F("[SX1280] RSSI:\t\t"));
        Serial.print(radio.getRSSI());
        Serial.println(F(" dBm"));

        Serial.print(F("[SX1280] SNR:\t\t"));
        Serial.print(radio.getSNR());
        Serial.println(F(" dB"));

        Serial.print(F("[SX1280] Frequency Error:\t"));
        float FREQ_ERROR = radio.getFrequencyError();
        Serial.print(FREQ_ERROR);
        Serial.println(F(" Hz"));
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println(F("CRC error!"));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
    }

    //=========================================
    // DO RANGING WITH SLAVE WITH RECEIVED PARAMS
    //=========================================

    uint8_t BW_ID = (packetRecipe[0] & 0x30) >> 4;
    uint8_t SF = packetRecipe[0] & 0x0F;
    uint8_t sweepCount = (packetRecipe[5] & 0xF0) >> 4;

    if (MAIN_MASTER) {
        if (SF == 10) {
            if (BW_ID == 2) {
                BW_ID = 0; SF = 5;
                advanceRunId();
            }
            else {BW_ID++; SF = 5;}
        } else {SF++;}
    }
    
    communicateRangingPhase(BW_ID, SF, sweepCount);
}

void communicateRangingPhase(uint8_t BW_ID, uint8_t SF, uint8_t sweepCount) {
    radio.setBandwidth(DEFAULT_BW);
    radio.setSpreadingFactor(DEFAULT_SF);
    radio.setFrequency(DEFAULT_RF);

    uint8_t payload[6] =  {
        (0x0 << 6) | (BW_ID << 4) | SF,
        0x0,  
        0x0,
        DEVICE_ID,
        TARGET_ID,
        (sweepCount & 0xF) << 4 | 0x0
    };

    uint8_t packetRecipe[6];

    // Try to communicate with right tranceiver
    while(true) {
        receivedFlag = false;
        digitalWrite(LED_BUILTIN, HIGH);
        do {
            //=========================================
            //  MASTER TRANSMITTING LORA PACKET
            //=========================================
            
            Serial.println(F("[COMM] Sent LoRa Packet"));
            state = radio.transmit(payload, 8);
            
            //=========================================
            //  MASTER LISTENING LORA RESPONSE
            //=========================================

            Serial.print(F("[COMM] Waiting LoRa ACK "));
            state = radio.startReceive();
            
            Serial.print(F("."));
            if (state != RADIOLIB_ERR_NONE) {
                Serial.print(F("failed, code "));
                Serial.println(state);
                while (true) { delay(10); }
            }

            delay(500);
        } while (!receivedFlag);
        Serial.println(F("\nsuccess!"));
        digitalWrite(LED_BUILTIN, LOW);
        
        int numBytes = radio.getPacketLength();
        if (numBytes > 6) numBytes = 6;
        state = radio.readData(packetRecipe, numBytes);

        if (state == RADIOLIB_ERR_CRC_MISMATCH) {
            Serial.println(F("CRC error!"));
            continue;
        } else {
            Serial.print(F("failed, code "));
            Serial.println(state);
            continue;
        }
        
        // check if packet was addressed to the device
        uint16_t target = static_cast<uint16_t>(packetRecipe[4])    // currently limited
        /* | (static_cast<uint16_t>(packetRecipe[2]) & 0xF << 8)*/; // +4 bits

        if (target == DEVICE_ID) break;
        
        Serial.println(F("Received someone else LoRa packet"));
    }

    //=========================================
    //  RECEIVED PACKET'S LOG MESSAGE 
    //=========================================

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("Received packet!"));

        Serial.print(F("From ID:\t\t"));
        char buffer[10];
        sprintf(buffer, "0x%02X%02X", packetRecipe[0], packetRecipe[1]);
        Serial.println(buffer);

        Serial.print(F("RSSI:\t\t"));
        Serial.print(radio.getRSSI());
        Serial.println(F(" dBm"));

        Serial.print(F("SNR:\t\t"));
        Serial.print(radio.getSNR());
        Serial.println(F(" dB"));

        Serial.print(F("Frequency Error:\t"));
        FREQ_ERROR = radio.getFrequencyError();
        Serial.print(FREQ_ERROR);
        Serial.println(F(" Hz"));
    }

    radio.setBandwidth(static_cast<float>(BW[BW_ID]) / 1000.0);
    radio.setSpreadingFactor(SF);
    rangingPhase(BW_ID, SF, sweepCount);
}

uint8_t rangingRSSI() {
    uint8_t data[1] = { 0 };
    int state = radio.readRegister(RADIOLIB_SX128X_REG_RANGING_RSSI, data, 1); // 0x0964

    return data[0];
}

void rangingPhase(uint8_t BW_ID, uint8_t SF, uint8_t sweepCount) {
    rngValid = 0;
    rngTimedOut = 0;
    rngFail = 0;

    //=========================================
    // GATHERING RANGING SAMPLE
    //=========================================

    Serial.println(F("Ranging ... "));

    for (int i = 0; i < sweepCount * 40; i++) {
        rawRng[i] = 0;
        rngRSSI[i] = 0;
    }

    for (int i = 0; i < sweepCount; i++) {    
        int chnCounter = 0;
        while (chnCounter < 40) {
            radio.setFrequency(CHANNELS[chnCounter] / 1000000);
            chnCounter++;

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

            Serial.print(" ");
            Serial.print(rngCounter);
            rngCounter++;
        }
    }

    Serial.print(F("Ranging Done! BandWidth: "));
    Serial.print(BW[BW_ID]);
    Serial.print(F(" ; SF: "));
    Serial.print(SF);
    Serial.println(F(" ; Packets stats:"));
    Serial.print(F("Valid:\t"));
    Serial.println(rngValid);
    Serial.print(F("TimedOut:\t"));
    Serial.println(rngTimedOut);
    Serial.print(F("Failed:\t"));
    Serial.println(rngFail);

    passTurnPhase(BW_ID, SF, sweepCount);
}

void passTurnPhase(uint8_t BW_ID, uint8_t SF, uint8_t sweepCount) {
    radio.setBandwidth(DEFAULT_BW);
    radio.setSpreadingFactor(DEFAULT_SF);
    radio.setFrequency(DEFAULT_RF);    
    
    uint8_t payload[6] {
        (0x1 << 6) | (BW_ID << 4) | SF,
        0x0,  
        0x0,
        DEVICE_ID,
        anchor_child[DEVICE_ID],
        (sweepCount & 0xF) << 4 | 0x0
    };

    radio.transmit(payload, 6);

    uint8_t packetRecipe[6];

    // Try to communicate with right tranceiver
    while(true) {
        receivedFlag = false;
        digitalWrite(LED_BUILTIN, HIGH);
        do {
            //=========================================
            //  MASTER TRANSMITTING LORA PACKET
            //=========================================
            
            Serial.println(F("[COMM] Sent LoRa Packet"));
            state = radio.transmit(payload, 8);
            
            //=========================================
            //  MASTER LISTENING LORA RESPONSE
            //=========================================

            Serial.print(F("[COMM] Waiting LoRa ACK "));
            state = radio.startReceive();
            
            Serial.print(F("."));
            if (state != RADIOLIB_ERR_NONE) {
                Serial.print(F("failed, code "));
                Serial.println(state);
                while (true) { delay(10); }
            }

            delay(500);
        } while (!receivedFlag);
        Serial.println(F("\nsuccess!"));
        digitalWrite(LED_BUILTIN, LOW);
        
        int numBytes = radio.getPacketLength();
        if (numBytes > 6) numBytes = 6;
        state = radio.readData(packetRecipe, numBytes);

        if (state == RADIOLIB_ERR_CRC_MISMATCH) {
            Serial.println(F("CRC error!"));
            continue;
        } else {
            Serial.print(F("failed, code "));
            Serial.println(state);
            continue;
        }
        
        uint8_t pkt_type = packetRecipe[0] >> 6;
        // check if packet was addressed to the device
        uint16_t target = static_cast<uint16_t>(packetRecipe[4])    // currently limited
        /* | (static_cast<uint16_t>(packetRecipe[2]) & 0xF << 8)*/; // +4 bits


        if (target != DEVICE_ID) {
            Serial.println(F("Received someone else LoRa packet"));
            continue;
        }
        if (pkt_type != 0x1) {
            Serial.println(F("Received incorrect packet type"));
            continue;
        }

        break;
    }

    //=========================================
    //  RECEIVED PACKET'S LOG MESSAGE 
    //=========================================

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("Received packet!"));

        Serial.print(F("From ID:\t\t"));
        char buffer[10];
        sprintf(buffer, "0x%02X%02X", packetRecipe[0], packetRecipe[1]);
        Serial.println(buffer);

        Serial.print(F("RSSI:\t\t"));
        Serial.print(radio.getRSSI());
        Serial.println(F(" dBm"));

        Serial.print(F("SNR:\t\t"));
        Serial.print(radio.getSNR());
        Serial.println(F(" dB"));

        Serial.print(F("Frequency Error:\t"));
        FREQ_ERROR = radio.getFrequencyError();
        Serial.print(FREQ_ERROR);
        Serial.println(F(" Hz"));
    }

    dataSendPhase(BW_ID, SF, sweepCount);
}

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

void connectWiFiIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }
}

String buildRangingJson(uint8_t BW_ID, uint8_t SF, uint8_t sweepCount) {
    uint8_t BW_SF = (uint8_t)(((BW_ID & 0x0F) << 4) | (SF & 0x0F));

    String json;
    json.reserve(256 + rngValid * (20)); // reduce reallocs; tweak as needed

    json += "{";
    json += "\"run_id\":" + String(run_id) + ",";
    json += "\"device_id\":" + String(DEVICE_ID) + ",";
    json += "\"target_id\":" + String(TARGET_ID) + ",";
    json += "\"bw_sf\":" + String(BW_SF) + ",";
    json += "\"freq_error\":" + String(FREQ_ERROR, 6) + ",";
    json += "\"sweepCount\":" + String(sweepCount) + ",";
    json += "\"valid\":" + String(rngValid) + ",";
    json += "\"timeout\":" + String(rngTimedOut) + ",";
    json += "\"fail\":" + String(rngFail) + ",";

    json += "\"raw_rng\":[";
    for (uint16_t i = 0; i < rngValid; i++) {
        if (i) json += ",";
        json += String(rawRng[i]);
    }
    json += "],";

    json += "\"rng_rssi\":[";
    for (uint16_t i = 0; i < rngValid; i++) {
        if (i) json += ",";
        json += String(rngRSSI[i]);
    }
    json += "]";

    json += "}";
    return json;
}

bool dataSendPhase(uint8_t BW_ID, uint8_t SF, uint8_t sweepCount) {
    connectWiFiIfNeeded();

    String payload = buildRangingJson(BW_ID, SF, sweepCount);

    HTTPClient http;
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");

    int code = http.POST(payload);
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

bool init = false;

void loop() {
    if (MAIN_MASTER && !init) {
        communicateRangingPhase(0, 5, 3);
        init = false;
    }

    waitTurnPhase();

    Serial.println(F("Going to another circle ... \n"));
}