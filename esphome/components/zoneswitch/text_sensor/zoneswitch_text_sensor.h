#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "../zoneswitch.h"

namespace esphome {
namespace zoneswitch {

enum TextSensorMetric {
  TEXT_SENSOR_METRIC_NODE_ADDRESS = 0,
};

enum TextSensorFormat {
  TEXT_SENSOR_FORMAT_HEX = 0,
  TEXT_SENSOR_FORMAT_DECIMAL = 1,
};

class ZoneSwitch;

class ZoneSwitchTextSensor : public text_sensor::TextSensor, public Component, public ZoneSwitchDiagnosticListener {
 public:
  void set_parent(ZoneSwitch *parent) { this->parent_ = parent; }
  void set_metric(TextSensorMetric metric) { this->metric_ = metric; }
  void set_format(TextSensorFormat format) { this->format_ = format; }

  void on_diagnostics_update(uint8_t node_addr, bool online, uint32_t rx_ok_count, uint32_t rx_bad_count) override;
  void dump_config() override;

 protected:
  void publish_if_changed_(uint8_t node_addr, const char *state);

  ZoneSwitch *parent_{nullptr};
  TextSensorMetric metric_{TEXT_SENSOR_METRIC_NODE_ADDRESS};
  TextSensorFormat format_{TEXT_SENSOR_FORMAT_HEX};
  bool has_published_state_{false};
  uint8_t last_published_node_addr_{0};
};

}  // namespace zoneswitch
}  // namespace esphome
