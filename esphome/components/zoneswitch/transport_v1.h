#pragma once
#include "esphome/components/uart/uart.h"
namespace esphome {
namespace zoneswitch {
enum class V1TransportResult { SENT, ACK_TIMEOUT, ACK_INVALID, TX_FAILED };
V1TransportResult transact_v1(uart::UARTComponent* bus, uint8_t direction_pin, uint8_t mask);
}  // namespace zoneswitch
}  // namespace esphome
