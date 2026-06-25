#include "zoneswitch.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace zoneswitch {

static const char *const TAG = "zoneswitch";
static constexpr uint32_t NODE_PREF_KEY = 0x5A510001UL;
static constexpr uint8_t NODE_PREF_MAGIC = 0xA5;

float ZoneSwitch::get_setup_priority() const { return setup_priority::DATA; }

void ZoneSwitch::setup() {
  if (!this->restore_node_) {
    return;
  }

  this->node_pref_ = global_preferences->make_preference<NodePreference>(NODE_PREF_KEY, true);
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
  check_uart_settings(9600);
  ESP_LOGCONFIG(TAG, "  Zones configured: %d", (int) this->zones_.size());
  ESP_LOGCONFIG(TAG, "  Switches configured: %d", (int) this->switches_.size());
  ESP_LOGCONFIG(TAG, "  Poll interval: %ums", this->poll_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Polling enabled: %s", YESNO(this->enable_polling_));
  ESP_LOGCONFIG(TAG, "  TX fallback node: 0x%02X", this->tx_node_addr_);
  ESP_LOGCONFIG(TAG, "  Restore learned node: %s", YESNO(this->restore_node_));
  if (this->restored_node_valid_) {
    ESP_LOGCONFIG(TAG, "  Restored node candidate: 0x%02X", this->restored_node_addr_);
  }
  ESP_LOGCONFIG(TAG, "  TX idle guard: %ums", this->tx_idle_guard_ms_);
  ESP_LOGCONFIG(TAG, "  Node confirmations required: %u", this->node_confirmations_required_);
  ESP_LOGCONFIG(TAG, "  Node mismatch threshold: %u", this->node_mismatch_threshold_);
  ESP_LOGCONFIG(TAG, "  Offline miss threshold: %u", this->offline_miss_threshold_);
  ESP_LOGCONFIG(TAG, "  Spill zone guard: %u", this->spill_zone_);
  ESP_LOGCONFIG(TAG, "  Last node address: 0x%02X", this->node_addr_);
  ESP_LOGCONFIG(TAG, "  Last mask: 0x%02X", this->last_mask_);
  ESP_LOGCONFIG(TAG, "  RX ok: %u", this->rx_ok_count_);
  ESP_LOGCONFIG(TAG, "  RX bad: %u", this->rx_bad_count_);
  if (this->learned_arg0_ != 0x00) {
    ESP_LOGCONFIG(TAG, "  Protocol variant (frame[5]): 0x%02X", this->learned_arg0_);
  } else {
    ESP_LOGCONFIG(TAG, "  Protocol variant (frame[5]): not yet learned");
  }
  if (this->flow_control_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Flow control pin set");
  } else if (this->enable_polling_ || !this->switches_.empty()) {
    ESP_LOGCONFIG(TAG, "  Component flow control pin not set; relying on UART/transceiver configuration");
  }
}

void ZoneSwitch::register_zone(ZoneSwitchMaskListener *zone) { this->zones_.push_back(zone); }

void ZoneSwitch::register_switch(ZoneSwitchMaskListener *zone_switch) { this->switches_.push_back(zone_switch); }

void ZoneSwitch::register_diagnostic(ZoneSwitchDiagnosticListener *diagnostic) { this->diagnostics_.push_back(diagnostic); }

void ZoneSwitch::request_zone_state(uint8_t zone, bool target_on) {
  if (zone < 1 || zone > 6) {
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

  const uint8_t bit = (uint8_t) (1 << (zone - 1));

  if (target_on) {
    this->desired_mask_ = (uint8_t) (this->desired_mask_ | bit);
  } else {
    this->desired_mask_ = (uint8_t) (this->desired_mask_ & (uint8_t) ~bit);
  }

  this->pending_desired_ = true;
}

uint8_t ZoneSwitch::crc8_maxim_(const uint8_t *data, size_t len) {
  // CRC-8/MAXIM (1-Wire): poly=0x31, refin=true, refout=true.
  // Using the equivalent LSB-first algorithm with the reflected polynomial
  // (0x8C) avoids per-byte bit-reversal and is both faster and simpler.
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x01) {
        crc = (crc >> 1) ^ 0x8C;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

void ZoneSwitch::publish_mask_(uint8_t mask) {
  for (auto *zone : this->zones_) {
    zone->on_mask_update(mask);
  }

  for (auto *zone_switch : this->switches_) {
    zone_switch->on_mask_update(mask);
  }
}

void ZoneSwitch::publish_diagnostics_() {
  for (auto *diagnostic : this->diagnostics_) {
    diagnostic->on_diagnostics_update(this->node_addr_, this->online_, this->rx_ok_count_, this->rx_bad_count_);
  }
}

uint8_t ZoneSwitch::get_tx_node_() const {
  if (this->node_locked_ && this->node_addr_ != 0) {
    return this->node_addr_;
  }
  if (this->restored_node_valid_ && this->restored_node_addr_ != 0) {
    return this->restored_node_addr_;
  }
  return this->tx_node_addr_;
}

void ZoneSwitch::save_locked_node_() {
  if (!this->restore_node_ || !this->node_locked_ || this->node_addr_ == 0x00 || this->learned_arg0_ == 0x00) {
    return;
  }

  if (this->restored_node_valid_ && this->restored_node_addr_ == this->node_addr_ &&
      this->restored_arg0_ == this->learned_arg0_) {
    return;
  }

  NodePreference stored{NODE_PREF_MAGIC, this->node_addr_, this->learned_arg0_};
  if (this->node_pref_.save(&stored)) {
    global_preferences->sync();
    this->restored_node_addr_ = stored.node;
    this->restored_arg0_ = stored.arg0;
    this->restored_node_valid_ = true;
    if (this->debug_) {
      ESP_LOGD(TAG, "Saved node candidate: node=0x%02X arg0=0x%02X", stored.node, stored.arg0);
    }
  } else {
    ESP_LOGW(TAG, "Failed to save node candidate");
  }
}

bool ZoneSwitch::send_request_(uint8_t arg1) {
  const uint8_t node = this->get_tx_node_();
  if (node == 0x00) {
    if (this->debug_) {
      ESP_LOGW(TAG, "Skipping TX because node address is 0x00");
    }
    return false;
  }

  if (this->flow_control_pin_ != nullptr && this->tx_de_assert_pending_) {
    if (this->debug_) {
      ESP_LOGD(TAG, "Deferring TX because DE pin is still asserted");
    }
    return false;
  }

  if (this->available() > 0) {
    if (this->debug_) {
      ESP_LOGD(TAG, "Deferring TX because RX data is pending");
    }
    return false;
  }

  const uint32_t now = millis();
  if (this->last_rx_byte_ms_ != 0 && (now - this->last_rx_byte_ms_) < this->tx_idle_guard_ms_) {
    if (this->debug_) {
      ESP_LOGD(TAG, "Deferring TX until bus has been idle for %ums", this->tx_idle_guard_ms_);
    }
    return false;
  }

  // Use the learned ARG0 variant if known, otherwise fall back to 0x00 (the
  // controller-side request value seen in all captures). The ARG0 in outbound
  // requests is always 0x00 in captured data; learned_arg0_ is a response-side
  // field and must NOT be used in TX frames.
  uint8_t frame[9];
  frame[0] = 0xAA;
  frame[1] = 0x00;
  frame[2] = node;
  frame[3] = this->tx_seq_++;
  // Skip sequence number 0x00 to avoid confusion with uninitialised state.
  if (this->tx_seq_ == 0x00)
    this->tx_seq_ = 0x01;
  frame[4] = 0x01;
  frame[5] = 0x00;
  frame[6] = arg1;
  frame[7] = crc8_maxim_(&frame[1], 6);
  frame[8] = 0x55;
  this->last_tx_seq_ = frame[3];
  this->has_last_tx_seq_ = true;

  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->digital_write(true);
  }

  this->write_array(frame, sizeof(frame));
  this->flush();

  if (this->flow_control_pin_ != nullptr) {
    // ESPHome UART flush waits for TX FIFO drain. Schedule non-blocking DE
    // de-assertion after a conservative extra frame-time margin; service_flow_control_()
    // will lower the pin in loop() once the guard window has elapsed.
    this->tx_de_assert_at_ms_ = millis() + this->tx_de_assert_delay_ms_;
    this->tx_de_assert_pending_ = true;
  }

  if (this->debug_) {
    ESP_LOGD(TAG, "TX req: node=0x%02X seq=0x%02X arg1=0x%02X chk=0x%02X", node, frame[3], arg1, frame[7]);
  }

  this->waiting_for_response_ = true;
  this->waiting_for_write_response_ = arg1 != 0x00;
  return true;
}

uint8_t ZoneSwitch::apply_spill_guard_(uint8_t diff) const {
  if (this->spill_zone_ < 1 || this->spill_zone_ > 6) {
    return diff;
  }

  const uint8_t spill_bit = (uint8_t) (1 << (this->spill_zone_ - 1));
  const uint8_t non_spill_mask = (uint8_t) (0x3F & (uint8_t) ~spill_bit);

  if ((diff & spill_bit) && !(this->desired_mask_ & spill_bit) && (this->last_mask_ & spill_bit) &&
      ((this->desired_mask_ & non_spill_mask) == 0)) {
    return (uint8_t) (diff & (uint8_t) ~spill_bit);
  }

  return diff;
}

void ZoneSwitch::run_poll_cycle_() {
  if (!this->enable_polling_) {
    return;
  }

  const uint32_t now = millis();
  if ((now - this->last_poll_ms_) < this->poll_interval_ms_) {
    return;
  }
  this->last_poll_ms_ = now;

  if (this->waiting_for_response_) {
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
      this->publish_diagnostics_();
    }

    if (this->pending_desired_) {
      this->require_fresh_status_before_write_ = true;
    }

    if (missed_write) {
      if (this->debug_) {
        ESP_LOGW(TAG, "Write response missed; waiting for fresh status before another toggle");
      }
      return;
    }
  }

  if (this->pending_desired_ && this->has_status_ && this->online_ && !this->require_fresh_status_before_write_) {
    const uint8_t diff = this->apply_spill_guard_((uint8_t) ((this->desired_mask_ ^ this->last_mask_) & 0x3F));
    if (diff != 0) {
      uint8_t toggle_bit = 0;
      for (uint8_t index = 0; index < 6; index++) {
        uint8_t bit = (uint8_t) (1 << index);
        if (diff & bit) {
          toggle_bit = bit;
          break;
        }
      }

      if (toggle_bit != 0) {
        this->send_request_(toggle_bit);
        return;
      }
    }

    this->pending_desired_ = false;
  }

  this->send_request_(0x00);
}

bool ZoneSwitch::handle_frame_(const uint8_t *frame) {
  if (frame[0] != 0xAA || frame[8] != 0x55) {
    this->rx_bad_count_++;
    this->publish_diagnostics_();
    return false;
  }

  uint8_t calc = crc8_maxim_(&frame[1], 6);
  if (calc != frame[7]) {
    this->rx_bad_count_++;
    if (this->debug_) {
      ESP_LOGW(TAG, "Checksum mismatch. got=0x%02X expected=0x%02X", frame[7], calc);
    }
    this->publish_diagnostics_();
    return false;
  }

  this->rx_ok_count_++;

  // Status response family: AA NODE 00 SEQ 0x81 <ARG0> MASK CHK 55
  //
  // frame[2] == 0x00 : SRC field is always 0x00 in controller responses
  // frame[4] == 0x81 : CMD byte for response/ack (confirmed across all captures)
  // frame[5]         : ARG0. Current captures confirm 0x01. We learn this value
  //                    with the node address and only lock it after multiple
  //                    matching status frames so we don't accidentally misinterpret
  //                    unrelated frame types.
  if (frame[2] == 0x00 && frame[4] == 0x81) {
    const uint8_t arg0 = frame[5];
    const uint8_t mask = frame[6] & 0x3F;
    if ((frame[6] & 0xC0) != 0) {
      if (this->debug_) {
        ESP_LOGW(TAG, "Ignoring status candidate with invalid zone mask: 0x%02X", frame[6]);
      }
      this->publish_diagnostics_();
      return true;
    }

    if (!this->node_locked_) {
      if (frame[1] == this->candidate_node_addr_ && arg0 == this->candidate_arg0_) {
        if (this->candidate_confirmations_ < 0xFF) {
          this->candidate_confirmations_++;
        }
      } else {
        this->candidate_node_addr_ = frame[1];
        this->candidate_arg0_ = arg0;
        this->candidate_confirmations_ = 1;
      }

      this->node_addr_ = this->candidate_node_addr_;

      if (this->debug_) {
        ESP_LOGD(TAG, "Status candidate: node=0x%02X arg0=0x%02X confirmations=%u/%u", this->candidate_node_addr_,
                 this->candidate_arg0_, this->candidate_confirmations_, this->node_confirmations_required_);
      }

      if (this->candidate_confirmations_ < this->node_confirmations_required_) {
        this->publish_diagnostics_();
        return true;
      }

      this->learned_arg0_ = this->candidate_arg0_;
      this->node_locked_ = true;
      if (this->debug_) {
        ESP_LOGD(TAG, "Locked node address: node=0x%02X frame[5]=0x%02X", this->node_addr_, this->learned_arg0_);
      }
      this->save_locked_node_();
    }

    if (frame[1] != this->node_addr_ || arg0 != this->learned_arg0_) {
      if (this->node_mismatch_count_ < 0xFF) {
        this->node_mismatch_count_++;
      }

      if (this->debug_) {
        ESP_LOGW(TAG, "Status node mismatch: got node=0x%02X arg0=0x%02X expected node=0x%02X arg0=0x%02X count=%u/%u",
                 frame[1], arg0, this->node_addr_, this->learned_arg0_, this->node_mismatch_count_,
                 this->node_mismatch_threshold_);
      }

      if (this->node_mismatch_count_ >= this->node_mismatch_threshold_) {
        ESP_LOGW(TAG, "Node mismatch threshold reached; unlocking node and restarting autodetection");
        this->node_locked_ = false;
        this->learned_arg0_ = 0x00;
        this->candidate_node_addr_ = frame[1];
        this->candidate_arg0_ = arg0;
        this->candidate_confirmations_ = 1;
        this->node_addr_ = this->candidate_node_addr_;
        this->node_mismatch_count_ = 0;
        this->restored_node_valid_ = false;
        this->has_status_ = false;
        this->online_ = false;
        this->waiting_for_response_ = false;
        this->waiting_for_write_response_ = false;
        this->pending_desired_ = false;
        this->require_fresh_status_before_write_ = true;
      }

      this->publish_diagnostics_();
      return true;
    }

    this->node_mismatch_count_ = 0;

    // Valid status response — process it.
    const uint8_t previous_mask = this->last_mask_;
    const bool previous_has_status = this->has_status_;

    if (this->waiting_for_response_ && this->has_last_tx_seq_ && frame[3] != this->last_tx_seq_ && this->debug_) {
      ESP_LOGW(TAG, "Response sequence mismatch. got=0x%02X expected=0x%02X", frame[3], this->last_tx_seq_);
    }

    this->node_addr_ = frame[1];
    this->last_seq_ = frame[3];
    this->last_mask_ = mask;
    if (!this->pending_desired_) {
      this->desired_mask_ = this->last_mask_;
    }
    this->has_status_ = true;
    this->waiting_for_response_ = false;
    this->waiting_for_write_response_ = false;
    this->require_fresh_status_before_write_ = false;
    this->consecutive_misses_ = 0;
    this->online_ = true;

    if (this->debug_) {
      ESP_LOGD(TAG, "Status: node=0x%02X seq=0x%02X mask=0x%02X", this->node_addr_, this->last_seq_, this->last_mask_);
    }

    if (!previous_has_status || this->last_mask_ != previous_mask) {
      this->publish_mask_(this->last_mask_);
    }

    this->publish_diagnostics_();
  } else {
    this->publish_diagnostics_();
  }

  return true;
}

void ZoneSwitch::service_flow_control_() {
  if (!this->tx_de_assert_pending_)
    return;
  if (this->flow_control_pin_ == nullptr) {
    this->tx_de_assert_pending_ = false;
    return;
  }
  if (millis() >= this->tx_de_assert_at_ms_) {
    this->flow_control_pin_->digital_write(false);
    this->tx_de_assert_pending_ = false;
  }
}

void ZoneSwitch::loop() {
  this->service_flow_control_();
  this->run_poll_cycle_();

  while (this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte)) {
      break;
    }
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
