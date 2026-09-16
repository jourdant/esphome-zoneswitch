#include "zoneswitch.h"

#include <cmath>

#include "esphome/core/log.h"
namespace esphome {
namespace zoneswitch {
static const char* const TAG = "zoneswitch";
float ZoneSwitch::get_setup_priority() const { return setup_priority::DATA; }

void ZoneSwitch::register_zone(ZoneSwitchMaskListener* zone) {
  zone->next_mask_listener_ = this->mask_listeners_;
  this->mask_listeners_ = zone;
  this->zone_count_++;
}

void ZoneSwitch::register_switch(ZoneSwitchMaskListener* zone_switch) {
  zone_switch->next_mask_listener_ = this->mask_listeners_;
  this->mask_listeners_ = zone_switch;
  this->switch_count_++;
}

void ZoneSwitch::register_diagnostic(ZoneSwitchDiagnosticListener* diagnostic) {
  diagnostic->next_diagnostic_listener_ = this->diagnostic_listeners_;
  this->diagnostic_listeners_ = diagnostic;
}

void ZoneSwitch::publish_mask_(uint8_t mask) {
  for (auto* listener = this->mask_listeners_; listener != nullptr; listener = listener->next_mask_listener_) {
    listener->on_mask_update(mask);
  }
}

void ZoneSwitch::publish_diagnostics_(bool force) {
  const uint32_t now = millis();
  if (!force && (now - this->last_diagnostic_publish_ms_) < this->diagnostic_update_interval_ms_) {
    return;
  }

  this->last_diagnostic_publish_ms_ = now;
  for (auto* listener = this->diagnostic_listeners_; listener != nullptr;
       listener = listener->next_diagnostic_listener_) {
    listener->on_diagnostics_update(this->get_node_addr(), this->online_, this->rx_ok_count_, this->rx_bad_count_);
  }
}

bool ZoneSwitch::deadline_reached_(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

uint8_t ZoneSwitch::apply_spill_guard_(uint8_t diff) const {
  if (this->spill_zone_ < 1 || this->spill_zone_ > 6) {
    return diff;
  }

  const uint8_t spill_bit = (uint8_t)(1 << (this->spill_zone_ - 1));
  const uint8_t non_spill_mask = (uint8_t)(0x3F & (uint8_t)~spill_bit);

  if ((diff & spill_bit) && !(this->desired_mask_ & spill_bit) && (this->last_mask_ & spill_bit) &&
      ((this->desired_mask_ & non_spill_mask) == 0)) {
    return (uint8_t)(diff & (uint8_t)~spill_bit);
  }

  return diff;
}

void ZoneSwitch::service_status_timeout_(uint32_t now) {
  if (this->status_timeout_ms_ == 0 || !this->online_ || !this->has_status_ ||
      (now - this->last_status_ms_) < this->status_timeout_ms_) {
    return;
  }

  this->online_ = false;
  this->waiting_for_response_ = false;
  this->waiting_for_write_response_ = false;
  this->status_expired_();
  this->set_transaction_result_("status_timeout");
  if (this->debug_) {
    ESP_LOGW(TAG, "Marked offline after %ums without a valid status", static_cast<unsigned>(this->status_timeout_ms_));
  }
  this->publish_diagnostics_(true);
}
void ZoneSwitch::setup() {
  if (flow_control_pin_ != nullptr) {
    flow_control_pin_->setup();
    flow_control_pin_->digital_write(false);
  }
}
void ZoneSwitch::dump_config() {
  ESP_LOGCONFIG(TAG, "ZoneSwitch:");
  ESP_LOGCONFIG(TAG, "  Listen only: %s", YESNO(listen_only_));
  ESP_LOGCONFIG(TAG, "  Zones: %u; switches: %u", zone_count_, switch_count_);
  ESP_LOGCONFIG(TAG, "  Polling: %s; interval: %ums", YESNO(enable_polling_), static_cast<unsigned>(poll_interval_ms_));
  ESP_LOGCONFIG(TAG, "  Status timeout: %ums", static_cast<unsigned>(status_timeout_ms_));
}
void ZoneSwitch::request_refresh() {
  if (listen_only_) {
    set_transaction_result_("listen_only");
    return;
  }
  refresh_requested_ = true;
}
void ZoneSwitch::request_zone_state(uint8_t zone, bool target_on) {
  if (listen_only_) {
    set_transaction_result_("listen_only");
    return;
  }
  if (zone < 1 || zone > 6) {
    set_transaction_result_("invalid_zone");
    return;
  }
  queue_zone_state_(zone, target_on);
}
float ZoneSwitch::get_status_age() const {
  return has_status_ ? static_cast<float>(millis() - last_status_ms_) / 1000.0f : NAN;
}
void ZoneSwitch::set_transaction_result_(const char* result) {
  transaction_result_ = result;
  publish_diagnostics_(true);
}
void ZoneSwitch::service_diagnostics_() {
  // Age changes even when the event-driven bus is silent.
  if (millis() - last_diagnostic_publish_ms_ >= diagnostic_update_interval_ms_) publish_diagnostics_(true);
}

}  // namespace zoneswitch
}  // namespace esphome
