#include "zoneswitch_binary_sensor.h"
#include "../zoneswitch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zoneswitch {

static const char *const TAG = "zoneswitch.binary_sensor";

void ZoneSwitchBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "ZoneSwitch Binary Sensor", this);
  ESP_LOGCONFIG(TAG, "  Zone: %u", this->zone_);
}

void ZoneSwitchBinarySensor::on_mask_update(uint8_t mask) {
  if (this->zone_ < 1 || this->zone_ > 6)
    return;

  const uint8_t bit = (uint8_t) (1 << (this->zone_ - 1));
  this->publish_state((mask & bit) != 0);
}

}  // namespace zoneswitch
}  // namespace esphome
