#pragma once
#include "NodeBase.h"
#include <Preferences.h>
#include "../protocol/ControlProtocol.h"

class Anchor : public NodeBase {
public:
    Anchor(SX1280& radio, const NodeConfig& cfg, uint8_t ledPin = LED_BUILTIN);

    void begin() override;
    void loop() override;

private:
    Preferences prefs_;
    uint32_t runId_ = 0;
    float freqError_ = 0.0f;
    bool started_ = false;

    uint32_t rawRng_[DEFAULT_SZ] = {0};
    uint8_t rngRssi_[DEFAULT_SZ] = {0};
    uint16_t rngValid_ = 0, rngTimeout_ = 0, rngFail_ = 0;

    void initRunId();
    void advanceRunId();

    void waitTurnPhase();
    void configureSlavePhase(ControlPacket rx);
    uint8_t rangingRssi();
    void rangingPhase(ControlPacket rx);
    void passTurnPhase(ControlPacket rx);
    bool dataSendPhase(ControlPacket rx);
    void connectWiFiIfNeeded();
    static bool appendJsonf(char* out, size_t cap, size_t* pos, const char* fmt, ...);
    bool buildRangingJsonBuffer(char* out, size_t cap, 
        size_t* outLen, uint8_t bwId, uint8_t sf, uint8_t sweepCount);
};
