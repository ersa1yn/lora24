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
#define SF 10
#define SAMPLE_SIZE 100

SX1280 radio = new Module(33, 26, 27, 25);

int state;
bool receivedFlag = false;

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

    if (byteArr[0] == 0x01) break;

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
    float FREQ_ERROR = radio.getFrequencyError();
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


  //=========================================
  //  SLAVE TRANSMITTING LORA RESPONSE
  //=========================================

  Serial.println(F("[COMM] Sent LoRa Packet Response"));

  byte byteArr[8] = { 0x02, 0x23, 0x45, 0x67,
                      0x89, 0xAB, 0xCD, 0xEF };
  state = radio.transmit(byteArr, 1);
}

int rngCounter;
int rngValid;
int rngTimedOut;
int rngFail;

void rangingPhase() {
    int rngCounter = 0;
    int rngValid = 0;
    int rngTimedOut = 0;
    int rngFail = 0;

    //=========================================
    // GATHERING RANGING SAMPLE
    //=========================================

    Serial.println(F("Ranging ... "));

    while (rngCounter < SAMPLE_SIZE) {

        state = radio.range(false, 0x12345678 /*, RNG_CALIB*/);

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

  radio.setFrequency(((float)BW) / 1000.0);
  radio.setSpreadingFactor(SF);
}

void loop() {
  communicationPhase();
  rangingPhase();
}
