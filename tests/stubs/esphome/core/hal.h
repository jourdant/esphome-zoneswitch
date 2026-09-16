#pragma once
#include <cstdint>
namespace esphome {
extern uint32_t test_now;
inline uint32_t millis() { return test_now; }
class GPIOPin {
 public:
  void setup() {}
  void digital_write(bool) {}
};
}  // namespace esphome
