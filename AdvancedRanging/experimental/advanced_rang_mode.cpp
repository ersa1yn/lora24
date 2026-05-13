#define RADIOLIB_LOW_LEVEL 1

#define RADIOLIB_SX128X_PACKET_TYPE_

#include <RadioLib.h>

// Helper command to enable/disable Advanced Ranging (Opcode 0x9A)
void setAdvancedRangingState(SX1280& radio, bool enable) {
  uint8_t data = enable ? 0x01 : 0x00;
  
  // Access the low-level Module to send the SPI command directly
  radio.getMod()->SPIwriteStream(RADIOLIB_SX128X_CMD_SET_ADVANCED_RANGING , &data, 1);
}

// 1. Start Advanced Ranging
int16_t startAdvancedRanging(SX1280& radio) {
  // Ensure we are in Standby RC
  int16_t state = radio.standby();
  if (state != RADIOLIB_ERR_NONE) return state;

  // Set the role to Slave (0x00)
  state = radio.setRangingRole(RADIOLIB_SX128X_RANGING_ROLE_SLAVE);
  if (state != RADIOLIB_ERR_NONE) return state;

  // Enable Advanced Ranging Mode
  setAdvancedRangingState(radio, true);

  // Set IRQ mask to 0x8000 (AdvancedRangingDone)
  // Maps AdvancedRangingDone to DIO1
  uint32_t irqMask = RADIOLIB_SX128X_IRQ_ADVANCED_RANGING_DONE;
  uint32_t irqDio1 = RADIOLIB_SX128X_IRQ_ADVANCED_RANGING_DONE;
  state = radio.setDioIrqParams(irqMask, irqDio1);
  if (state != RADIOLIB_ERR_NONE) return state;

  // Set to Continuous RX Mode
  return radio.setRx(RADIOLIB_SX128X_RX_TIMEOUT_INF);
}

// 2. Read the Overheard Ranging Address
uint32_t getAdvancedRangingAddress(SX1280& radio) {
  // set mode to standby XOSC
  int16_t state = standby(RADIOLIB_SX128X_STANDBY_XOSC);
  if (state != RADIOLIB_ERR_NONE) return state;

  // enable clock
  uint8_t data[3] = { 0 };
  state = readRegister(RADIOLIB_SX128X_REG_RANGING_LORA_CLOCK_ENABLE, data, 1);
  RADIOLIB_ASSERT(state);

  data[0] |= (1 << 1);
  state = writeRegister(RADIOLIB_SX128X_REG_RANGING_LORA_CLOCK_ENABLE, data, 1);
  RADIOLIB_ASSERT(state);

  uint32_t address = 0;
  uint8_t val = 0;

  // Read first 16 bits
  radio.readRegister(0x927, &val, 1);
  val = (val & 0xFC) | 0x00;
  radio.writeRegister(0x927, &val, 1);
  
  uint8_t b0, b1;
  radio.readRegister(0x960, &b0, 1);
  radio.readRegister(0x95F, &b1, 1);
  address |= b0;
  address |= (b1 << 8);

  // Read next 16 bits
  radio.readRegister(0x927, &val, 1);
  val = (val & 0xFC) | 0x01;
  radio.writeRegister(0x927, &val, 1);
  
  uint8_t b2, b3;
  radio.readRegister(0x960, &b2, 1);
  radio.readRegister(0x95F, &b3, 1);
  address |= ((uint32_t)b2 << 16);
  address |= ((uint32_t)b3 << 24);

  return address;
}

// 3. Stop Advanced Ranging
int16_t stopAdvancedRanging(SX1280& radio) {
  int16_t state = radio.standby();
  setAdvancedRangingState(radio, false);
  return state;
}