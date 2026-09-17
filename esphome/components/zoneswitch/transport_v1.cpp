#include "transport_v1.h"

#include "esphome/core/log.h"

#ifdef USE_ZONESWITCH_V1
#include "esphome/components/uart/uart_component_esp_idf.h"
#include "hal/gpio_ll.h"
#include "hal/uart_ll.h"
#endif

namespace esphome {
namespace zoneswitch {

static const char* const TAG = "zoneswitch.v1";

#ifdef USE_ZONESWITCH_V1
static bool send_byte(uart::UARTComponent* parent, uint8_t direction_pin, uint8_t byte) {
  // The component exclusively owns TX. Do not mix uart.write/bridges with this
  // direct-FIFO transport. IDF continues to own the RX ISR and ring buffer.
  auto* bus = static_cast<uart::IDFUARTComponent*>(parent);
  auto* hw = UART_LL_GET_HW(bus->get_hw_serial_number());
  if (!uart_ll_is_tx_idle(hw)) return false;
  static portMUX_TYPE tx_mux = portMUX_INITIALIZER_UNLOCKED;
  portENTER_CRITICAL(&tx_mux);
  gpio_ll_set_level(&GPIO, direction_pin, 1);
  delay_microseconds_safe(10);
  uart_ll_write_txfifo(hw, &byte, 1);
  const uint32_t started = micros();
  bool timed_out = false;
  while (!uart_ll_is_tx_idle(hw)) {
    if (static_cast<uint32_t>(micros() - started) >= 200) {
      timed_out = true;
      break;
    }
  }
  gpio_ll_set_level(&GPIO, direction_pin, 0);
  portEXIT_CRITICAL(&tx_mux);
  return !timed_out;
}
#endif
V1TransportResult transact_v1(uart::UARTComponent* parent, uint8_t direction_pin, uint8_t mask) {
#ifdef USE_ZONESWITCH_V1
  if (!send_byte(parent, direction_pin, 0xC0)) return V1TransportResult::TX_FAILED;
  auto* bus = static_cast<uart::IDFUARTComponent*>(parent);
  const auto port = static_cast<uart_port_t>(bus->get_hw_serial_number());
  const uint32_t started = micros();
  uint8_t ack = 0;
  int received = 0;
  // Interrupts remain enabled during this bounded ACK wait. No logging or UART
  // debug callbacks between ACK and mask: they spoil the measured turnaround.
  while (static_cast<uint32_t>(micros() - started) < 10000) {
    received = uart_read_bytes(port, &ack, 1, 0);
    if (received != 0) break;
  }
  if (received != 1 || ack != 0x30) {
    ESP_LOGW(TAG, "Handshake failed: read=%d ACK=%02X; no mask sent, no retry", received, ack);
    return received == 0 ? V1TransportResult::ACK_TIMEOUT : V1TransportResult::ACK_INVALID;
  }
  const bool sent = send_byte(parent, direction_pin, mask);

  return sent ? V1TransportResult::SENT : V1TransportResult::TX_FAILED;
#else
  ESP_LOGE(TAG, "V1 requires the ESP32-S3 ESP-IDF transport");
  return V1TransportResult::TX_FAILED;
#endif
}
}  // namespace zoneswitch
}  // namespace esphome
