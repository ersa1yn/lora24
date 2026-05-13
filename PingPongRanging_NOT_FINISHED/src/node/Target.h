#pragma once
#include "NodeBase.h"
#include "../protocol/ControlProtocol.h"

class Target : public NodeBase {
public:
  Target(SX1280& radio, const NodeConfig& cfg, uint8_t ledPin = LED_BUILTIN);

  void begin() override;
  void loop() override;

private:
  int rngValid_ = 0;
  int rngTimeout_ = 0;
  int rngFail_ = 0;

  void getConfigurationPhase();
  void rangingPhase(ControlPacket rx);
};
