#pragma once

#include <Arduino.h>
#include <RadioLib.h>

#include <cstdint>

// ======== CONFIG ========

constexpr int     PACKET_SZ = 4;
constexpr int     TOTAL_CHNL = 40;
constexpr float   DEFAULT_BW = 812.5f;
constexpr uint8_t DEFAULT_SF = 7;
constexpr float   DEFAULT_RF = 2400.0f;
constexpr int     DEFAULT_SZ = 256;
constexpr uint8_t DEFAULT_SC = 3;  // sweep count

extern const int BW[4];

extern const char WIFI_SSID[];
extern const char WIFI_PASS[];
// Laptop IP on WiFi hotspot:
extern const char SERVER_URL[];

// anc1 => anc2 => anc3 => anc1; id=0 means "accept from any peer"
extern const uint8_t ANCHOR_NUM;
extern const uint8_t ANCHOR_IDS[];

enum class PacketType : uint8_t { RangingRequest = 0, MasterDone = 1 };

struct ControlPacket {
    PacketType type;
    uint8_t bwId;
    uint8_t sf;
    uint8_t sweepCount;
    uint8_t srcId;
    uint8_t dstId;
};

enum class Phase : uint8_t { WaitTurn, ConfigureSlave, Ranging, PassTurn, SendData };
enum class LinkResult : uint8_t {
    Ok,
    Timeout,
    CrcError,
    RadioError,
    WrongPacketLength,
    WrongContent,
    WrongPeer,
    WrongDestination,
    WrongType,
    WrongSweepCount
};

struct LinkContext {
    SX1280& radio;
    volatile bool& receivedFlag;
    uint8_t selfId;
    uint8_t ledPin;
    bool useLed;
    bool verbose;
};

// ======== CONFIG END ========

LinkResult sendAndAwaitAck(
    LinkContext& ctx,
    const ControlPacket& tx,
    uint32_t timeoutMs = 5000,
    uint16_t retries = 10
);

LinkResult awaitAndSendAck(
    LinkContext& ctx,
    ControlPacket& rx,
    PacketType expectedType,
    uint8_t expectedFrom = 0,
    uint32_t timeoutMs = 5000,
    uint16_t retries = 10
);

int indexOf(uint8_t id);

uint8_t parentOf(uint8_t id);

uint8_t childOf(uint8_t id);

void printLinkResult(LinkResult res);

void packControlPacket(const ControlPacket& in, uint8_t* raw);

bool unpackControlPacket(ControlPacket& out, const uint8_t* raw);
