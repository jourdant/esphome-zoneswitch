#include "zoneswitch.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zoneswitch {

static const char *const TAG = "zoneswitch";

float ZoneSwitch::get_setup_priority() const { return setup_priority::DATA; }

void ZoneSwitch::dump_config() {
  ESP_LOGCONFIG(TAG, "ZoneSwitch:");
  check_uart_settings(9600);
  ESP_LOGCONFIG(TAG, "  Zones configured: %d", (int) this->zones_.size());
  ESP_LOGCONFIG(TAG, "  Switches configured: %d", (int) this->switches_.size());
  ESP_LOGCONFIG(TAG, "  Poll interval: %ums", this->poll_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Polling enabled: %s", YESNO(this->enable_polling_));
  ESP_LOGCONFIG(TAG, "  TX fallback node: 0x%02X", this->tx_node_addr_);
  ESP_LOGCONFIG(TAG, "  Last node address: 0x%02X", this->node_addr_);
  ESP_LOGCONFIG(TAG, "  Last mask: 0x%02X", this->last_mask_);
  ESP_LOGCONFIG(TAG, "  RX ok: %u", this->rx_ok_count_);
  ESP_LOGCONFIG(TAG, "  RX bad: %u", this->rx_bad_count_);
  if (this->flow_control_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Flow control pin set");
  }
}

void ZoneSwitch::register_zone(ZoneSwitchMaskListener *zone) { this->zones_.push_back(zone); }

void ZoneSwitch::register_switch(ZoneSwitchMaskListener *zone_switch) { this->switches_.push_back(zone_switch); }

void ZoneSwitch::register_diagnostic(ZoneSwitchDiagnosticListener *diagnostic) { this->diagnostics_.push_back(diagnostic); }

void ZoneSwitch::request_zone_state(uint8_t zone, bool target_on) {
  if (zone < 1 || zone > 6) {
    return;
  }

  const uint8_t bit = (uint8_t) (1 << (zone - 1));

  if (target_on) {
    this->desired_mask_ = (uint8_t) (this->desired_mask_ | bit);
  } else {
    this->desired_mask_ = (uint8_t) (this->desired_mask_ & (uint8_t) ~bit);
  }

  this->pending_desired_ = true;
}

static uint8_t reflect8(uint8_t value) {
  uint8_t reflected = 0;
  for (uint8_t bit = 0; bit < 8; bit++) {
    if (value & (1 << bit))
      reflected |= (1 << (7 - bit));
  }
  return reflected;
}

uint8_t ZoneSwitch::crc8_maxim_(const uint8_t *data, size_t len) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; i++) {
    uint8_t current = reflect8(data[i]);
    crc ^= current;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x80) {
        crc = (uint8_t) (((crc << 1) ^ 0x31) & 0xFF);
      } else {
        crc = (uint8_t) ((crc << 1) & 0xFF);
      }
    }
  }
  return reflect8(crc);
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
    diagnostic->on_diagnostics_update(this->node_addr_, this->online_);
  }
}

uint8_t ZoneSwitch::get_tx_node_() const {
  return this->node_addr_ != 0 ? this->node_addr_ : this->tx_node_addr_;
}

void ZoneSwitch::send_request_(uint8_t arg1) {
  const uint8_t node = this->get_tx_node_();
  if (node == 0x00) {
    return;
  }

  uint8_t frame[9];
  frame[0] = 0xAA;
  frame[1] = 0x00;
  frame[2] = node;
  frame[3] = this->tx_seq_++;
  frame[4] = 0x01;
  frame[5] = 0x00;
  frame[6] = arg1;
  frame[7] = crc8_maxim_(&frame[1], 6);
  frame[8] = 0x55;

  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->digital_write(true);
  }

  this->write_array(frame, sizeof(frame));
  this->flush();

  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->digital_write(false);
  }

  if (this->debug_) {
    ESP_LOGD(TAG, "TX req: node=0x%02X seq=0x%02X arg1=0x%02X chk=0x%02X", node, frame[3], arg1, frame[7]);
  }

  this->waiting_for_response_ = true;
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
  }

  if (this->pending_desired_ && this->has_status_) {
    const uint8_t diff = (uint8_t) ((this->desired_mask_ ^ this->last_mask_) & 0x3F);
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

void ZoneSwitch::handle_frame_(const uint8_t *frame) {
  if (frame[0] != 0xAA || frame[8] != 0x55) {
    this->rx_bad_count_++;
    return;
  }

  uint8_t calc = crc8_maxim_(&frame[1], 6);
  if (calc != frame[7]) {
    this->rx_bad_count_++;
    if (this->debug_) {
      ESP_LOGW(TAG, "Checksum mismatch. got=0x%02X expected=0x%02X", frame[7], calc);
    }
    return;
  }

  this->rx_ok_count_++;

  // Status response family: AA NODE 00 SEQ 81 01 MASK CHK 55
  if (frame[2] == 0x00 && frame[4] == 0x81 && frame[5] == 0x01) {
    const uint8_t previous_node_addr = this->node_addr_;
    const bool previous_online = this->online_;

    this->node_addr_ = frame[1];
    this->last_seq_ = frame[3];
    this->last_mask_ = frame[6] & 0x3F;
    if (!this->has_status_) {
      this->desired_mask_ = this->last_mask_;
    }
    this->has_status_ = true;
    this->waiting_for_response_ = false;
    this->consecutive_misses_ = 0;
    this->online_ = true;

    if (this->debug_) {
      ESP_LOGD(TAG, "Status: node=0x%02X seq=0x%02X mask=0x%02X", this->node_addr_, this->last_seq_, this->last_mask_);
    }

    this->publish_mask_(this->last_mask_);

    if (this->node_addr_ != previous_node_addr || this->online_ != previous_online) {
      this->publish_diagnostics_();
    }
  }
}

void ZoneSwitch::loop() {
  this->run_poll_cycle_();

  while (this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte)) {
      break;
    }

    if (this->rx_index_ == 0 && byte != 0xAA) {
      continue;
    }

    this->rx_frame_[this->rx_index_++] = byte;

    if (this->rx_index_ < 9) {
      continue;
    }

    this->handle_frame_(this->rx_frame_);
    this->rx_index_ = 0;
  }
}

}  // namespace zoneswitch
}  // namespace esphome
