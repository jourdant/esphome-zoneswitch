#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>
namespace esphome {
namespace uart {
class UARTDevice {
 public:
  std::deque<uint8_t> rx;
  std::vector<std::vector<uint8_t>> tx;
  size_t available() { return rx.size(); }
  bool read_byte(uint8_t* out) {
    if (rx.empty()) return false;
    *out = rx.front();
    rx.pop_front();
    return true;
  }
  void write_array(const uint8_t* p, size_t n) { tx.emplace_back(p, p + n); }
  void flush() {}
  void check_uart_settings(int) {}
};
}  // namespace uart
}  // namespace esphome
