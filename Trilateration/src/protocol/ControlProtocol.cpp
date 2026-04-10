#include "ControlProtocol.h"

bool packControlPacket(const ControlPacket& in, uint8_t* raw) {
    if (!raw) return false;
    raw[0] = (static_cast<uint8_t>(in.type) << 6) | (in.bwId << 4) | in.sf;
    raw[1] = in.sweepCount << 4;
    raw[2] = in.srcId;
    raw[3] = in.dstId;
    return true;
}

bool unpackControlPacket(ControlPacket& out, const uint8_t* raw) {
    if (!raw) return false;
    out.type = static_cast<PacketType>(raw[0] >> 6);
    out.bwId = (raw[0] >> 4) & 0x3;
    out.sf = raw[0] & 0xF;
    out.sweepCount = raw[1] >> 4;
    out.srcId = raw[2];
    out.dstId = raw[3];

    if (static_cast<uint8_t>(out.type) >= 2) return false;
    if (out.bwId == 3) return false;        // BW[3] = 203.125 KHz, N/A for the Project
    if (out.sf < 5 || out.sf > 10) return false;
    if (out.sweepCount > 6) return false;
    return true;
}
