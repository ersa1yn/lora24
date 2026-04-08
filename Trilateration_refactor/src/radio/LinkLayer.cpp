#include "LinkLayer.h"

LinkResult awaitAndSendAck(LinkContext& ctx, ControlPacket& rx, PacketType expectedType,
    uint8_t expectedFrom, uint32_t timeoutMs) {

    // ==================================
    // Await packet
    // ==================================
    uint8_t raw[PACKET_SZ];

    setLed(ctx, true);

    unsigned long timeout = timeoutMs * 1000;
    st = ctx.radio.receive(raw, PACKET_SZ, timeout);
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

    // ==================================
    // Check received packet
    // ==================================

    size_t len = ctx.radio.getPacketLength();
    if (len != PACKET_SZ) return LinkResult::WrongPacketLength;

    st = ctx.radio.readData(raw, len);
    if (st == RADIOLIB_ERR_CRC_MISMATCH) return LinkResult::CrcError;
    if (st != RADIOLIB_ERR_NONE)         return LinkResult::RadioError;

    if (!unpackControlPacket(rx, raw))   return LinkResult::WrongContent;
    
    // ==================================
    // Print packet contents
    // ==================================

    // if (VERBOSE) {
        char buffer[100];
        sprintf(buffer, "type:%hhu; bwId:%hhu; sf:%hhu, sweep:%hhu, srcId:%hhu; dstId:%hhu",
            static_cast<uint8_t>(rx.type), rx.bwId, rx.sf, rx.sweepCount, rx.srcId, rx.dstId);
        Serial.println(F("Received packet! Contents:"));
        Serial.println(buffer);
    // }

    if (rx.dstId != ctx.selfId)          return LinkResult::WrongDestination;
    if (expectedFrom > 0 &&
        rx.srcId != expectedFrom)        return LinkResult::WrongPeer;
    if (rx.type != expectedType)         return LinkResult::WrongType;
    if (rx.sweepCount > 6)               return LinkResult::WrongSweepCount;

    // ==================================
    // Send ACK if it is expected type
    // ==================================

    ControlPacket ack = {expectedType, rx.bwId, rx.sf, rx.sweepCount, ctx.selfId, rx.srcId};
    uint8_t ackRaw[PACKET_SZ];
    packControlPacket(ack, ackRaw);
    st = ctx.radio.transmit(ackRaw, PACKET_SZ);
    if (st != RADIOLIB_ERR_NONE) return LinkResult::RadioError;

    return LinkResult::Ok;
}

LinkResult sendAndAwaitAck(LinkContext& ctx, const ControlPacket& tx, 
    uint32_t timeoutMs) {

    // ==================================
    // Send Packet
    // ==================================
    uint8_t txRaw[PACKET_SZ];
    packControlPacket(tx, txRaw);

    st = ctx.radio.transmit(txRaw, PACKET_SZ);
    if (st != RADIOLIB_ERR_NONE) return LinkResult::RadioError;

    delay(1);

    // ==================================
    // Await Acknowledgement
    // ==================================

    uint8_t raw[PACKET_SZ];

    setLed(ctx, true);

    RadioLibTime_t timeout = timeoutMs * 1000;
    st = ctx.radio.receive(raw, PACKET_SZ, timeout);
    if (st == RADIOLIB_ERR_RX_TIMEOUT) return LinkResult::Timeout;
    else if (st != RADIOLIB_ERR_NONE)  return LinkResult::RadioError;
    
    setLed(ctx, false);

/*
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
*/

    // ==================================
    // Check received packet
    // ==================================

    ControlPacket ack;    
    uint8_t ackRaw[PACKET_SZ];
    size_t len = ctx.radio.getPacketLength();
    if (len != PACKET_SZ) return LinkResult::WrongPacketLength;

    st = ctx.radio.readData(ackRaw, len);
    if (st == RADIOLIB_ERR_CRC_MISMATCH) return LinkResult::CrcError;
    if (st != RADIOLIB_ERR_NONE)         return LinkResult::RadioError;

    if (!unpackControlPacket(ack, ackRaw)) return LinkResult::WrongContent;
   
    // ==================================
    // Print packet contents
    // ==================================
    // if (VERBOSE) {
        char buffer[100];
        sprintf(buffer, "type:%hhu; bwId:%hhu; sf:%hhu, sweep:%hhu, srcId:%hhu; dstId:%hhu",
            static_cast<uint8_t>(tx.type), tx.bwId, tx.sf, tx.sweepCount, tx.srcId, tx.dstId);
        Serial.println(F("Received packet! Contents:"));
        Serial.println(buffer);
    // }

    if (ack.dstId != ctx.selfId) return LinkResult::WrongDestination;
    if (ack.srcId != tx.dstId)   return LinkResult::WrongPeer;
    if (ack.type != tx.type)     return LinkResult::WrongType;
    if (ack.sweepCount > 6)      return LinkResult::WrongSweepCount;

    return LinkResult::Ok;
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
