#include "zoneswitch_text_sensor.h"

#include <cstdio>
#include <cstring>

#include "../zoneswitch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zoneswitch {

static const char* const TAG = "zoneswitch.text_sensor";

void ZoneSwitchTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "ZoneSwitch Text Sensor", this);
  ESP_LOGCONFIG(TAG, "  Metric: %s",
                metric_ == TEXT_SENSOR_METRIC_NODE_ADDRESS ? "node_address" : "transaction_result");
  ESP_LOGCONFIG(TAG, "  Format: %s", this->format_ == TEXT_SENSOR_FORMAT_HEX ? "hex" : "decimal");
}

void ZoneSwitchTextSensor::publish_if_changed_(uint8_t node_addr, const char* state) {
  if (this->has_published_state_ && this->last_published_node_addr_ == node_addr) return;

  this->has_published_state_ = true;
  this->last_published_node_addr_ = node_addr;
  this->publish_state(state);
}

void ZoneSwitchTextSensor::on_diagnostics_update(uint8_t node_addr, bool online, uint32_t rx_ok_count,
                                                 uint32_t rx_bad_count) {
  if (metric_ == TEXT_SENSOR_METRIC_TRANSACTION_RESULT) {
    const char* result = parent_->get_transaction_result();
    if (last_result_ == nullptr || strcmp(result, last_result_) != 0) {
      last_result_ = result;
      publish_state(result);
    }
    return;
  }
  if (this->metric_ == TEXT_SENSOR_METRIC_NODE_ADDRESS) {
    if (this->has_published_state_ && this->last_published_node_addr_ == node_addr) return;
    char buf[7];
    if (this->format_ == TEXT_SENSOR_FORMAT_HEX) {
      snprintf(buf, sizeof(buf), "0x%02X", node_addr);
    } else {
      snprintf(buf, sizeof(buf), "%u", node_addr);
    }
    this->publish_if_changed_(node_addr, buf);
  }
}

}  // namespace zoneswitch
}  // namespace esphome
