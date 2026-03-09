#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "../zoneswitch.h"

namespace esphome {
namespace zoneswitch {

enum DiagnosticMetric {
  DIAGNOSTIC_METRIC_NODE_ADDRESS = 0,
  DIAGNOSTIC_METRIC_ONLINE = 1,
};

class ZoneSwitch;

class ZoneSwitchDiagnosticSensor : public sensor::Sensor, public Component, public ZoneSwitchDiagnosticListener {
 public:
  void set_parent(ZoneSwitch *parent) { this->parent_ = parent; }
  void set_metric(DiagnosticMetric metric) { this->metric_ = metric; }

  void on_diagnostics_update(uint8_t node_addr, bool online) override;
  void dump_config() override;

 protected:
  ZoneSwitch *parent_{nullptr};
  DiagnosticMetric metric_{DIAGNOSTIC_METRIC_NODE_ADDRESS};
};

}  // namespace zoneswitch
}  // namespace esphome
