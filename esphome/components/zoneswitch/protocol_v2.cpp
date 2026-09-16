#include "esphome/core/log.h"
#include "zoneswitch.h"

namespace esphome {
namespace zoneswitch {

static const char* const TAG = "zoneswitch.v2";
static constexpr uint8_t NODE_PREF_MAGIC = 0xA5;

uint8_t ZoneSwitch::crc8_maxim_(const uint8_t* data, size_t len) {
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
      ESP_LOGD(TAG, "Deferring TX until bus has been idle for %ums", static_cast<unsigned>(this->tx_idle_guard_ms_));
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
  if (this->tx_seq_ == 0x00) this->tx_seq_ = 0x01;
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
  this->last_tx_ms_ = millis();
  this->next_tx_retry_ms_ = 0;
  return true;
}

bool ZoneSwitch::handle_frame_(const uint8_t* frame) {
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
      const uint8_t previous_candidate_node = this->candidate_node_addr_;
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
        this->publish_diagnostics_(this->candidate_node_addr_ != previous_candidate_node);
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

      this->publish_diagnostics_(!this->node_locked_);
      return true;
    }

    this->node_mismatch_count_ = 0;

    // Valid status response — process it.
    const uint8_t previous_mask = this->last_mask_;
    const bool previous_has_status = this->has_status_;

    const bool sequence_matches = this->has_last_tx_seq_ && frame[3] == this->last_tx_seq_;
    if (this->waiting_for_response_ && !sequence_matches && this->debug_) {
      ESP_LOGW(TAG, "Response sequence mismatch. got=0x%02X expected=0x%02X", frame[3], this->last_tx_seq_);
    }

    this->node_addr_ = frame[1];
    this->last_seq_ = frame[3];
    this->last_mask_ = mask;
    if (!this->pending_desired_) {
      this->desired_mask_ = this->last_mask_;
    }
    this->has_status_ = true;
    this->last_status_ms_ = millis();
    if (!this->waiting_for_write_response_ || sequence_matches) {
      this->waiting_for_response_ = false;
      this->waiting_for_write_response_ = false;
      this->require_fresh_status_before_write_ = false;
    }
    this->consecutive_misses_ = 0;
    const bool became_online = !this->online_;
    this->online_ = true;

    if (this->debug_) {
      ESP_LOGD(TAG, "Status: node=0x%02X seq=0x%02X mask=0x%02X", this->node_addr_, this->last_seq_, this->last_mask_);
    }

    if (!previous_has_status || this->last_mask_ != previous_mask) {
      this->publish_mask_(this->last_mask_);
    }

    this->publish_diagnostics_(became_online);
  } else {
    this->publish_diagnostics_();
  }

  return true;
}

}  // namespace zoneswitch
}  // namespace esphome
