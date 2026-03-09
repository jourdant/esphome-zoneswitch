#include "zoneswitch_binary_sensor.h"
#include "../zoneswitch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zoneswitch {

static const char *const TAG = "zoneswitch.binary_sensor";

void ZoneSwitchBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "ZoneSwitch Binary Sensor", this);
  if (this->type_ == BINARY_SENSOR_METRIC_ZONE) {
    ESP_LOGCONFIG(TAG, "  Type: zone");
    ESP_LOGCONFIG(TAG, "  Zone: %u", this->zone_);
  } else {
    ESP_LOGCONFIG(TAG, "  Type: online");
  }
}

void ZoneSwitchBinarySensor::on_mask_update(uint8_t mask) {
  if (this->type_ != BINARY_SENSOR_METRIC_ZONE)
    return;

  if (this->zone_ < 1 || this->zone_ > 6)
    return;

  const uint8_t bit = (uint8_t) (1 << (this->zone_ - 1));
  this->publish_state((mask & bit) != 0);
}

void ZoneSwitchBinarySensor::on_diagnostics_update(uint8_t node_addr, bool online) {
  (void) node_addr;
  if (this->type_ != BINARY_SENSOR_METRIC_ONLINE)
    return;

  this->publish_state(online);
}

}  // namespace zoneswitch
}  // namespace esphome
