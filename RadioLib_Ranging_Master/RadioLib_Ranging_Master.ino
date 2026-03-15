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
#include <utility>
#include <cstdint>

#define NUMBER_OF_FACTORS_PER_SFBW 160
#define BW 1625000
#define BW_ID 2
#define SF 10
#define SAMPLE_SIZE 100

SX1280 radio = new Module(33, 26, 27, 25);

const uint16_t RNG_CALIB[3][6] = {
    { 10299, 10271, 10244, 10242, 10230, 10246 },
    { 11486, 11474, 11453, 11426, 11417, 11401 },
    { 30000, 13493, 13528, 13515, 13430, 13376 }
};

const double RNG_FGRAD[3][6] = { 
    { -0.148, -0.214, -0.419, -0.853, -1.686, -3.423 },
    { -0.041, -0.811, -0.218, -0.429, -0.853, -1.737 },
    { 0.103,  -0.041, -0.101, -0.211, -0.424, -0.87  }
};

const double RangingCorrectionSF9BW0800[NUMBER_OF_FACTORS_PER_SFBW] =
{
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    39.6364,
    -53.2896,
    -35.4979,
    -17.7061,
    0.085636,
    26.9106,
    18.8654,
    10.8202,
    2.775,
    -5.2702,
    -1.6081,
    0.5434,
    5.9908,
    3.0153,
    2.6491,
    2.4202,
    4.3657,
    4.1368,
    4.2971,
    2.8322,
    3.8393,
    3.7935,
    3.4273,
    3.3358,
    3.2442,
    2.9238,
    3.1069,
    3.0611,
    2.8322,
    2.7864,
    2.878,
    2.6949,
    2.7864,
    2.8322,
    2.8322,
    2.6033,
    2.7407,
    2.6949,
    2.6491,
    2.3287,
    2.5576,
    2.6949,
    2.5576,
    2.5576,
    3.3358,
    4.1597,
    3.9309,
    3.9309,
    4.0224,
    4.2513,
    4.2513,
    4.2513,
    4.5259,
    4.4344,
    4.6633,
    4.7548,
    4.4344,
    4.6633,
    4.9379,
    4.7548,
    4.6175,
    5.121,
    5.1668,
    5.0753,
    6.8605,
    7.0436,
    7.0436,
    7.7761,
    7.6845,
    7.6845,
    7.9134,
    7.7761,
    7.8218,
    7.9592,
    7.7761,
    7.8218,
    7.9592,
    8.0507,
    8.0049,
    7.8218,
    8.0965,
    8.1881,
    8.1881,
    8.3712,
    8.8747,
    8.9205,
    8.8289,
    8.9205,
    9.012,
    9.1494,
    9.3325,
    9.2867,
    9.5156,
    9.3782,
    9.2409,
    9.2867,
    9.424,
    9.6987,
    9.8818,
    9.7444,
    9.8818,
    10.0191,
    9.9276,
    9.9733,
    10.0649,
    10.0649,
    10.0649,
    9.9276,
    10.0649,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
    10.1564,
   
}; 

int state;
bool receivedFlag = false;
float FREQ_ERROR;

void setFlag(void) {
    receivedFlag = true;
}

void communicationPhase() {
    //=========================================
    //  MASTER TRANSMITTING LORA PACKET
    //=========================================

    String str;

    //=========================================
    //  MASTER LISTETING LORA RESPONSE
    //=========================================

    // set the function that will be called
    // when new packet is received
    radio.setPacketReceivedAction(setFlag);

    while(true) {

        do {
            Serial.println(F("[COMM] Sent LoRa Packet"));

            byte byteArr[8] = {0x01, 0x23, 0x45, 0x67,
                        0x89, 0xAB, 0xCD, 0xEF};
            state = radio.transmit(byteArr, 1);
            
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
        
        if (byteArr[0] == 0x02) break;

        Serial.println(F("Received someone else LoRa packet"));
    }

    if (state == RADIOLIB_ERR_NONE) {
        // packet was successfully received
        Serial.println(F("[SX1280] Received packet!"));

        // print RSSI (Received Signal Strength Indicator)
        Serial.print(F("[SX1280] RSSI:\t\t"));
        Serial.print(radio.getRSSI());
        Serial.println(F(" dBm"));

        // print SNR (Signal-to-Noise Ratio)
        Serial.print(F("[SX1280] SNR:\t\t"));
        Serial.print(radio.getSNR());
        Serial.println(F(" dB"));

        // print the Frequency Error
        // of the last received packet
        Serial.print(F("[SX1280] Frequency Error:\t"));
        FREQ_ERROR = radio.getFrequencyError();
        Serial.print(FREQ_ERROR);
        Serial.println(F(" Hz"));
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        // packet was received, but is malformed
        Serial.println(F("CRC error!"));
    } else {
        // some other error occurred
        Serial.print(F("failed, code "));
        Serial.println(state);
    }
}

std::pair<int, int> rawRng[SAMPLE_SIZE];
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
    int state = radio.readRegister(0x0964, data, 1);

    // return ( (int32_t) (~data[0] + 1) ) / 2;
    return data[0];
}

void rangingPhase() {
    int rngCounter = 0;
    int rngValid = 0;
    int rngTimedOut = 0;
    int rngFail = 0;

    //=========================================
    // GATHERING RANGING SAMPLE
    //=========================================

    Serial.println(F("Ranging ... "));

    while(rngCounter < SAMPLE_SIZE) {

        state = radio.range(true, 0x12345678/*, RNG_CALIB*/);

        if (state == RADIOLIB_ERR_NONE) {
            // Serial.println(F("success!"));
            // Serial.print(F("Raw distance:\t\t"));
            
            rawRng[rngValid].first = radio.getRangingResultRaw(); // 24 bits in 2's complement form
            rawRng[rngValid].second = rangingRSSI();

            // Serial.print(distance);
            // Serial.println(F(" meters (raw)"));

            // Serial.print(F("FEI-based calibration:\t\t"));
            // Serial.println(calibrClockDrift);
            
            // Serial.print(F("LNA gain calibration:\t\t"));
            // Serial.print(calibrLnaGain);
            // Serial.print(F("\tRSSI: "));
            // Serial.print(absRssi);
            // Serial.print(F("\tLUT: "));
            // Serial.println(RangingCorrectionSF9BW0800[absRssi]);

            // Serial.print(F("Final calibration:\t\t"));
            // Serial.println(calibrClockDrift + calibrLnaGain - distance);
            rngValid++;

        } else if (state == RADIOLIB_ERR_RANGING_TIMEOUT) {
            // Serial.println(F("timed out!"));
            rngTimedOut++;
        } else {
            // Serial.print(F("failed, code "));
            // Serial.println(state);
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

    Serial.println(F("Ranging Done! Packets:"));
    Serial.print(F("Valid:\t"));
    Serial.println(rngValid);
    Serial.print(F("TimedOut:\t"));
    Serial.println(rngTimedOut);
    Serial.print(F("Failed:\t"));
    Serial.println(rngFail);
    Serial.println(F("Post-processing ...\n"));

    // Convert the ranging LSB to distance in meter. The theoretical conversion from register value to distance [m] is given by:
    // distance [m] = ( complement2( register ) * 150 ) / ( 2^12 * bandwidth[MHz] ) ). The API provide BW in [Hz] so the implemented
    // formula is complement2( register ) / bandwidth[Hz] * A, where A = 150 / (2^12 / 1e6) = 36621.09 
    for (int i = 0; i < rngValid; i++) {
        distance[i] = ( (float) (~rawRng[i].first + 1) ) / ( (float) BW ) * 36621.09375;
    }

    for (int i = rngValid - 1; i >= 0; i++) {
        for (int j = 0; j < i; j++) {
            if (distance[j] > distance[j + 1]) {
                float temp = distance[j];
                distance[j] = distance[j + 1];
                distance[j + 1] = temp;
            }
        }
    }

    for (int i = 0; i < rngValid; i++) {
        calibClkDrift[i] = distance[i] - (RNG_FGRAD[BW_ID][SF - 5] * FREQ_ERROR / 1000.0);

        if (rawRng[i].second >= 140 || rawRng[i].second < 0) {
            Serial.print(F("WARNING, Ranging RSSI should be in range [0, 140]: "));
            Serial.println(rawRng[i].second);

            while(1) {
                delay(10);
            }
        }
        calibLNAGain[i] = distance[i] - RangingCorrectionSF9BW0800[rawRng[i].second];

        calibFinal[i] = calibClkDrift[i] + calibLNAGain[i] - distance[i]; 
    }

    // RSSI-based correction for LNA Gain is not uniform
    // Hence median value might not be true for both calibLNAGain and calibFinal
    for (int i = rngValid - 1; i >= 0; i++) {
        for (int j = 0; j < i; j++) {
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
    
    radio.setFrequency( ((float)BW) / 1000.0 );
    radio.setSpreadingFactor( SF );    
}

void loop() {
    communicationPhase();
    rangingPhase();
}
