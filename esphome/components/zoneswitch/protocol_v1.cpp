#include <cstring>

#include "esphome/core/log.h"
#include "zoneswitch.h"

#ifdef USE_ZONESWITCH_V1
#include "esphome/components/uart/uart_component_esp_idf.h"
#include "hal/gpio_ll.h"
#include "hal/uart_ll.h"
#endif

namespace esphome {
namespace zoneswitch {

static const char* const TAG = "zoneswitch.v1";
static constexpr uint32_t RESPONSE_TIMEOUT_MS = 250;
static constexpr uint32_t TRANSACTION_GAP_MS = 1000;
static constexpr uint32_t FRAME_TIMEOUT_MS = 20;

bool ZoneSwitch::send_v1_byte_(uint8_t byte) {
#ifdef USE_ZONESWITCH_V1
  // The component exclusively owns TX. Do not mix uart.write/bridges with this
  // direct-FIFO transport. IDF continues to own the RX ISR and ring buffer.
  auto* bus = static_cast<uart::IDFUARTComponent*>(this->parent_);
  auto* hw = UART_LL_GET_HW(bus->get_hw_serial_number());
  if (!uart_ll_is_tx_idle(hw)) return false;
  static portMUX_TYPE tx_mux = portMUX_INITIALIZER_UNLOCKED;
  portENTER_CRITICAL(&tx_mux);
  gpio_ll_set_level(&GPIO, this->v1_direction_pin_, 1);
  delay_microseconds_safe(10);
  uart_ll_write_txfifo(hw, &byte, 1);
  const uint32_t started = micros();
  bool timed_out = false;
  while (!uart_ll_is_tx_idle(hw)) {
    if (static_cast<uint32_t>(micros() - started) >= 200) {
      timed_out = true;
      break;
    }
  }
  gpio_ll_set_level(&GPIO, this->v1_direction_pin_, 0);
  portEXIT_CRITICAL(&tx_mux);
  return !timed_out;
#else
  return false;
#endif
}

bool ZoneSwitch::transact_v1_(uint8_t mask) {
#ifdef USE_ZONESWITCH_V1
  if (!this->send_v1_byte_(0xC0)) return false;
  auto* bus = static_cast<uart::IDFUARTComponent*>(this->parent_);
  const auto port = static_cast<uart_port_t>(bus->get_hw_serial_number());
  const uint32_t started = micros();
  uint8_t ack = 0;
  int received = 0;
  // Interrupts remain enabled during this bounded ACK wait. No logging or UART
  // debug callbacks between ACK and mask: they spoil the measured turnaround.
  while (static_cast<uint32_t>(micros() - started) < 10000) {
    received = uart_read_bytes(port, &ack, 1, 0);
    if (received != 0) break;
  }
  if (received != 1 || ack != 0x30) {
    ESP_LOGW(TAG, "Handshake failed: read=%d ACK=%02X; no mask sent, no retry", received, ack);
    return false;
  }
  const bool sent = this->send_v1_byte_(mask);
  if (this->debug_) ESP_LOGD(TAG, "TX C0 -> RX 30 -> TX %02X; DE LOW; sent=%s", mask, YESNO(sent));
  return sent;
#else
  ESP_LOGE(TAG, "V1 requires the ESP32-S3 ESP-IDF transport");
  return false;
#endif
}

void ZoneSwitch::fail_v1_transaction_() {
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

bool ZoneSwitch::handle_v1_status_(const uint8_t* frame) {
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

void ZoneSwitch::receive_v1_byte_(uint8_t byte) {
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

void ZoneSwitch::loop_v1_() {
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
  if (this->diagnostics_dirty_ && now - this->last_diagnostic_publish_ms_ >= this->diagnostic_update_interval_ms_)
    this->publish_diagnostics_(true);
  this->service_status_timeout_(now);
  if (this->waiting_for_response_) {
    if (now - this->last_tx_ms_ >= RESPONSE_TIMEOUT_MS) this->fail_v1_transaction_();
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
  if (!this->transact_v1_(mask)) {
    this->fail_v1_transaction_();
    return;
  }
  this->waiting_for_response_ = true;
  this->waiting_for_write_response_ = mask != 0;
  this->last_tx_ms_ = millis();
}

}  // namespace zoneswitch
}  // namespace esphome
