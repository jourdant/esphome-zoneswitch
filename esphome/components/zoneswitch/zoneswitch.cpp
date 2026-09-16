#include "zoneswitch.h"

#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zoneswitch {

static const char* const TAG = "zoneswitch";
static constexpr uint8_t NODE_PREF_MAGIC = 0xA5;
static constexpr uint8_t MAX_RX_BYTES_PER_LOOP = 32;
static constexpr uint32_t RX_FRAME_TIMEOUT_MS = 20;
static constexpr uint32_t TX_RETRY_INTERVAL_MS = 10;

float ZoneSwitch::get_setup_priority() const { return setup_priority::DATA; }

void ZoneSwitch::setup() {
  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->setup();
    this->flow_control_pin_->digital_write(false);
  }
  if (this->protocol_ == Protocol::V1) {
    this->refresh_requested_ = true;  // One startup query; no periodic polling by default.
    return;
  }
  if (!this->restore_node_) {
    return;
  }

  this->node_pref_ = global_preferences->make_preference<NodePreference>(this->preference_key_, true);
  NodePreference restored{};
  if (!this->node_pref_.load(&restored) || restored.magic != NODE_PREF_MAGIC || restored.node == 0x00) {
    if (this->debug_) {
      ESP_LOGD(TAG, "No restored node candidate available");
    }
    return;
  }

  this->restored_node_addr_ = restored.node;
  this->restored_arg0_ = restored.arg0;
  this->restored_node_valid_ = true;
  this->candidate_node_addr_ = restored.node;
  this->candidate_arg0_ = restored.arg0;
  this->candidate_confirmations_ = 0;
  this->node_addr_ = restored.node;

  if (this->debug_) {
    ESP_LOGD(TAG, "Restored node candidate: node=0x%02X arg0=0x%02X", restored.node, restored.arg0);
  }
}

void ZoneSwitch::dump_config() {
  ESP_LOGCONFIG(TAG, "ZoneSwitch:");
  ESP_LOGCONFIG(TAG, "  Protocol: %s", this->protocol_ == Protocol::V1 ? "V1 (experimental)" : "V2");
  check_uart_settings(this->protocol_ == Protocol::V1 ? 250000 : 9600);
  if (this->protocol_ == Protocol::V1) {
    ESP_LOGCONFIG(TAG, "  Wall LEDs may remain stale; external queries can impair wall-button response");
    ESP_LOGCONFIG(TAG, "  Polling: %s; node-address diagnostics do not apply", YESNO(this->enable_polling_));
    return;
  }
  ESP_LOGCONFIG(TAG, "  Zones configured: %u", this->zone_count_);
  ESP_LOGCONFIG(TAG, "  Switches configured: %u", this->switch_count_);
  ESP_LOGCONFIG(TAG, "  Poll interval: %ums", static_cast<unsigned>(this->poll_interval_ms_));
  ESP_LOGCONFIG(TAG, "  Polling enabled: %s", YESNO(this->enable_polling_));
  ESP_LOGCONFIG(TAG, "  TX fallback node: 0x%02X", this->tx_node_addr_);
  ESP_LOGCONFIG(TAG, "  Restore learned node: %s", YESNO(this->restore_node_));
  if (this->restored_node_valid_) {
    ESP_LOGCONFIG(TAG, "  Restored node candidate: 0x%02X", this->restored_node_addr_);
  }
  ESP_LOGCONFIG(TAG, "  TX idle guard: %ums", static_cast<unsigned>(this->tx_idle_guard_ms_));
  ESP_LOGCONFIG(TAG, "  Node confirmations required: %u", this->node_confirmations_required_);
  ESP_LOGCONFIG(TAG, "  Node mismatch threshold: %u", this->node_mismatch_threshold_);
  ESP_LOGCONFIG(TAG, "  Offline miss threshold: %u", this->offline_miss_threshold_);
  ESP_LOGCONFIG(TAG, "  Status timeout: %ums", static_cast<unsigned>(this->status_timeout_ms_));
  ESP_LOGCONFIG(TAG, "  Diagnostic update interval: %ums", static_cast<unsigned>(this->diagnostic_update_interval_ms_));
  ESP_LOGCONFIG(TAG, "  Spill zone guard: %u", this->spill_zone_);
  ESP_LOGCONFIG(TAG, "  Last node address: 0x%02X", this->node_addr_);
  ESP_LOGCONFIG(TAG, "  Last mask: 0x%02X", this->last_mask_);
  ESP_LOGCONFIG(TAG, "  RX ok: %u", static_cast<unsigned>(this->rx_ok_count_));
  ESP_LOGCONFIG(TAG, "  RX bad: %u", static_cast<unsigned>(this->rx_bad_count_));
  if (this->learned_arg0_ != 0x00) {
    ESP_LOGCONFIG(TAG, "  Protocol variant (frame[5]): 0x%02X", this->learned_arg0_);
  } else {
    ESP_LOGCONFIG(TAG, "  Protocol variant (frame[5]): not yet learned");
  }
  if (this->flow_control_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Flow control pin set");
  } else if (this->enable_polling_ || this->switch_count_ != 0) {
    ESP_LOGCONFIG(TAG, "  Component flow control pin not set; relying on UART/transceiver configuration");
  }
}

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

void ZoneSwitch::request_zone_state(uint8_t zone, bool target_on) {
  if (zone < 1 || zone > 6) {
    return;
  }

  if (this->protocol_ == Protocol::V1) {
    if (this->waiting_for_response_ || this->v1_pending_zone_ != 0) {
      ESP_LOGW(TAG, "V1 request busy; command not queued");
      return;
    }
    this->v1_pending_zone_ = zone;
    this->v1_target_on_ = target_on;
    this->v1_write_ready_ = false;
    this->refresh_requested_ = true;  // Obtain current state before deciding whether to toggle.
    return;
  }

  if (!this->has_status_ || !this->online_) {
    if (this->debug_) {
      ESP_LOGW(TAG, "Ignoring zone request before first valid status frame");
    }
    return;
  }

  if (!this->pending_desired_) {
    this->desired_mask_ = this->last_mask_;
  }

  const uint8_t bit = (uint8_t)(1 << (zone - 1));

  if (target_on) {
    this->desired_mask_ = (uint8_t)(this->desired_mask_ | bit);
  } else {
    this->desired_mask_ = (uint8_t)(this->desired_mask_ & (uint8_t)~bit);
  }

  this->pending_desired_ = true;
}

void ZoneSwitch::publish_mask_(uint8_t mask) {
  for (auto* listener = this->mask_listeners_; listener != nullptr; listener = listener->next_mask_listener_) {
    listener->on_mask_update(mask);
  }
}

void ZoneSwitch::publish_diagnostics_(bool force) {
  const uint32_t now = millis();
  this->diagnostics_dirty_ = true;
  if (!force && (now - this->last_diagnostic_publish_ms_) < this->diagnostic_update_interval_ms_) {
    return;
  }

  this->last_diagnostic_publish_ms_ = now;
  this->diagnostics_dirty_ = false;
  for (auto* listener = this->diagnostic_listeners_; listener != nullptr;
       listener = listener->next_diagnostic_listener_) {
    listener->on_diagnostics_update(this->node_addr_, this->online_, this->rx_ok_count_,
                                    static_cast<unsigned>(this->rx_bad_count_));
  }
}

bool ZoneSwitch::deadline_reached_(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

bool ZoneSwitch::tx_retry_due_(uint32_t now) const {
  return this->next_tx_retry_ms_ == 0 || deadline_reached_(now, this->next_tx_retry_ms_);
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

void ZoneSwitch::run_poll_cycle_() {
  const uint32_t now = millis();
  this->service_status_timeout_(now);
  this->service_response_timeout_(now);

  if (!this->waiting_for_response_ && this->pending_desired_ && this->has_status_ && this->online_ &&
      !this->require_fresh_status_before_write_ && this->tx_retry_due_(now)) {
    const uint8_t diff = this->apply_spill_guard_((uint8_t)((this->desired_mask_ ^ this->last_mask_) & 0x3F));
    if (diff != 0) {
      uint8_t toggle_bit = 0;
      for (uint8_t index = 0; index < 6; index++) {
        uint8_t bit = (uint8_t)(1 << index);
        if (diff & bit) {
          toggle_bit = bit;
          break;
        }
      }

      if (toggle_bit != 0) {
        if (!this->send_request_(toggle_bit)) {
          this->next_tx_retry_ms_ = now + TX_RETRY_INTERVAL_MS;
        }
        return;
      }
    }

    this->pending_desired_ = false;
  }

  if ((!this->enable_polling_ && !this->refresh_requested_) || this->waiting_for_response_ ||
      (!this->refresh_requested_ && (now - this->last_poll_ms_) < this->poll_interval_ms_) ||
      !this->tx_retry_due_(now)) {
    return;
  }

  if (this->send_request_(0x00)) {
    this->last_poll_ms_ = now;
    this->refresh_requested_ = false;
  } else {
    this->next_tx_retry_ms_ = now + TX_RETRY_INTERVAL_MS;
  }
}

void ZoneSwitch::service_status_timeout_(uint32_t now) {
  if (this->status_timeout_ms_ == 0 || !this->online_ || this->last_status_ms_ == 0 ||
      (now - this->last_status_ms_) < this->status_timeout_ms_) {
    return;
  }

  this->online_ = false;
  this->waiting_for_response_ = false;
  this->waiting_for_write_response_ = false;
  this->require_fresh_status_before_write_ = this->pending_desired_;
  if (this->debug_) {
    ESP_LOGW(TAG, "Marked offline after %ums without a valid status", static_cast<unsigned>(this->status_timeout_ms_));
  }
  this->publish_diagnostics_(true);
}

void ZoneSwitch::service_response_timeout_(uint32_t now) {
  if (!this->waiting_for_response_ || (now - this->last_tx_ms_) < this->poll_interval_ms_) {
    return;
  }

  const bool missed_write = this->waiting_for_write_response_;
  this->waiting_for_response_ = false;
  this->waiting_for_write_response_ = false;

  if (this->consecutive_misses_ < 0xFF) {
    this->consecutive_misses_++;
  }
  if (this->consecutive_misses_ >= this->offline_miss_threshold_ && this->online_) {
    this->online_ = false;
    if (this->debug_) {
      ESP_LOGW(TAG, "Marked offline after %u missed responses", this->consecutive_misses_);
    }
    this->publish_diagnostics_(true);
  }
  if (this->pending_desired_) {
    this->require_fresh_status_before_write_ = true;
  }
  if (missed_write && this->debug_) {
    ESP_LOGW(TAG, "Write response missed; waiting for fresh status before another toggle");
  }
}

void ZoneSwitch::service_flow_control_() {
  if (!this->tx_de_assert_pending_) return;
  if (this->flow_control_pin_ == nullptr) {
    this->tx_de_assert_pending_ = false;
    return;
  }
  if (deadline_reached_(millis(), this->tx_de_assert_at_ms_)) {
    this->flow_control_pin_->digital_write(false);
    this->tx_de_assert_pending_ = false;
  }
}

void ZoneSwitch::loop() {
  if (this->protocol_ == Protocol::V1) {
    this->loop_v1_();
    return;
  }
  this->service_flow_control_();
  this->run_poll_cycle_();

  const uint32_t now = millis();
  if (this->diagnostics_dirty_ && (now - this->last_diagnostic_publish_ms_) >= this->diagnostic_update_interval_ms_) {
    this->publish_diagnostics_(true);
  }
  if (this->rx_index_ != 0 && (now - this->last_rx_byte_ms_) >= RX_FRAME_TIMEOUT_MS) {
    this->rx_index_ = 0;
    this->rx_bad_count_++;
    this->publish_diagnostics_();
  }

  uint8_t processed = 0;
  while (processed < MAX_RX_BYTES_PER_LOOP && this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte)) {
      break;
    }
    processed++;
    this->last_rx_byte_ms_ = millis();

    if (this->rx_index_ == 0 && byte != 0xAA) {
      continue;
    }

    this->rx_frame_[this->rx_index_++] = byte;

    if (this->rx_index_ < 9) {
      continue;
    }

    const bool handled = this->handle_frame_(this->rx_frame_);
    this->rx_index_ = 0;
    if (!handled) {
      for (uint8_t index = 1; index < 9; index++) {
        if (this->rx_frame_[index] == 0xAA) {
          this->rx_index_ = 9 - index;
          memmove(this->rx_frame_, &this->rx_frame_[index], this->rx_index_);
          break;
        }
      }
    }
  }
}

}  // namespace zoneswitch
}  // namespace esphome
