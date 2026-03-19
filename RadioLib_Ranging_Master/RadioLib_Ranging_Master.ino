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
#include <cmath>
#include <cstdint>

#include "rangingCorrection.h" 

#define SAMPLE_SIZE 100
#define DEVICE_ID 0x0001
#define TARGET_ID 0x0002

uint32_t BW[3] = { 406250, 812500, 1625000 };

SX1280 radio = new Module(33, 26, 27, 25);

int state;
bool receivedFlag = false;
double FREQ_ERROR = 0x0;

void setFlag(void) {
    receivedFlag = true;
}

void communicationPhase(uint8_t BW_ID, uint8_t SF) {    
    // set the function that will be called
    // when new packet is received
    radio.setPacketReceivedAction(setFlag);

    // Try to communicate with right tranceiver
    while(true) {
        do {
            //=========================================
            //  MASTER TRANSMITTING LORA PACKET
            //=========================================
            
            Serial.println(F("[COMM] Sent LoRa Packet"));
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
            
            //=========================================
            //  MASTER LISTENING LORA RESPONSE
            //=========================================

            Serial.print(F("[COMM] Waiting LoRa ACK ... "));
            state = radio.startReceive();
            if (state == RADIOLIB_ERR_NONE) {
                Serial.println(F("success!"));
            } else {
                Serial.print(F("failed, code "));
                Serial.println(state);
                while (true) { delay(10); }
            }

            delay(2000);
        } while(!receivedFlag);

        byte byteArr[8];
        int numBytes = radio.getPacketLength();
        state = radio.readData(byteArr, numBytes);
        
        if ((byteArr[2] << 8) | byteArr[3] == DEVICE_ID) break;

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
}

int rawRng[SAMPLE_SIZE];
int rngRSSI[SAMPLE_SIZE];
float distance[SAMPLE_SIZE];
float calibClkDrift[SAMPLE_SIZE];
float calibLNAGain[SAMPLE_SIZE];
float calibFinal[SAMPLE_SIZE];

float computeMean(float *sample, int total) {
    float res = 0.0;
    for (int i = 0; i < total; i++) {
        res += sample[i];
    }
    return res / (float) total;
}

float computeStdDev(float mean, float *sample, int total) {
    float res = 0.0;
    for (int i = 0; i < total; i++) {
        float temp = mean - sample[i];
        res += temp * temp;
    }
    res = res / (float) total;
    res = sqrt(res);

    return res;
}

int32_t rangingRSSI() {
    uint8_t data[1] = { 0 };
    int state = radio.readRegister(RADIOLIB_SX128X_REG_RANGING_RSSI, data, 1); // 0x0964

    // return ( (int32_t) (~data[0] + 1) ) / 2;
    return data[0];
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

    while(rngCounter < SAMPLE_SIZE) {

        state = radio.range(true, TARGET_ID/*, RNG_CALIB*/);

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

    // Convert the ranging LSB to distance in meter. 
    // The theoretical conversion from register value to distance [m] is given by: 
    // distance [m] = ( complement2( register ) * 150 ) / ( 2^12 * bandwidth[MHz] ) ).
    // The API provide BW in [Hz] so the implemented formula is: 
    // distance [m] = complement2( register ) / bandwidth[Hz] * A, 
    // where A = 150 / (2^12 / 1e6) = 36621.09375
    for (int i = 0; i < rngValid; i++) {
        distance[i] = ( (float) (~rawRng[i] + 1) ) / ( (float) BW[BW_ID] ) * 36621.09375;

        calibClkDrift[i] = distance[i] - (RNG_FGRAD[BW_ID][SF - 5] * FREQ_ERROR / 1000.0);

        if (rngRSSI[i] > 159 || rngRSSI[i] < 0) {
            Serial.print(F("WARNING, Ranging RSSI should be in range [0, 160): "));
            Serial.println(rngRSSI[i]);

            while(1) {
                delay(10);
            }
        }
        calibLNAGain[i] = distance[i] + RNG_LUT[BW_ID][SF][rngRSSI[i]];

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

    Serial.println(F("Post-processind done!\nMeasuring new batch ... \n"));
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
