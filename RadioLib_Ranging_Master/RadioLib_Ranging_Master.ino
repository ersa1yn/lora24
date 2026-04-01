/*

This example performs ranging exchange between two
SX1280 LoRa radio modules. Ranging allows to measure
distance between the modules using time-of-flight
measurement.

Protocol:
Master sends LoRa packet, Slave responds
In capturing Slave's response, Master captures Frequency Error value

Afterwards, Master does series of Ranging exchanges
In each exchange, calibration is computed:
Fixed clock offset dependent on Master-Slave pair using FEI and gradient table
LNA gain offset dependent on each exchange using RSSI value of Slave Ranging response and LUT

TODO : Rebuild Calibration table specific to LAMBDA80C-24D RF + ESP-32 DSP circuit and its antenna
TODO : Introduce channel hopping for cross-technology interference, multipath problem
TODO : Take some sort of Median value over period of exchange

*/


#define RADIOLIB_LOW_LEVEL (1)

#include <RadioLib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>

#include "rangingCorrection.h" 

#define SAMPLE_SIZE static_cast<uint16_t>(100)
#define DEVICE_ID static_cast<uint16_t>(0x0001)
#define TARGET_ID static_cast<uint16_t>(0x0002)
#define DEFAULT_BW static_cast<float>(812.5)
#define DEFAULT_SF static_cast<uint8_t>(7)
#define DEFAULT_RF static_cast<float>(2400.0)

uint32_t BW[3] = { 406250, 812500, 1625000 };

Preferences prefs;

// ======== CONFIG ========
const char* WIFI_SSID = "yers";
const char* WIFI_PASS = "12345678";
// Laptop IP on WiFi hotspot:
const char* SERVER_URL = "http://10.42.0.1:5000/reading";
// Unique per device
const char* DEVICE_NAME = "feather-master-01";

SX1280 radio = new Module(33, 26, 27, 25);

int state;
bool receivedFlag = false;

uint32_t RUN_ID = 0;

double FREQ_ERROR = 0x0;
uint32_t rawRng[SAMPLE_SIZE];
uint8_t  rngRSSI[SAMPLE_SIZE];

float distance[SAMPLE_SIZE];
float calibClkDrift[SAMPLE_SIZE];
float calibLNAGain[SAMPLE_SIZE];
float calibFinal[SAMPLE_SIZE];

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
    RUN_ID = prefs.getULong("run_id", 0);

    // Reserve 0 if you want; start at 1 for first real run
    if (RUN_ID == 0) {
        RUN_ID = 1;
        prefs.putULong("run_id", RUN_ID);
    }

    Serial.print("Loaded RUN_ID: ");
    Serial.println(RUN_ID);
}

// Call when a FULL BWxSF sweep is completed successfully
void advanceRunId() {
    RUN_ID++;
    prefs.putULong("run_id", RUN_ID);  // commit to flash
    Serial.print("Advanced RUN_ID to: ");
    Serial.println(RUN_ID);
}

void setFlag(void) {
    receivedFlag = true;
}

void communicationPhase(uint8_t BW_ID, uint8_t SF) {
    radio.setBandwidth(DEFAULT_BW);
    radio.setSpreadingFactor(DEFAULT_SF);
    radio.setFrequency(DEFAULT_RF);

    // set the function that will be called
    // when new packet is received
    radio.setPacketReceivedAction(setFlag);

    // 0 - src ID MSB
    // 1 - src ID LSB
    // 2 - dst ID MSB
    // 3 - dst ID LSB
    // 4 - BW_ID [7:4] & SF [3:0]
    // 5 - RF_FREQ_ID <= To be Defined
    // 6 - SAMPLE_SIZE MSB
    // 7 - SAMPLE_SIZE LSB
    uint8_t payload[8] =  { 
        (DEVICE_ID & 0xFF00) >> 8, DEVICE_ID & 0xFF,
        (TARGET_ID & 0xFF00) >> 8, TARGET_ID & 0xFF,
        (BW_ID << 4) | SF, 0x0, 
        (SAMPLE_SIZE & 0xFF00) >> 8, SAMPLE_SIZE & 0xFF
    };

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
        
        uint8_t packetRecipe[8];
        int numBytes = radio.getPacketLength();
        if (numBytes > 8) numBytes = 8;
        state = radio.readData(packetRecipe, numBytes);
        
        uint16_t target = (static_cast<uint16_t>(packetRecipe[2]) << 8) | 
                           static_cast<uint16_t>(packetRecipe[3]); 
        if (target == DEVICE_ID) break;
        
        Serial.println(F("Received someone else LoRa packet"));
    }

    //=========================================
    //  RECEIVED PACKET'S LOG MESSAGE 
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
        FREQ_ERROR = radio.getFrequencyError();
        Serial.print(FREQ_ERROR);
        Serial.println(F(" Hz"));
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println(F("CRC error!"));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
    }

    radio.setBandwidth(static_cast<float>(BW[BW_ID]) / 1000.0);
    radio.setSpreadingFactor(SF);
    rangingPhase(BW_ID, SF);
}

uint8_t rangingRSSI() {
    uint8_t data[1] = { 0 };
    int state = radio.readRegister(RADIOLIB_SX128X_REG_RANGING_RSSI, data, 1); // 0x0964

    return data[0];
}

void rangingPhase(uint8_t BW_ID, uint8_t SF) {
    rngCounter = 0;
    rngValid = 0;
    rngTimedOut = 0;
    rngFail = 0;

    int chnCounter = 0;

    //=========================================
    // GATHERING RANGING SAMPLE
    //=========================================

    Serial.println(F("Ranging ... "));

    while(rngCounter < SAMPLE_SIZE) {
        
        // radio.setFrequency(CHANNELS[chnCounter] / 1e6f);
        // if (chnCounter % 2) chnCounter--; else chnCounter++;
        // chnCounter = (chnCounter + 1) % 40;

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

}

float computeMean(float *sample, uint16_t total) {
    float res = 0.0;
    for (int i = 0; i < total; i++) {
        res += sample[i];
    }
    return res / total;
}

float computeStdDev(float mean, float *sample, uint16_t total) {
    float res = 0.0;
    for (int i = 0; i < total; i++) {
        float temp = mean - sample[i];
        res += temp * temp;
    }
    res = res / total;
    res = sqrt(res);

    return res;
}

void postProcess(uint8_t BW_ID, uint8_t SF) {
    //=========================================
    //  POST-PROCESSING RANGING OUTPUT
    //=========================================

    Serial.println(F("Post-processing ...\n"));

    // Convert the ranging LSB to distance in meter. 
    // The theoretical conversion from register value to distance [m] is given by: 
    // distance [m] = ( complement2( register ) * 150 ) / ( 2^12 * bandwidth[MHz] ) ).
    // The API provide BW in [Hz] so the implemented formula is: 
    // distance [m] = complement2( register ) / bandwidth[Hz] * A, 
    // where A = 150 / (2^12 / 1e6) = 36621.09375
    for (int i = 0; i < rngValid; i++) {
        distance[i] = (static_cast<float>(~rawRng[i] + 1) / static_cast<float>(BW[BW_ID]))
            * 36621.09375f;

        calibClkDrift[i] = distance[i] - (RNG_FGRAD[BW_ID][SF - 5] * FREQ_ERROR / 1e3f);

        if (rngRSSI[i] > 159) {
            Serial.print(F("WARNING, Ranging RSSI should be in range [0, 160): "));
            Serial.println(rngRSSI[i]);

            while(1) {
                delay(10);
            }
        }
        calibLNAGain[i] = distance[i] + RNG_LUT[BW_ID][SF - 5][rngRSSI[i]];

        calibFinal[i] = calibClkDrift[i] + calibLNAGain[i] - distance[i];
    }

    for (int i = rngValid - 1; i >= 0; i++) {
        for (int j = 0; j < i; j++) {
            if (distance[j] > distance[j + 1]) {
                float temp = distance[j];
                distance[j] = distance[j + 1];
                distance[j + 1] = temp;
            }

            if (calibClkDrift[j] > calibClkDrift[j + 1]) {
                float temp = calibClkDrift[j];
                calibClkDrift[j] = calibClkDrift[j + 1];
                calibClkDrift[j + 1] = temp;
            }

            if (calibLNAGain[j] > calibLNAGain[j + 1]) {
                float temp = calibLNAGain[j];
                calibLNAGain[j] = calibLNAGain[j + 1];
                calibLNAGain[j + 1] = temp;
            }

            if (calibFinal[j] > calibFinal[j + 1]) {
                float temp = calibFinal[j];
                calibFinal[j] = calibFinal[j + 1];
                calibFinal[j + 1] = temp;
            }
        }
    }

    float meanVals[4];
    meanVals[0] = computeMean(distance, rngValid);
    meanVals[1] = computeMean(calibClkDrift, rngValid);
    meanVals[2] = computeMean(calibLNAGain, rngValid);
    meanVals[3] = computeMean(calibFinal, rngValid);

    float stdDevVals[4];
    stdDevVals[0] = computeStdDev(meanVals[0], distance, rngValid);
    stdDevVals[1] = computeStdDev(meanVals[1], calibClkDrift, rngValid);
    stdDevVals[2] = computeStdDev(meanVals[2], calibLNAGain, rngValid);
    stdDevVals[3] = computeStdDev(meanVals[3], calibFinal, rngValid);

    float medianVals[4];
    if (rngValid % 2) {
        medianVals[0] = distance[rngValid / 2];
        medianVals[1] = calibClkDrift[rngValid / 2];
        medianVals[2] = calibLNAGain[rngValid / 2];
        medianVals[3] = calibFinal[rngValid / 2];
    } else {
        medianVals[0] = ( distance[rngValid / 2 - 1] + distance[rngValid / 2] ) / 2;
        medianVals[1] = ( calibClkDrift[rngValid / 2 - 1] + calibClkDrift[rngValid / 2] ) / 2;
        medianVals[2] = ( calibLNAGain[rngValid / 2 - 1] + calibLNAGain[rngValid / 2] ) / 2;
        medianVals[3] = ( calibFinal[rngValid / 2 - 1] + calibFinal[rngValid / 2] ) / 2;
    }
        
    Serial.println(F("Raw Distance Measured:"));
    Serial.print(F("Mean:\t"));
    Serial.print(meanVals[0]);
    Serial.print(F("\tStandard Deviation:\t"));
    Serial.print(stdDevVals[0]);
    Serial.print(F("\tMedian:\t"));
    Serial.println(medianVals[0]);

    Serial.println(F("Clock Drift Calibration:"));
    Serial.print(F("Mean:\t"));
    Serial.print(meanVals[1]);
    Serial.print(F("\tStandard Deviation:\t"));
    Serial.print(stdDevVals[1]);
    Serial.print(F("\tMedian:\t"));
    Serial.println(medianVals[1]);

    Serial.println(F("LNA Gain Calibration:"));
    Serial.print(F("Mean:\t"));
    Serial.print(meanVals[2]);
    Serial.print(F("\tStandard Deviation:\t"));
    Serial.print(stdDevVals[2]);
    Serial.print(F("\tMedian:\t"));
    Serial.println(medianVals[2]);
    
    Serial.println(F("Final Calibration:"));
    Serial.print(F("Mean:\t"));
    Serial.print(meanVals[3]);
    Serial.print(F("\tStandard Deviation:\t"));
    Serial.print(stdDevVals[3]);
    Serial.print(F("\tMedian:\t"));
    Serial.println(medianVals[3]);

    Serial.println(F("Post-processind done!\n"));
}

void connectWiFiIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }
}

String buildRangingJson(uint8_t BW_ID, uint8_t SF) {
    uint8_t BW_SF = (uint8_t)(((BW_ID & 0x0F) << 4) | (SF & 0x0F));

    String json;
    json.reserve(256 + rngValid * (20)); // reduce reallocs; tweak as needed

    json += "{";
    json += "\"run_id\":" + String(RUN_ID) + ",";
    json += "\"device_id\":" + String(DEVICE_ID) + ",";
    json += "\"target_id\":" + String(TARGET_ID) + ",";
    json += "\"bw_sf\":" + String(BW_SF) + ",";
    json += "\"freq_error\":" + String(FREQ_ERROR, 6) + ",";
    json += "\"sample_size\":" + String(SAMPLE_SIZE) + ",";
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

bool sendData(uint8_t BW_ID, uint8_t SF) {
    connectWiFiIfNeeded();

    String payload = buildRangingJson(BW_ID, SF);

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

void runAllConfigs() {
    //=========================================
    //  RUN IN EACH CONFIGURATION
    //=========================================

    for (uint8_t BW_ID = 0; BW_ID < 3; BW_ID++) {
        for (uint8_t SF = 5; SF <= 10; SF++) {
            Serial.print(F("Bandwidth: "));
            Serial.println(BW[BW_ID]);
            Serial.print(F("Spreading Factor: "));
            Serial.println(SF);

            communicationPhase(BW_ID, SF);

            // postProcess(BW_ID, SF);
            sendData(BW_ID, SF);
        }
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

    initRunId();
}

void loop() {
    runAllConfigs();

    Serial.println(F("Measuring new batch ... \n"));
    advanceRunId();
}
