#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace zoneswitch {

class ZoneSwitch;

class ZoneSwitchBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void set_parent(ZoneSwitch *parent) { this->parent_ = parent; }
  void set_zone(uint8_t zone) { this->zone_ = zone; }
  void on_mask_update(uint8_t mask);
  void dump_config() override;

 protected:
  ZoneSwitch *parent_{nullptr};
  uint8_t zone_{1};
};

}  // namespace zoneswitch
}  // namespace esphome
