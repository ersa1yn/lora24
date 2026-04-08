#include <Arduino.h>
#include <RadioLib.h>
#include <algorithm>

#include "Utilities.h"

const int BW[4] = {406250, 812500, 1625000, 203125};
const char WIFI_SSID[] = "yers";
const char WIFI_PASS[] = "12345678";
const char SERVER_URL[] = "http://10.42.0.1:5000/reading";

constexpr uint8_t ANCHOR_NUM = 3;
constexpr uint8_t ANCHOR_IDS[] = {0x01, 0x02, 0x03};

static inline void setLed(LinkContext& ctx, bool on) {
    if (ctx.useLed) digitalWrite(ctx.ledPin, on ? HIGH : LOW);
}

LinkResult awaitAndSendAck(LinkContext& ctx, ControlPacket& rx, PacketType expectedType,
    uint8_t expectedFrom, uint32_t timeoutMs, uint16_t retries) {
    
    uint8_t raw[PACKET_SZ];

        digitalWrite(ctx.ledPin, on ? HIGH : LOW);

    int st = ctx.radio.receive(raw, PACKET_SZ, timeoutMs);
    if (st == RADIOLIB_ERR_RX_TIMEOUT) return LinkResult::Timeout;
    else if (st != RADIOLIB_ERR_NONE)  return LinkResult::RadioError;
    
    setLed(ctx, false);
    /* ctx.receivedFlag = false;
    int st = ctx.radio.startReceive();
    setLed(ctx, true);
    int st = ctx.radio.startReceive();
    if (st != RADIOLIB_ERR_NONE) {
        return LinkResult::RadioError;
    }
        delay(timeoutMs);
    ctx.receivedFlag = false;
    while (!ctx.receivedFlag && retries > 0) {
        delay(timeoutMs);
        retries--;
    }
    setLed(ctx, false);
        retries--;
    if (!ctx.receivedFlag) {
        return LinkResult::Timeout;
    }
    }
    setLed(ctx, false);

    if (!ctx.receivedFlag) return LinkResult::Timeout;
    */

    size_t len = ctx.radio.getPacketLength();
    if (len != PACKET_SZ) return LinkResult::WrongPacketLength;

    st = ctx.radio.readData(raw, len);
    if (st == RADIOLIB_ERR_CRC_MISMATCH) return LinkResult::CrcError;
    if (st != RADIOLIB_ERR_NONE)         return LinkResult::RadioError;

    if (!unpackControlPacket(rx, raw))   return LinkResult::WrongContent;
    
    if (VERBOSE) {
        char buffer[100];
        sprintf(buffer, "type:%hhu; bwId:%hhu; sf:%hhu, sweep:%hhu, srcId:%hhu; dstId:%hhu",
            static_cast<uint8_t>(rx.type), rx.bwId, rx.sf, rx.sweepCount, rx.srcId, rx.dstId);
        Serial.println(F("Received packet! Contents:"));
        Serial.println(buffer);
    }

    if (rx.dstId != ctx.selfId)          return LinkResult::WrongDestination;
    if (expectedFrom > 0 &&
        rx.srcId != expectedFrom)        return LinkResult::WrongPeer;
    if (rx.type != expectedType)         return LinkResult::WrongType;
    if (rx.sweepCount > 6)               return LinkResult::WrongSweepCount;

    ControlPacket ack = {expectedType, rx.bwId, rx.sf, rx.sweepCount, ctx.selfId, rx.srcId};
    uint8_t ackRaw[PACKET_SZ];
    packControlPacket(ack, ackRaw);
    st = ctx.radio.transmit(ackRaw, PACKET_SZ);
    if (st != RADIOLIB_ERR_NONE) return LinkResult::RadioError;

    return LinkResult::Ok;
}

LinkResult sendAndAwaitAck(LinkContext& ctx, const ControlPacket& tx, 
    uint32_t timeoutMs, uint16_t retries) {
    uint8_t txRaw[PACKET_SZ];
    packControlPacket(tx, txRaw);

    int st = ctx.radio.transmit(txRaw, PACKET_SZ);
    if (st != RADIOLIB_ERR_NONE) return LinkResult::RadioError;

    delay(1);

    ctx.receivedFlag = false;
    st = ctx.radio.startReceive();
    if (st != RADIOLIB_ERR_NONE) return LinkResult::RadioError;

    setLed(ctx, true);
    while (!ctx.receivedFlag && retries > 0) {
        delay(timeoutMs);
        retries--;
    }
    setLed(ctx, false);

    if (!ctx.receivedFlag) return LinkResult::Timeout;

    ControlPacket ack;    
    uint8_t ackRaw[PACKET_SZ];
    size_t len = ctx.radio.getPacketLength();
    if (len != PACKET_SZ) return LinkResult::WrongPacketLength;

    st = ctx.radio.readData(ackRaw, len);
    if (st == RADIOLIB_ERR_CRC_MISMATCH) return LinkResult::CrcError;
    if (st != RADIOLIB_ERR_NONE)         return LinkResult::RadioError;

    if (!unpackControlPacket(ack, ackRaw)) return LinkResult::WrongContent;
   
    if (VERBOSE) {
        char buffer[100];
        sprintf(buffer, "type:%hhu; bwId:%hhu; sf:%hhu, sweep:%hhu, srcId:%hhu; dstId:%hhu",
            static_cast<uint8_t>(rx.type), rx.bwId, rx.sf, rx.sweepCount, rx.srcId, rx.dstId);
        Serial.println(F("Received packet! Contents:"));
        Serial.println(buffer);
    }

    if (ack.dstId != ctx.selfId) return LinkResult::WrongDestination;
    if (ack.srcId != tx.dstId)   return LinkResult::WrongPeer;
    if (ack.type != tx.type)     return LinkResult::WrongType;
    if (ack.sweepCount > 6)      return LinkResult::WrongSweepCount;

    return LinkResult::Ok;
}

int indexOf(uint8_t id) {
    return std::find(ANCHOR_IDS, ANCHOR_IDS + ANCHOR_NUM, id) - ANCHOR_IDS;
}

uint8_t parentOf(uint8_t id) {
    return ANCHOR_IDS[(indexOf(id) + ANCHOR_NUM - 1) % ANCHOR_NUM];
}

uint8_t childOf(uint8_t id) {
    return ANCHOR_IDS[(indexOf(id) + 1) % ANCHOR_NUM];
}

void printLinkResult(LinkResult res) {
    switch (res) {
        case LinkResult::Ok:
            Serial.println(F("Success"));
            break;
        case LinkResult::Timeout:
            Serial.println(F("Timeout"));
            break;
        case LinkResult::CrcError:
            Serial.println(F("CRC Error"));
            break;
        case LinkResult::RadioError:
            Serial.println(F("Radio Error"));
            break;
        case LinkResult::WrongPacketLength:
            Serial.println(F("Packet Length Mismatch"));
            break;
        case LinkResult::WrongContent:
            Serial.println(F("Undefined Packet; Type/BW/SF"));
            break;
        case LinkResult::WrongDestination:
            Serial.println(F("Received Someone else's Packet"));
            break;
        case LinkResult::WrongPeer:
            Serial.println(F("Expected Packet from Someone else"));
            break;
        case LinkResult::WrongType:
            Serial.println(F("Received Packet for different type"));
            break;
        case LinkResult::WrongSweepCount:
            Serial.println(F("Received Packet with too big Sweep Count"));
            break;
        default:
            Serial.println(F("Undocumented Link Result"));
            break;
    }
}

void packControlPacket(const ControlPacket& in, uint8_t* raw) {
    raw[0] = ( static_cast<uint8_t>(in.type) << 6 ) |
             ( in.bwId << 4 ) | ( in.sf );
    raw[1] = in.sweepCount << 4;
    raw[2] = in.srcId;
    raw[3] = in.dstId;
}

bool unpackControlPacket(ControlPacket& out, const uint8_t* raw) {
    out.type  = static_cast<PacketType>(raw[0] >> 6);
    out.bwId  = (raw[0] >> 4) & 0x3;
    out.sf    = raw[0] & 0xF;
    out.srcId = raw[2];
    out.dstId = raw[3];
    out.sweepCount = raw[1] >> 4;

    if (static_cast<uint8_t>(out.type) == 2 ||
        static_cast<uint8_t>(out.type) == 3 ||
        out.bwId == 3 || 
        out.sf < 5 || out.sf > 10) 
        return false;

    return true;
}