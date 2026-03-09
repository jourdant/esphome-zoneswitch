#include "zoneswitch_diagnostic_sensor.h"
#include "../zoneswitch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zoneswitch {

static const char *const TAG = "zoneswitch.sensor";

void ZoneSwitchDiagnosticSensor::dump_config() {
  LOG_SENSOR("", "ZoneSwitch Diagnostic Sensor", this);
  const char *metric_name = this->metric_ == DIAGNOSTIC_METRIC_NODE_ADDRESS ? "node_address" : "online";
  ESP_LOGCONFIG(TAG, "  Metric: %s", metric_name);
}

void ZoneSwitchDiagnosticSensor::on_diagnostics_update(uint8_t node_addr, bool online) {
  if (this->metric_ == DIAGNOSTIC_METRIC_NODE_ADDRESS) {
    this->publish_state((float) node_addr);
    return;
  }

  if (this->metric_ == DIAGNOSTIC_METRIC_ONLINE) {
    this->publish_state(online ? 1.0f : 0.0f);
  }
}

}  // namespace zoneswitch
}  // namespace esphome
