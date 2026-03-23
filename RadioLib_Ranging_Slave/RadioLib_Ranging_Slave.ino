/*

This example performs ranging exchange between two
SX1280 LoRa radio modules. Ranging allows to measure
distance between the modules using time-of-flight
measurement.

Protocol:
Master sends LoRa packet, Slave responds
In capturing Slave's response, Master captures Frequency Error value

Next, Master does fixed number of Ranging exchanges
In each exchange, rawRanging and RSSI values are captured
After finishing ranging, post-processing computes calibration and statistical values:
Fixed clock offset dependent on Master-Slave pair using FEI and gradient table
LNA gain offset dependent on each exchange using RSSI value of Slave Ranging response and LUT

TODO : Rebuild Calibration table specific to LAMBDA80C-24D RF + ESP-32 DSP circuit and its antenna
TODO : Introduce channel hopping for cross-technology interference, multipath problem
TODO : Take some sort of Median value over period of exchange

*/

#include <RadioLib.h>
#include <cmath>
#include <cstdint>

#define DEVICE_ID  static_cast<uint16_t>(0x0002)
#define DEFAULT_BW static_cast<float>(812.5)
#define DEFAULT_SF static_cast<uint8_t>(7)

uint32_t BW[3] = { 406250, 812500, 1625000 };

SX1280 radio = new Module(33, 26, 27, 25);

int state;
bool receivedFlag = false;

void setFlag(void) {
    receivedFlag = true;
}

void communicationPhase() {
    
    radio.setBandwidth(DEFAULT_BW);
    radio.setSpreadingFactor(DEFAULT_SF);

    // set the function that will be called
    // when new packet is received
    radio.setPacketReceivedAction(setFlag);

    uint8_t packetRecipe[8];
    

    while (true) {
        //=========================================
        // SLAVE LISTENING LORA PACKET
        //=========================================

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
        Serial.println(F("\nsuccess!"));

        
        int numBytes = radio.getPacketLength();
        if (numBytes > 8) numBytes = 8;
        state = radio.readData(packetRecipe, numBytes);

        uint16_t target = (static_cast<uint16_t>(packetRecipe[2]) << 8) | 
                          static_cast<uint16_t>(packetRecipe[3]); 
        if (target == DEVICE_ID) break;

        Serial.println(F("Received someone else LoRa packet"));
    }

    //=========================================
    // SLAVE TRANSMITTING LORA RESPONSE
    //=========================================

    Serial.println(F("[COMM] Sent LoRa Packet Response"));
    // 0 - src ID MSB
    // 1 - src ID LSB
    // 2 - dst ID MSB
    // 3 - dst ID LSB
    // 4 - BW_ID [7:4] & SF [3:0]
    // 5 - RF_FREQ_ID <= Carrier wave RF, To be Defined
    // 6 - SAMPLE_SIZE MSB
    // 7 - SAMPLE_SIZE LSB
    
    uint8_t payload[8] = { 
        DEVICE_ID >> 8, DEVICE_ID & 0xFF, 
        packetRecipe[0], packetRecipe[1], 
        packetRecipe[4], packetRecipe[5], 
        packetRecipe[6], packetRecipe[7]};

    state = radio.transmit(payload, 8);

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
    // START RANGING PHASE WITH RECEIVED PARAMS
    //=========================================

    uint8_t BW_ID = (packetRecipe[4] & 0xf0) >> 4;
    uint8_t SF = packetRecipe[4] & 0x0f;
    uint16_t SAMPLE_SIZE = (static_cast<uint16_t>(packetRecipe[6]) << 8) | 
                            static_cast<uint16_t>(packetRecipe[7]); 

    radio.setBandwidth(static_cast<float>(BW[BW_ID] / 1000.0));
    radio.setSpreadingFactor(SF);
    rangingPhase(BW_ID, SF, SAMPLE_SIZE);
}

void rangingPhase(uint8_t BW_ID, uint8_t SF, uint16_t SAMPLE_SIZE) {
    int rngCounter = 0;
    int rngValid = 0;
    int rngTimedOut = 0;
    int rngFail = 0;

    //=========================================
    // GATHERING RANGING SAMPLE
    //=========================================

    Serial.println(F("Ranging ... "));

    while (rngCounter < SAMPLE_SIZE) {
        state = radio.range(false, DEVICE_ID /*, RNG_CALIB*/);

        if (state == RADIOLIB_ERR_NONE) {
            rngValid++;
        } else if (state == RADIOLIB_ERR_RANGING_TIMEOUT) {
            rngTimedOut++;
        } else {
            rngFail++;
        }

        rngCounter++;
    }

    //=========================================
    // RANGING OUTPUT
    //=========================================

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
    Serial.println(F("\nSwitching to communication phase ...\n"));

    radio.setBandwidth(DEFAULT_BW);
    radio.setSpreadingFactor(DEFAULT_SF);
    communicationPhase();
}

void setup() {
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
}

void loop() {
    communicationPhase();
}
