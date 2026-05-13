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

// timeout takes values in microseconds in range 0x1-0xFFFF * 15.625 = [15.625:1023984.375], 
// which is ~1 sec at max; 
// due to RadioLib implementation, it is impossible for continuous Rx in blocking receive()
// TODO: implement continuous Rx through startReceive() 
LinkResult sendAndAwaitAck(LinkContext& ctx, 
    const ControlPacket& tx, uint32_t timeout = 1000000);
LinkResult awaitAndSendAck(LinkContext& ctx, ControlPacket& rx, 
    PacketType expectedType, uint8_t expectedFrom = 0, uint32_t timeout = 1000000);
void printLinkResult(LinkResult res);
