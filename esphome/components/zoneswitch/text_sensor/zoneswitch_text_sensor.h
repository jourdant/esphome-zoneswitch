#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "../zoneswitch.h"

namespace esphome {
namespace zoneswitch {

enum TextSensorMetric {
  TEXT_SENSOR_METRIC_NODE_ADDRESS = 0,
};

class ZoneSwitch;

class ZoneSwitchTextSensor : public text_sensor::TextSensor, public Component, public ZoneSwitchDiagnosticListener {
 public:
  void set_parent(ZoneSwitch *parent) { this->parent_ = parent; }
  void set_metric(TextSensorMetric metric) { this->metric_ = metric; }

  void on_diagnostics_update(uint8_t node_addr, bool online) override;
  void dump_config() override;

 protected:
  ZoneSwitch *parent_{nullptr};
  TextSensorMetric metric_{TEXT_SENSOR_METRIC_NODE_ADDRESS};
};

}  // namespace zoneswitch
}  // namespace esphome
