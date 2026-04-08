#include "TargetNode.h"
#include "../config/ProjectConfig.h"
#include "../rangingCorrection.h"

TargetNode::TargetNode(SX1280& radio, const NodeConfig& cfg, uint8_t ledPin)
  : NodeBase(radio, cfg, ledPin, true) {}

void TargetNode::begin() {
    pinMode(ledPin_, OUTPUT);
    Serial.begin(9600);

    Serial.print(F("Initializing ... "));
    if (!initRadio()) {
        Serial.println(F("failed"));
        while (true) delay(10);
    }
    Serial.println(F("success!"));
}

void TargetNode::loop() {
    getConfigurationPhase();
}

void TargetNode::getConfigurationPhase() {
    radio_.setBandwidth(DEFAULT_BW);
    radio_.setSpreadingFactor(DEFAULT_SF);
    radio_.setFrequency(DEFAULT_RF);

    ControlPacket rx{};
    while (true) {
        LinkResult r = awaitAndSendAck(link_, rx, PacketType::RangingRequest);
        if (cfg_.verbose) {
            Serial.print(F("Get Configuration: "));
            printLinkResult(r);
        }
        if (r == LinkResult::Ok) break;
    }

    if (cfg_.verbose) {
        Serial.print(F("Received packet from: 0x"));
        char b[10];
        sprintf(b, "%02x", rx.srcId);
        Serial.println(b);

        Serial.print(F("RSSI:\t\t"));
        Serial.print(radio_.getRSSI()); Serial.println(F(" dBm"));
        Serial.print(F("SNR:\t\t"));
        Serial.print(radio_.getSNR()); Serial.println(F(" dB"));
        Serial.print(F("Frequency Error:\t"));
        Serial.print(radio_.getFrequencyError()); Serial.println(F(" Hz"));
    }

    rangingPhase(rx);
}

void TargetNode::rangingPhase(ControlPacket rx) {
    rngValid_ = rngTimeout_ = rngFail_ = 0;

    if (cfg_.verbose) Serial.println(F("Ranging ... "));

    radio_.setBandwidth(BW[rx.bwId] / 1000.0f);
    radio_.setSpreadingFactor(rx.sf);

    for (int i = 0; i < rx.sweepCount; i++) {
        int ch = 0;
        while (ch < TOTAL_CHNL) {
            radio_.setFrequency(CHANNELS[ch++]);

            digitalWrite(ledPin_, HIGH);
            int st = radio_.range(false, cfg_.deviceId);
            digitalWrite(ledPin_, LOW);

            if (st == RADIOLIB_ERR_NONE) rngValid_++;
            else if (st == RADIOLIB_ERR_RANGING_TIMEOUT) rngTimeout_++;
            else rngFail_++;

            if (cfg_.verbose) { Serial.print(" "); Serial.print(ch); }
        }
    }

    if (cfg_.verbose) {
        Serial.print(F("Ranging Done! BandWidth: ")); Serial.print(BW[rx.bwId]);
        Serial.print(F(" ; SF: ")); Serial.println(rx.sf);
        Serial.println(F("Packets stats:"));
        Serial.print(F("Valid:\t"));   Serial.println(rngValid_);
        Serial.print(F("Timeout:\t")); Serial.println(rngTimeout_);
        Serial.print(F("Failed:\t"));  Serial.println(rngFail_);
    }
}
