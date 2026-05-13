/*

| # | contents |
+ - + -------------- + ----------- + -------- +
| 0 | pkt type [0:1] | BW_ID [2:3] | SF [4:7] |
+ - + -------------- + ----------- + -------- +
+ - + ----------------- + ---------------- +
| 1 | sweepCount [8:11] | reserved [12:15] |
+ - + ----------------- + ---------------- +
+ - + ---------------- + 
| 2 | src ID LSB [0:7] | 
+ - + ---------------- + 
+ - + ---------------- +
| 3 | dst ID LSB [0:7] |
+ - + ---------------- +

Limited to 4 bytes for now

+ - + ------------- +
| 4 | CHNL ID [0:7] | <= RESERVED; NOT IN USE
+ - + ------------- +
+ - + ---------------- + ---------------- +
| 5 | src ID MSB [0:3] | dst ID MSB [4:7] | <= RESERVED; NOT IN USE
+ - + ---------------- + ---------------- +

ptk type:
0 - ranging request
1 - master ping; "I am done, your turn"
2, 3 - reserved

BW_ID:
0 - 406250 Hz 
1 - 812500 Hz
2 - 1625000 Hz
3 - 203125 Hz (N/A for this project)

SF: 5 - 12; 0-4 & 13-15 Not supported by Hardware

CHN_ID: 40 channels corresponding to BLE chnls; check "rangingCorrection.h"

src/dst ID: currently only 8 lower bits are used, 256 devices; 
            extendable to cover 4k devices

sweepCount: number of ranging done, multiply by 40

*/

#pragma once
#include <Arduino.h>

enum class PacketType : uint8_t { RangingRequest = 0, MasterDone = 1 };

struct ControlPacket {
    PacketType type;
    uint8_t bwId;
    uint8_t sf;
    uint8_t sweepCount;
    uint8_t srcId;
    uint8_t dstId;
};

bool packControlPacket(const ControlPacket& in, uint8_t* raw);
bool unpackControlPacket(ControlPacket& out, const uint8_t* raw);
