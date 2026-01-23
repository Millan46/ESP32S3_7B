#pragma once
#include <stdint.h>

namespace Uart {

  // --- ESP32 -> STM32 (CMD) ---
  static constexpr uint8_t CMD_SYNC      = 0x01;
  static constexpr uint8_t CMD_LED_PWM   = 0x10;
  static constexpr uint8_t CMD_FAN_PWM   = 0x20;
  static constexpr uint8_t CMD_LED_TIMER = 0x30;
  static constexpr uint8_t CMD_FAN_TIMER = 0x31;
  // --- Extra CMD ---
  static constexpr uint8_t CMD_CLEAN      = 0x40;  // value: segundos (0=default)
  static constexpr uint8_t CMD_PRIVATE    = 0x42;  // value: 0=OFF, 1=ON (GPIO privacy)

  // --- STM32 -> ESP32 (EVT) ---
  static constexpr uint8_t EVT_PIR           = 0xA0;
  static constexpr uint8_t EVT_IR            = 0xA1;
  static constexpr uint8_t EVT_LED_SYNC      = 0xB0;
  static constexpr uint8_t EVT_FAN_SYNC      = 0xB1;
  static constexpr uint8_t EVT_LED_TIMER_SYNC= 0xB2;
  static constexpr uint8_t EVT_FAN_TIMER_SYNC= 0xB3;
  static constexpr uint8_t EVT_READY         = 0x90;

  // Timer countdown
  static constexpr uint8_t EVT_LED_REMAIN    = 0xC0;
  static constexpr uint8_t EVT_FAN_REMAIN    = 0xC1;

  struct Packet {
    uint8_t cmd;
    uint8_t value;
  };

} // namespace Uart
