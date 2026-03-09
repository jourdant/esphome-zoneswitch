#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

namespace esphome {
namespace zoneswitch {

class ZoneSwitch;

class ZoneSwitchSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(ZoneSwitch *parent) { this->parent_ = parent; }
  void set_zone(uint8_t zone) { this->zone_ = zone; }
  void on_mask_update(uint8_t mask);
  void dump_config() override;

 protected:
  void write_state(bool state) override;

  ZoneSwitch *parent_{nullptr};
  uint8_t zone_{1};
};

}  // namespace zoneswitch
}  // namespace esphome
