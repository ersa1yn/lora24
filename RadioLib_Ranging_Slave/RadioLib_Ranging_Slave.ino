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

#define SAMPLE_SIZE 100
#define DEVICE_ID 0x0002
#define TARGET_ID 0x0001

uint32_t BW[3] = { 406250, 812500, 1625000 };

SX1280 radio = new Module(33, 26, 27, 25);

int state;
bool receivedFlag = false;

void setFlag(void) {
    receivedFlag = true;
}

void communicationPhase(uint8_t BW_ID, uint8_t SF) {
    //=========================================
    //  SLAVE LISTENING LORA PACKET
    //=========================================

    // set the function that will be called
    // when new packet is received
    radio.setPacketReceivedAction(setFlag);

    while (true) {

        do {
            Serial.print(F("[COMM] Waiting LoRa Packet ... "));
            state = radio.startReceive();
            if (state == RADIOLIB_ERR_NONE) {
                Serial.println(F("success!"));
            } else {
                Serial.print(F("failed, code "));
                Serial.println(state);
                while (true) { delay(10); }
            }

            delay(2000);
        } while (!receivedFlag);

        byte byteArr[8];
        int numBytes = radio.getPacketLength();
        state = radio.readData(byteArr, numBytes);

        if ((byteArr[2] << 8) | byteArr[3] == DEVICE_ID) break;

        Serial.println(F("Received someone else LoRa packet"));
    }

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
    //  SLAVE TRANSMITTING LORA RESPONSE
    //=========================================

    Serial.println(F("[COMM] Sent LoRa Packet Response"));
    // 0 - src ID MSB
    // 1 - src ID LSB
    // 2 - dst ID MSB
    // 3 - dst ID LSB
    // 4 - BW_ID [7:4] & SF [3:0]
    // 5 - RF_FREQ_ID <= To be Defined
    byte byteArr[6] = { 
        DEVICE_ID >> 8, DEVICE_ID & 0xFF, 
        TARGET_ID >> 8, TARGET_ID & 0xFF, 
        (BW_ID << 4) | SF, 0x0};

    state = radio.transmit(byteArr, 6);
}

void rangingPhase(uint8_t BW_ID, uint8_t SF) {
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
        delay(20);
    }

    //=========================================
    //  POST-PROCESSING RANGING OUTPUT
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
    Serial.println(F("Post-processing ...\n"));
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
    //=========================================
    //  RUN IN EACH CONFIGURATION
    //=========================================

    for (uint8_t BW_ID = 0; BW_ID < 3; BW_ID++) {
        for (uint8_t SF = 0; SF < 6; SF++) {
            radio.setBandwidth( ((float) BW[BW_ID]) / 1000.0 );
            radio.setSpreadingFactor( SF );

            communicationPhase(BW_ID, SF);
            rangingPhase(BW_ID, SF);
        }
    }
}
