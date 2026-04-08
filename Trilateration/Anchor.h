#pragma once

#include <Arduino.h>

struct AnchorConfig {
    bool mainMaster;
    bool verbose;
    uint8_t deviceId;
    uint8_t targetId;
    const char* deviceName;
};

AnchorConfig makeDefaultAnchorConfig();

void anchorSetConfig(const AnchorConfig& config);

const AnchorConfig& anchorGetConfig();

void anchorSetup();

void anchorLoop();
