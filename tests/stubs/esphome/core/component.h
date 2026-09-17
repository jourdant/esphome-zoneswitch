#pragma once
#include <cstddef>
#include <cstdint>
namespace esphome {
namespace setup_priority {
constexpr float DATA = 600;
}
class Component {
 public:
  virtual ~Component() = default;
  virtual float get_setup_priority() const { return 0; }
  virtual void setup() {}
  virtual void loop() {}
  virtual void dump_config() {}
};
}  // namespace esphome
