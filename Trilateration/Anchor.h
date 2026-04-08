
#define RADIOLIB_LOW_LEVEL (1)
#include <RadioLib.h>
// check Notion for mapping
SX1280 radio = new Module(33, 26, 27, 25); 

#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>

#include <Utilities.h>

#include <stdarg.h>
#include <stdio.h>

#include "rangingCorrection.h"

extern const bool    MAIN_MASTER = true;
extern const bool    VERBOSE = true;
extern const uint8_t DEVICE_ID = 0x01; // master counted forwards
extern const uint8_t TARGET_ID = 0xff; // slave counted backwards
extern const char*   DEVICE_NAME = "feather-master-01"; // Unique per device

void advanceRunId();

void waitTurnPhase();

void configureSlavePhase(ControlPacket rx);

uint8_t rangingRSSI();

void rangingPhase(ControlPacket rx);

void passTurnPhase(ControlPacket rx);

void connectWiFiIfNeeded();

static bool appendJsonf(char* out, size_t cap, size_t* pos, const char* fmt, ...);

bool buildRangingJsonBuffer( char* out, size_t cap, size_t* outLen, 
    uint8_t bwId, uint8_t sf, uint8_t sweepCount);

bool dataSendPhase(ControlPacket rx);

void setup();

void loop();
