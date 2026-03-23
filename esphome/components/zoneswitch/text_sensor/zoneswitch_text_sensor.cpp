#include "zoneswitch_text_sensor.h"
#include "../zoneswitch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zoneswitch {

static const char *const TAG = "zoneswitch.text_sensor";

void ZoneSwitchTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "ZoneSwitch Text Sensor", this);
  ESP_LOGCONFIG(TAG, "  Metric: node_address");
}

void ZoneSwitchTextSensor::on_diagnostics_update(uint8_t node_addr, bool online) {
  (void) online;
  if (this->metric_ != TEXT_SENSOR_METRIC_NODE_ADDRESS)
    return;

  char buf[7];
  snprintf(buf, sizeof(buf), "0x%02X", node_addr);
  this->publish_state(buf);
}

}  // namespace zoneswitch
}  // namespace esphome
