#include "zoneswitch.h"
#include "binary_sensor/zoneswitch_binary_sensor.h"
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
  ESP_LOGCONFIG(TAG, "  Poll interval: %ums", this->poll_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Last node address: 0x%02X", this->node_addr_);
  ESP_LOGCONFIG(TAG, "  Last mask: 0x%02X", this->last_mask_);
  ESP_LOGCONFIG(TAG, "  RX ok: %u", this->rx_ok_count_);
  ESP_LOGCONFIG(TAG, "  RX bad: %u", this->rx_bad_count_);
  if (this->flow_control_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Flow control pin set");
  }
}

void ZoneSwitch::register_zone(ZoneSwitchBinarySensor *zone) { this->zones_.push_back(zone); }

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
    this->node_addr_ = frame[1];
    this->last_seq_ = frame[3];
    this->last_mask_ = frame[6] & 0x3F;

    if (this->debug_) {
      ESP_LOGD(TAG, "Status: node=0x%02X seq=0x%02X mask=0x%02X", this->node_addr_, this->last_seq_, this->last_mask_);
    }

    this->publish_mask_(this->last_mask_);
  }
}

void ZoneSwitch::loop() {
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

  // Passive by default: rely on existing touchpad traffic.
  // Poll transmit can be added later if needed.
  (void) this->last_poll_ms_;
}

}  // namespace zoneswitch
}  // namespace esphome
