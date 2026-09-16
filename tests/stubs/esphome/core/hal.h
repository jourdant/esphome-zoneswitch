#pragma once
#include <cstdint>
namespace esphome {
extern uint32_t test_now;
inline uint32_t millis() { return test_now; }
class GPIOPin {
 public:
  void setup() {}
  bool level = true;
  unsigned high_writes = 0;
  void digital_write(bool value) {
    level = value;
    if (value) high_writes++;
  }
};
}  // namespace esphome
