#include "protocol_v1.h"

#include <cstring>

#include "esphome/core/log.h"
namespace esphome {
namespace zoneswitch {
static const char* const TAG = "zoneswitch.v1";
static constexpr uint32_t RESPONSE_TIMEOUT_MS = 250;
static constexpr uint32_t TRANSACTION_GAP_MS = 1000;
static constexpr uint32_t FRAME_TIMEOUT_MS = 20;
void V1ZoneSwitch::fail_v1_transaction_() {
  this->waiting_for_response_ = false;
  this->waiting_for_write_response_ = false;
  this->v1_pending_zone_ = 0;
  this->v1_write_ready_ = false;
  this->refresh_requested_ = false;
  this->rx_index_ = 0;
  this->v1_prefix_index_ = 0;
  // Unknown outcome: abandon intent. Neither a toggle nor a query is retried.
  this->online_ = false;
  this->publish_diagnostics_(true);
  ESP_LOGW(TAG, "V1 transaction unconfirmed; command cancelled; refresh state before another command");
}

bool V1ZoneSwitch::handle_v1_status_(const uint8_t* frame) {
  uint8_t sum = frame[0];
  uint8_t mask = 0;
  bool valid = frame[0] == 0x08;
  for (uint8_t i = 0; i < 6; i++) {
    valid &= (frame[i + 1] & 0x7F) == i;
    sum += frame[i + 1];
    if (frame[i + 1] & 0x80) mask |= 1U << i;
  }
  // Additive checksum fits all captured states; other variants remain unproven.
  if (!valid || sum != frame[7]) {
    this->rx_bad_count_++;
    this->publish_diagnostics_();
    return false;
  }
  this->rx_ok_count_++;
  const bool changed = !this->has_status_ || this->last_mask_ != mask;
  const bool became_online = !this->online_;
  this->last_mask_ = mask;
  this->has_status_ = true;
  this->online_ = true;
  this->last_status_ms_ = millis();
  if (this->waiting_for_response_) {
    this->set_transaction_result_("status_received");
    // V1 has no sequence/address correlation. Shared-bus collisions remain a
    // protocol limitation, not proof that every response belongs to our query.
    this->v1_write_ready_ = !this->waiting_for_write_response_ && this->v1_pending_zone_ != 0;
    this->waiting_for_response_ = false;
    this->waiting_for_write_response_ = false;
  }
  if (changed) this->publish_mask_(mask);
  this->publish_diagnostics_(became_online);
  if (this->debug_) ESP_LOGD(TAG, "Status mask=%02X (controller-reported; wall LEDs may differ)", mask);
  return true;
}

void V1ZoneSwitch::receive_v1_byte_(uint8_t byte) {
  // Recognise an observed C0/30/mask prefix so zone 4's mask 08 is not
  // counted as a corrupt status. Also accept bare statuses after our own TX.
  if (this->rx_index_ == 0) {
    if (byte == 0xC0) {
      this->v1_prefix_index_ = 1;
      return;
    }
    if (this->v1_prefix_index_ == 1 && byte == 0x30) {
      this->v1_prefix_index_ = 2;
      return;
    }
    if (this->v1_prefix_index_ == 2) {
      this->v1_prefix_index_ = 0;
      if (byte == 0 || (byte <= 0x20 && (byte & (byte - 1)) == 0)) return;
    }
    this->v1_prefix_index_ = 0;
  }
  // On a false status start, slide to a following 08 rather than discarding it.
  if (this->rx_index_ == 0 && byte != 0x08) return;
  this->rx_frame_[this->rx_index_++] = byte;
  if (this->rx_index_ < 8) return;
  const bool valid = this->handle_v1_status_(this->rx_frame_);
  this->rx_index_ = 0;
  if (!valid) {
    for (uint8_t i = 1; i < 8; i++) {
      if (this->rx_frame_[i] == 0x08) {
        this->rx_index_ = 8 - i;
        memmove(this->rx_frame_, this->rx_frame_ + i, this->rx_index_);
        break;
      }
    }
  }
}

void V1ZoneSwitch::loop() {
  uint32_t now = millis();
  if (now - this->last_rx_byte_ms_ >= FRAME_TIMEOUT_MS) this->v1_prefix_index_ = 0;
  if (this->rx_index_ != 0 && now - this->last_rx_byte_ms_ >= FRAME_TIMEOUT_MS) {
    this->rx_index_ = 0;
    this->rx_bad_count_++;
    this->publish_diagnostics_();
  }
  // Drain received state before deciding to transmit.
  for (uint8_t n = 0; n < 64 && this->available(); n++) {
    uint8_t byte;
    if (!this->read_byte(&byte)) break;
    this->last_rx_byte_ms_ = millis();
    this->receive_v1_byte_(byte);
  }
  now = millis();
  this->service_diagnostics_();
  this->service_status_timeout_(now);
  if (this->listen_only_) return;
  if (this->waiting_for_response_) {
    if (now - this->last_tx_ms_ >= RESPONSE_TIMEOUT_MS) {
      this->response_timeouts_++;
      this->set_transaction_result_("response_timeout");
      this->fail_v1_transaction_();
    }
    return;
  }
  if (this->available() || this->rx_index_ != 0 || now - this->last_rx_byte_ms_ < this->tx_idle_guard_ms_ ||
      (this->v1_has_tx_ && now - this->last_tx_ms_ < TRANSACTION_GAP_MS))
    return;

  uint8_t mask = 0;
  if (this->v1_pending_zone_ != 0 && this->v1_write_ready_) {
    if (!this->online_) {
      this->fail_v1_transaction_();
      return;
    }
    const uint8_t bit = 1U << (this->v1_pending_zone_ - 1);
    const bool current = (this->last_mask_ & bit) != 0;
    if (current != this->v1_target_on_) {
      this->desired_mask_ = this->v1_target_on_ ? this->last_mask_ | bit : this->last_mask_ & ~bit;
      mask = this->apply_spill_guard_(bit);
    }
    // Intent is consumed before transmission, including failures or no-op.
    this->v1_pending_zone_ = 0;
    this->v1_write_ready_ = false;
    if (mask == 0) return;
  } else if (!this->refresh_requested_ &&
             (!this->enable_polling_ || now - this->last_poll_ms_ < this->poll_interval_ms_)) {
    return;
  }
  this->refresh_requested_ = false;
  this->last_tx_ms_ = now;
  this->last_poll_ms_ = now;
  this->v1_has_tx_ = true;
  this->tx_count_++;
  const auto result = this->transact_v1_(mask);
  if (result != V1TransportResult::SENT) {
    if (result == V1TransportResult::ACK_TIMEOUT) this->ack_timeouts_++;
    this->set_transaction_result_(result == V1TransportResult::ACK_TIMEOUT   ? "ack_timeout"
                                  : result == V1TransportResult::ACK_INVALID ? "ack_invalid"
                                                                             : "tx_failed");
    this->fail_v1_transaction_();
    return;
  }
  this->set_transaction_result_("awaiting_status");
  this->waiting_for_response_ = true;
  this->waiting_for_write_response_ = mask != 0;
  this->last_tx_ms_ = millis();
}
void V1ZoneSwitch::setup() {
  ZoneSwitch::setup();
  refresh_requested_ = !listen_only_;  // One startup query; periodic polling defaults off.
}
void V1ZoneSwitch::dump_config() {
  ZoneSwitch::dump_config();
  ESP_LOGCONFIG(TAG, "  Protocol: V1 (experimental)");
  ESP_LOGCONFIG(TAG, "  External queries can leave wall LEDs stale and delay physical button response");
}
void V1ZoneSwitch::queue_zone_state_(uint8_t zone, bool target_on) {
  if (waiting_for_response_ || v1_pending_zone_ != 0) {
    rejected_busy_++;
    set_transaction_result_("rejected_busy");
    return;
  }
  v1_pending_zone_ = zone;
  v1_target_on_ = target_on;
  v1_write_ready_ = false;
  refresh_requested_ = true;
}
void V1ZoneSwitch::status_expired_() {
  v1_pending_zone_ = 0;
  v1_write_ready_ = false;
  refresh_requested_ = false;
}
V1TransportResult V1ZoneSwitch::transact_v1_(uint8_t mask) {
  if (listen_only_) return V1TransportResult::TX_FAILED;
  auto result = transact_v1(parent_, v1_direction_pin_, mask);
  if (debug_ && result == V1TransportResult::SENT) ESP_LOGD(TAG, "TX C0 -> RX 30 -> TX %02X; DE LOW", mask);
  return result;
}

}  // namespace zoneswitch
}  // namespace esphome
