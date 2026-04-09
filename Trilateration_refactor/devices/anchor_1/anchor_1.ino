#include <RadioLib.h>
#include "node/Anchor.h"
#include "node/NodeConfig.h"

const bool    MAIN_MASTER = true;
const bool    VERBOSE = true;
const uint8_t DEVICE_ID = 0x01;
const uint8_t TARGET_ID = 0xff;
const char*   DEVICE_NAME = "feather-master-01";

SX1280 radio = new Module(33, 26, 27, 25);

NodeConfig cfg{
  .role = NodeRole::Anchor,
  .mainMaster = MAIN_MASTER,
  .verbose = VERBOSE,
  .deviceId = DEVICE_ID,
  .targetId = TARGET_ID,
  .deviceName = DEVICE_NAME
};

Anchor node(radio, cfg);

void setup() { node.begin(); }
void loop() { node.loop(); }
