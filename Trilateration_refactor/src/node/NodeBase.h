#pragma once
#include <Arduino.h>
#include <RadioLib.h>
#include "NodeConfig.h"
#include "../radio/LinkLayer.h"

class NodeBase {
public:
    NodeBase(SX1280& radio, const NodeConfig& cfg, uint8_t ledPin = LED_BUILTIN, bool useLed = true)
        : radio_(radio), cfg_(cfg), ledPin_(ledPin), useLed_(useLed),
        link_{radio_, cfg_.deviceId, ledPin_, useLed_, cfg_.verbose} {}

    virtual ~NodeBase() = default;

    virtual void begin() = 0;
    virtual void loop() = 0;

protected:
    SX1280& radio_;
    NodeConfig cfg_;
    uint8_t ledPin_;
    bool useLed_;
    LinkContext link_;

    bool initRadio() {
        pinMode(ledPin_, OUTPUT);
        int st = radio_.begin();
        return (st == RADIOLIB_ERR_NONE);
    }

    void logLink(const __FlashStringHelper* phase, LinkResult r) {
        if (!cfg_.verbose) return;
        Serial.print(phase);
        Serial.print(F(": "));
        printLinkResult(r);
    }
};
