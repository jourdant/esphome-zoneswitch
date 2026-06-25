#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "../zoneswitch.h"

namespace esphome {
namespace zoneswitch {

class ZoneSwitch;

class ZoneSwitchSwitch : public switch_::Switch, public Component, public ZoneSwitchMaskListener {
 public:
  void set_parent(ZoneSwitch *parent) { this->parent_ = parent; }
  void set_zone(uint8_t zone) { this->zone_ = zone; }
  void on_mask_update(uint8_t mask) override;
  void dump_config() override;
  void setup() override;

 protected:
  void write_state(bool state) override;
  void publish_if_changed_(bool state);

  ZoneSwitch *parent_{nullptr};
  uint8_t zone_{1};
  bool has_published_state_{false};
  bool last_published_state_{false};
};

}  // namespace zoneswitch
}  // namespace esphome
