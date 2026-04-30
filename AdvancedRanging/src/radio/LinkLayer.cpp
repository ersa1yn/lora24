#include "LinkLayer.h"

static inline void setLed(LinkContext& ctx, bool on) {
    if (ctx.useLed) digitalWrite(ctx.ledPin, on ? HIGH : LOW);
}

LinkResult awaitAndSendAck(LinkContext& ctx, ControlPacket& rx, PacketType expectedType,
    uint8_t expectedFrom, uint32_t timeout) {

    // ==================================
    // Await packet
    // ==================================
    uint8_t rxRaw[PACKET_SZ];
    setLed(ctx, true);
    int state = ctx.radio.receive(rxRaw, PACKET_SZ, timeout);
    setLed(ctx, false);
    
    if (state == RADIOLIB_ERR_RX_TIMEOUT) return LinkResult::Timeout;
    if (state == RADIOLIB_ERR_CRC_MISMATCH) return LinkResult::CrcError;
    if (state != RADIOLIB_ERR_NONE) return LinkResult::RadioError;
    
    // ==================================
    // Check received packet
    // ==================================

    size_t len = ctx.radio.getPacketLength();
    if (len != PACKET_SZ) return LinkResult::WrongPacketLength;
    if (!unpackControlPacket(rx, rxRaw))   return LinkResult::WrongContent;
    
    // ==================================
    // Print packet contents
    // ==================================

    if (ctx.verbose) {
        char buffer[100];
        sprintf(buffer, "type:%hhu; bwId:%hhu; sf:%hhu, sweep:%hhu, srcId:%hhu; dstId:%hhu",
            static_cast<uint8_t>(rx.type), rx.bwId, rx.sf, 
            rx.sweepCount, rx.srcId, rx.dstId);
        Serial.println(F("Received packet! Contents:"));
        Serial.println(buffer);
    }

    if (rx.dstId != ctx.selfId)          return LinkResult::WrongDestination;
    if (expectedFrom > 0 &&
        rx.srcId != expectedFrom)        return LinkResult::WrongPeer;
    if (rx.type != expectedType)         return LinkResult::WrongType;

    // ==================================
    // Send ACK if it is expected type
    // ==================================

    ControlPacket ack = {expectedType, rx.bwId, rx.sf, rx.sweepCount, ctx.selfId, rx.srcId};
    uint8_t ackRaw[PACKET_SZ];
    packControlPacket(ack, ackRaw);
    state = ctx.radio.transmit(ackRaw, PACKET_SZ);
    if (state != RADIOLIB_ERR_NONE) return LinkResult::RadioError;

    return LinkResult::Ok;
}

LinkResult sendAndAwaitAck(LinkContext& ctx, const ControlPacket& tx, uint32_t timeout) {
    // ==================================
    // Send Packet
    // ==================================

    uint8_t txRaw[PACKET_SZ];
    packControlPacket(tx, txRaw);
    int state = ctx.radio.transmit(txRaw, PACKET_SZ);
    if (state != RADIOLIB_ERR_NONE) return LinkResult::RadioError;

    // ==================================
    // Await Acknowledgement
    // ==================================

    uint8_t ackRaw[PACKET_SZ];

    setLed(ctx, true);
    state = ctx.radio.receive(ackRaw, PACKET_SZ, timeout);
    setLed(ctx, false);

    if (state == RADIOLIB_ERR_RX_TIMEOUT) return LinkResult::Timeout;
    if (state == RADIOLIB_ERR_CRC_MISMATCH) return LinkResult::CrcError;
    if (state != RADIOLIB_ERR_NONE)  return LinkResult::RadioError;

    // ==================================
    // Check received packet
    // ==================================

    ControlPacket ack;    
    size_t len = ctx.radio.getPacketLength();
    if (len != PACKET_SZ) return LinkResult::WrongPacketLength;
    if (!unpackControlPacket(ack, ackRaw)) return LinkResult::WrongContent;
   
    // ==================================
    // Print packet contents
    // ==================================

    if (ctx.verbose) {
        char buffer[100];
        sprintf(buffer, "type:%hhu; bwId:%hhu; sf:%hhu, sweep:%hhu, srcId:%hhu; dstId:%hhu",
            static_cast<uint8_t>(ack.type), ack.bwId, ack.sf, 
            ack.sweepCount, ack.srcId, ack.dstId);
        Serial.println(F("Received packet! Contents:"));
        Serial.println(buffer);
    }

    if (ack.dstId != ctx.selfId) return LinkResult::WrongDestination;
    if (ack.srcId != tx.dstId)   return LinkResult::WrongPeer;
    if (ack.type != tx.type)     return LinkResult::WrongType;

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
        default:
            Serial.println(F("Undocumented Link Result"));
            break;
    }
}
