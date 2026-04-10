#include <RadioLib.h>
#include <Trilateration.h>

const bool    MAIN_MASTER = false;
const bool    VERBOSE = true;
const uint8_t DEVICE_ID = 0x02;
const uint8_t TARGET_ID = 0xff;
const char*   DEVICE_NAME = "feather-master-02";

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
