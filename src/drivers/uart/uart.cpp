#include "uart.h"
#include "protocol.h"

namespace Uart {

  // ===== Debug =====
  // Pon 1 solo si de verdad quieres logs (y mejor no en producción)
  #ifndef UART_DEBUG
  #define UART_DEBUG 0
  #endif

  #if UART_DEBUG
  #define DPRINT(...)   Serial.printf(__VA_ARGS__)
  #define DPRINTLN(...) Serial.println(__VA_ARGS__)
  #else
  #define DPRINT(...)   do{}while(0)
  #define DPRINTLN(...) do{}while(0)
  #endif

  static HardwareSerial uart(1); // UART1

  static inline uint8_t calc_crc(uint8_t cmd, uint8_t val) {
    return (uint8_t)(0xAA + cmd + val);
  }

  void begin(uint32_t baud) {

    uart.begin(baud, SERIAL_8N1, /*RX*/ 15, /*TX*/ 16);
    delay(50);
  }

  HardwareSerial& port() { return uart; }

  void send(uint8_t cmd, uint8_t value) {
    uint8_t pkt[4] = {0xAA, cmd, value, calc_crc(cmd, value)};
    uart.write(pkt, 4);

    // ⚠️ No spamear Serial por defecto
    #if UART_DEBUG
    DPRINT("TX: ");
    for (int i = 0; i < 4; i++) DPRINT("0x%02X ", pkt[i]);
    DPRINT("\n");
    #endif
  }

  bool read(Packet &out) {
  static uint8_t state = 0;
  static uint8_t cmd = 0, val = 0;
  static uint32_t last_byte_ms = 0;

  while (uart.available()) {
    uint8_t b = (uint8_t)uart.read();
    uint32_t now = millis();

    if (now - last_byte_ms > 30) state = 0;  // resync si hubo pausa
    last_byte_ms = now;

    switch (state) {
      case 0: if (b == 0xAA) state = 1; break;
      case 1: cmd = b; state = 2; break;
      case 2: val = b; state = 3; break;
      case 3: {
        uint8_t rx_crc = b;
        state = 0;
        if (rx_crc == calc_crc(cmd, val)) { out.cmd = cmd; out.value = val; return true; }
      } break;
    }
  }
  return false;
}

  void setLedLevel(uint8_t level0_100) { if (level0_100 > 100) level0_100 = 100; send(CMD_LED_PWM, level0_100);}
  void setFanLevel(uint8_t level0_100) { if (level0_100 > 100) level0_100 = 100; send(CMD_FAN_PWM, level0_100);}
  void setLedTimer(uint8_t sel0_4)   { if (sel0_4 > 4) sel0_4 = 4; send(CMD_LED_TIMER, sel0_4); }
  void setFanTimer(uint8_t sel0_4)   { if (sel0_4 > 4) sel0_4 = 4; send(CMD_FAN_TIMER, sel0_4); }
  void setPrivate(uint8_t on01)      { if (on01 > 1) on01 = 1; send(CMD_PRIVATE, on01); }


} // namespace Uart
