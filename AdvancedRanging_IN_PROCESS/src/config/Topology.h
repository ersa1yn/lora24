#pragma once
#include <Arduino.h>

namespace Topology {
    inline constexpr uint8_t anchorNum = 3;
    inline constexpr uint8_t anchorIds[anchorNum] = {0x01, 0x02, 0x03};

    inline int indexOf(uint8_t id) {
        for (int i = 0; i < anchorNum; ++i) if (anchorIds[i] == id) return i;
        return -1;
    }

    inline uint8_t parentOf(uint8_t id) {
        int i = indexOf(id);
        return (i < 0) ? 0 : anchorIds[(i + anchorNum - 1) % anchorNum];
    }

    inline uint8_t childOf(uint8_t id) {
        int i = indexOf(id);
        return (i < 0) ? 0 : anchorIds[(i + 1) % anchorNum];
    }
}
