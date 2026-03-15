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
#include <utility>
#include <cstdint>

#define BW 1625000
#define BW_ID 2
#define SF 6
#define SAMPLE_SIZE 100

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

int state;
bool receivedFlag = false;
float FREQ_ERROR;

void setFlag(void) {
    receivedFlag = true;
}

void communicationPhase() {
    //=========================================
    //  SLAVE LISTENING LORA PACKET
    //=========================================

    // set the function that will be called
    // when new packet is received
    radio.setPacketReceivedAction(setFlag);
    
    Serial.print(F("[COMM] Waiting LoRa Packet ... "));
    state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success!"));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true) { delay(10); }
    }

    while(!receivedFlag) {
        // reset flag
        receivedFlag = false;

        String str;
        state = radio.readData(str);

        // you can also read received data as byte array
        /*
        byte byteArr[8];
        int numBytes = radio.getPacketLength();
        int state = radio.readData(byteArr, numBytes);
        */

        if (state == RADIOLIB_ERR_NONE) {
            // packet was successfully received
            Serial.println(F("[SX1280] Received packet!"));

            // print data of the packet
            Serial.print(F("[SX1280] Data:\t\t"));
            Serial.println(str);

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

    //=========================================
    //  SLAVE TRANSMITTING LORA RESPONSE
    //=========================================

    Serial.println(F("[COMM] Sent LoRa Packet Response"));
    String str = "Exchange LoRa packet";
    state = radio.transmit(str);
}

int rngCounter;
int rngValid;
int rngTimedOut;
int rngFail;

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
    res = res / (float) total; // variance
    res = sqrt(res);

    return res;
}

int32_t rangingRSSI() {
  uint8_t data[1] = { 0 };
  int state = radio.readRegister(0x0964, data, 1);

  return ( (int32_t) (~data[0] + 1) ) / 2;
}

void rangingPhase() {

    //=========================================
    // GATHERING RANGING SAMPLE
    //=========================================

    Serial.println(F("Ranging ... "));

    while(rngCounter < SAMPLE_SIZE) {

        state = radio.range(false, 0x12345678/*, RNG_CALIB*/);

        if (state == RADIOLIB_ERR_NONE) {
            rngValid++;

        } else if (state == RADIOLIB_ERR_RANGING_TIMEOUT) {
            // Serial.println(F("timed out!"));
            rngTimedOut++;
        } else {
            // Serial.print(F("failed, code "));
            // Serial.println(state);
            rngFail++;
        }

        rngCounter++;
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
