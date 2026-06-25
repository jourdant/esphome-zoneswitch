#include "zoneswitch_switch.h"
#include "../zoneswitch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zoneswitch {

static const char *const TAG = "zoneswitch.switch";

void ZoneSwitchSwitch::setup() { this->publish_if_changed_(false); }

void ZoneSwitchSwitch::publish_if_changed_(bool state) {
  if (this->has_published_state_ && this->last_published_state_ == state)
    return;

  this->has_published_state_ = true;
  this->last_published_state_ = state;
  this->publish_state(state);
}

void ZoneSwitchSwitch::dump_config() {
  LOG_SWITCH("", "ZoneSwitch Switch", this);
  ESP_LOGCONFIG(TAG, "  Zone: %u", this->zone_);
}

void ZoneSwitchSwitch::on_mask_update(uint8_t mask) {
  if (this->zone_ < 1 || this->zone_ > 6)
    return;

  const uint8_t bit = (uint8_t) (1 << (this->zone_ - 1));
  this->publish_if_changed_((mask & bit) != 0);
}

void ZoneSwitchSwitch::write_state(bool state) {
  if (this->parent_ == nullptr)
    return;

  this->parent_->request_zone_state(this->zone_, state);
}

}  // namespace zoneswitch
}  // namespace esphome
