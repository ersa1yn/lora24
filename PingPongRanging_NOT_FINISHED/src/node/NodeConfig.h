#pragma once
#include <Arduino.h>

enum class NodeRole : uint8_t { Anchor, Target };

struct NodeConfig {
  NodeRole role;
  bool mainMaster;
  bool verbose;
  uint8_t deviceId;
  uint8_t targetId;
  const char* deviceName;
};
