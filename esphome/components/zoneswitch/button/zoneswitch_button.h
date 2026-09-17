#pragma once
#include "../zoneswitch.h"
#include "esphome/components/button/button.h"

namespace esphome {
namespace zoneswitch {
class ZoneSwitchRefreshButton : public button::Button {
 public:
  void set_parent(ZoneSwitch* parent) { this->parent_ = parent; }

 protected:
  void press_action() override { this->parent_->request_refresh(); }
  ZoneSwitch* parent_{nullptr};
};
}  // namespace zoneswitch
}  // namespace esphome
