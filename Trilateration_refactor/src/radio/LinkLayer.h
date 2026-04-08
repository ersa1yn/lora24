#pragma once
#include <Arduino.h>
#include <RadioLib.h>
#include "../protocol/ControlProtocol.h"
#include "../config/ProjectConfig.h"

enum class LinkResult : uint8_t {
    Ok, Timeout, CrcError, RadioError, WrongPacketLength,
    WrongContent, WrongPeer, WrongDestination, WrongType
};

struct LinkContext {
    SX1280& radio;
    uint8_t selfId;
    uint8_t ledPin;
    bool useLed;
    bool verbose;
};

LinkResult sendAndAwaitAck(LinkContext& ctx, 
    const ControlPacket& tx, uint32_t timeoutMs = 5000);
LinkResult awaitAndSendAck(LinkContext& ctx, ControlPacket& rx, 
    PacketType expectedType, uint8_t expectedFrom = 0, uint32_t timeoutMs = 5000);
void printLinkResult(LinkResult res);
