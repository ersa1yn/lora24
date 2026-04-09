#include <RadioLib.h>
#include "node/Target.h"
#include "node/NodeConfig.h"

const bool    VERBOSE = true;
const uint8_t DEVICE_ID = 0xff;
const char*   DEVICE_NAME = "feather-slave-ff";

SX1280 radio = new Module(33, 26, 27, 25);

NodeConfig cfg{
  .role = NodeRole::Target,
  .mainMaster = false,
  .verbose = VERBOSE,
  .deviceId = DEVICE_ID,
  .targetId = 0x00,
  .deviceName = DEVICE_NAME
};

Target node(radio, cfg);

void setup() { node.begin(); }
void loop() { node.loop(); }
