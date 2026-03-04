#pragma once
#include <Arduino.h>
#include "protocol.h"

namespace Uart {
  void begin(uint32_t baud);
  HardwareSerial& port();
  void send(uint8_t cmd, uint8_t value);

  void setLedLevel(uint8_t level0_100);
  void setFanLevel(uint8_t level0_100);
  void setLedTimer(uint8_t sel0_4);
  void setFanTimer(uint8_t sel0_4);
  void setPrivate(uint8_t on01);
  void setLock(uint8_t on01);
  bool read(Packet &out);
}
