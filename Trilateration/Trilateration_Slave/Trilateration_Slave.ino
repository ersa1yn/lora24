/*
File Description
*/

const bool VERBOSE = true;

const uint8_t DEVICE_ID = 0xff; // slave counted backwards
const char*   DEVICE_NAME = "feather-slave-ff"; // Unique per device

#include <RadioLib.h>
SX1280 radio = new Module(33, 26, 27, 25);

#include <Utilities.h>

#include "../rangingCorrection.h"

int state;

int rngValid;
int rngTimeout;
int rngFail;

bool receivedFlag = false;
void setFlag(void) { receivedFlag = true; }

LinkContext linkCtx = { radio, receivedFlag, DEVICE_ID, LED_BUILTIN, true };

void getConfigurationPhase() {
    radio.setBandwidth(DEFAULT_BW);
    radio.setSpreadingFactor(DEFAULT_SF);
    radio.setFrequency(DEFAULT_RF);

    ControlPacket rx;

    while (true) {
        LinkResult temp = awaitAndSendAck(linkCtx, rx, PacketType::RangingRequest);
        
        // Only main-master is connected to laptop, i.e. Arduino IDE's Serial Output
        if (VERBOSE) {    
            Serial.print(F("Get Configuration: "));
            printLinkResult(temp);
        }

        if (temp == LinkResult::Ok) break;
    }
    
    if (VERBOSE) {
        Serial.print(F("Received packet from: 0x"));
        char buffer[10];
        sprintf(buffer, "%02x", rx.srcId);
        Serial.println(buffer);

        Serial.print(F("RSSI:\t\t"));
        Serial.print(radio.getRSSI());
        Serial.println(F(" dBm"));

        Serial.print(F("SNR:\t\t"));
        Serial.print(radio.getSNR());
        Serial.println(F(" dB"));

        Serial.print(F("Frequency Error:\t"));
        Serial.print(radio.getFrequencyError());
        Serial.println(F(" Hz"));
    }

    rangingPhase(rx);
}

void rangingPhase(ControlPacket rx) {
    rngValid = 0;
    rngTimeout = 0;
    rngFail = 0;

    if (rx.sweepCount > 6) rx.sweepCount = 6;

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
            state = radio.range(false, DEVICE_ID/*, RNG_CALIB*/);
            digitalWrite(LED_BUILTIN, LOW);

            if (state == RADIOLIB_ERR_NONE) {
                rngValid++;
            } else if (state == RADIOLIB_ERR_RANGING_TIMEOUT) {
                rngTimeout++;
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
        Serial.print(F("Timeout:\t"));
        Serial.println(rngTimeout);
        Serial.print(F("Failed:\t"));
        Serial.println(rngFail);
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

}

void loop() {
    getConfigurationPhase();
}
