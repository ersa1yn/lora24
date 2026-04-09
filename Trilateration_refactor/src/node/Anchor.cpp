#include <WiFi.h>
#include <HTTPClient.h>

#include "Anchor.h"
#include "../config/ProjectConfig.h"
#include "../config/Topology.h"
#include "../calibration/calibration.h"

#define RADIOLIB_LOW_LEVEL (1)

Anchor::Anchor(SX1280& radio, const NodeConfig& cfg, uint8_t ledPin)
    : NodeBase(radio, cfg, ledPin, true) {}

void connectWiFiIfNeeded() {
    if (WiFi.status() == WL_CONNECTED) return;
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
    }
}

void Anchor::begin() {
    Serial.begin(9600);
    while (!Serial) {}

    connectWiFiIfNeeded();

    Serial.print(F("Anchor init... "));
    if (!initRadio()) {
        Serial.println(F("radio fail"));
        while (true) delay(3000);
    }
    Serial.println(F("ok"));

    initRunId();
}

void Anchor::loop() {
    if (cfg_.mainMaster && !started_) {
        ControlPacket startPkt{PacketType::MasterDone, 0, 5, DEFAULT_SC, 
            Topology::parentOf(cfg_.deviceId), cfg_.deviceId};
        configureSlavePhase(startPkt);
        started_ = true;
    }

    waitTurnPhase();
}

void Anchor::initRunId() {
    if (!prefs_.begin("ranging", false)) {
        Serial.println(F("NVS open failed"));
        while (true) delay(1000);
    }
    runId_ = prefs_.getULong("run_id", 1);
    if (runId_ == 0) runId_ = 1;
    prefs_.putULong("run_id", runId_);
}

void Anchor::advanceRunId() {
    ++runId_;
    prefs_.putULong("run_id", runId_);
}

void Anchor::waitTurnPhase() {
    radio_.setBandwidth(DEFAULT_BW);
    radio_.setSpreadingFactor(DEFAULT_SF);
    radio_.setFrequency(DEFAULT_RF);

    ControlPacket rx{};
    while (true) {
        LinkResult r = awaitAndSendAck(link_, rx, 
            PacketType::MasterDone, Topology::parentOf(cfg_.deviceId));
        logLink(F("WaitTurn"), r);
        if (r == LinkResult::Ok) break;
    }

    if (cfg_.mainMaster) {
        if (rx.sf == 10) {
            if (rx.bwId == 2) { 
                rx.bwId = 0; rx.sf = 5; 
                advanceRunId(); 
            } else { 
                rx.bwId++; rx.sf = 5;
            } 
        } else {
            rx.sf++;
        }
    }

    configureSlavePhase(rx);
}

void Anchor::configureSlavePhase(ControlPacket rx) {
    ControlPacket tx = rx;
    tx.type = PacketType::RangingRequest;
    tx.srcId = cfg_.deviceId;
    tx.dstId = cfg_.targetId;

    while (true) {
        LinkResult r = sendAndAwaitAck(link_, tx);
        logLink(F("ConfigureSlave"), r);
        if (r == LinkResult::Ok) break;
    }

    freqError_ = radio_.getFrequencyError();
    rangingPhase(rx);
}

uint8_t Anchor::rangingRssi() {
    uint8_t data[1] = { 0 };
    int state = radio_.readRegister(RADIOLIB_SX128X_REG_RANGING_RSSI, data, 1);  // 0x0964

    if (state != RADIOLIB_ERR_NONE) 
        Serial.println(F("Could not read rngRssi from the register"));

    return data[0];
}

void Anchor::rangingPhase(ControlPacket rx) {
    rngValid_ = 0;
    rngTimeout_ = 0;
    rngFail_ = 0;

    for (int i = 0; i < rx.sweepCount * TOTAL_CHNL; i++) {
        rawRng_[i] = 0;
        rngRssi_[i] = 0;
    }

    if (cfg_.verbose) {
        Serial.println(F("Ranging ... "));
    }

    radio_.setBandwidth(BW[rx.bwId] / 1000.0f);
    radio_.setSpreadingFactor(rx.sf);

    for (int i = 0; i < rx.sweepCount; i++) {
        int chnCounter = 0;
        while (chnCounter < TOTAL_CHNL) {
            radio_.setFrequency(CHANNELS[chnCounter++]);

            digitalWrite(LED_BUILTIN, HIGH);
            int state = radio_.range(true, cfg_.targetId /*, RNG_CALIB*/);
            digitalWrite(LED_BUILTIN, LOW);

            if (state == RADIOLIB_ERR_NONE) {
                rawRng_[rngValid_] = radio_.getRangingResultRaw();
                rngRssi_[rngValid_] = rangingRssi();

                rngValid_++;
            } else if (state == RADIOLIB_ERR_RANGING_TIMEOUT) {
                rngTimeout_++;
            } else {
                rngFail_++;
            }

            if (cfg_.verbose) {
                Serial.print(" ");
                Serial.print(chnCounter);
            }
        }
    }

    if (cfg_.verbose) {
        Serial.print(F("Ranging Done! BandWidth: "));
        Serial.print(BW[rx.bwId]);
        Serial.print(F(" ; SF: "));
        Serial.print(rx.sf);
        Serial.println(F(" ;\nPackets stats:"));
        Serial.print(F("Valid:\t"));
        Serial.println(rngValid_);
        Serial.print(F("TimedOut:\t"));
        Serial.println(rngTimeout_);
        Serial.print(F("Failed:\t"));
        Serial.println(rngFail_);
    }

    passTurnPhase(rx);
}

void Anchor::passTurnPhase(ControlPacket rx) {
    ControlPacket tx = rx;
    tx.type = PacketType::MasterDone;
    tx.srcId = cfg_.deviceId;
    tx.dstId = Topology::childOf(cfg_.deviceId);

    while (true) {
        LinkResult r = sendAndAwaitAck(link_, tx);
        logLink(F("PassTurn"), r);
        if (r == LinkResult::Ok) break;
    }

    while (!dataSendPhase(rx)) {}
}

static bool appendJsonf(char* out, size_t cap, size_t* pos, const char* fmt, ...) {
    if (!out || !pos || *pos >= cap) return false;

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(out + *pos, cap - *pos, fmt, args);
    va_end(args);

    if (n < 0) return false;
    size_t written = (size_t)n;
    if (written >= (cap - *pos)) {
        *pos = cap;
        return false;
    }

    *pos += written;
    return true;
}

bool Anchor::buildRangingJsonBuffer(char* out, size_t cap, 
    size_t* outLen, uint8_t bwId, uint8_t sf, uint8_t sweepCount) {
    if (!out || !outLen || cap == 0) return false;

    size_t pos = 0;
    uint8_t bwSf = (uint8_t)(((bwId & 0x0F) << 4) | (sf & 0x0F));

    if (!appendJsonf(out, cap, &pos,
                    "{\"run_id\":%lu,\"device_id\":%u,\"target_id\":%u,"
                    "\"bw_sf\":%u,\"freqError\":%.6f,\"sweepCount\":%u,"
                    "\"valid\":%u,\"timeout\":%u,\"fail\":%u,\"raw_rng\":[",
                    (unsigned long) runId_,
                    (unsigned)cfg_.deviceId,
                    (unsigned)cfg_.targetId,
                    (unsigned)bwSf,
                    (double)freqError_,
                    (unsigned)sweepCount,
                    (unsigned)rngValid_,
                    (unsigned)rngTimeout_,
                    (unsigned)rngFail_)) return false;

    for (uint16_t i = 0; i < rngValid_; i++) {
        if (!appendJsonf(out, cap, &pos, "%s%lu",
                        (i ? "," : ""), (unsigned long)rawRng_[i])) return false;
    }

    if (!appendJsonf(out, cap, &pos, "],\"rng_rssi\":[")) return false;

    for (uint16_t i = 0; i < rngValid_; i++) {
        if (!appendJsonf(out, cap, &pos, "%s%u",
                        (i ? "," : ""), (unsigned)rngRssi_[i])) return false;
    }

    if (!appendJsonf(out, cap, &pos, "]}")) return false;

    *outLen = pos;
    return true;
}

bool Anchor::dataSendPhase(ControlPacket rx) {
    connectWiFiIfNeeded();

    char payload[6144];
    size_t payloadLen = 0;

    if (!buildRangingJsonBuffer(payload, sizeof(payload), 
        &payloadLen, rx.bwId, rx.sf, rx.sweepCount)) {

        Serial.println(F("JSON build failed: buffer too small"));
        return false;
    }

    HTTPClient http;
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");

    int code = http.POST((uint8_t*)payload, payloadLen);
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
    return true;
}
