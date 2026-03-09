#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"
#include "../zoneswitch.h"

namespace esphome {
namespace zoneswitch {

class ZoneSwitch;

enum BinarySensorMetric {
  BINARY_SENSOR_METRIC_ZONE = 0,
  BINARY_SENSOR_METRIC_ONLINE = 1,
};

class ZoneSwitchBinarySensor
    : public binary_sensor::BinarySensor,
      public Component,
      public ZoneSwitchMaskListener,
      public ZoneSwitchDiagnosticListener {
 public:
  void set_parent(ZoneSwitch *parent) { this->parent_ = parent; }
  void set_zone(uint8_t zone) { this->zone_ = zone; }
  void set_type(BinarySensorMetric type) { this->type_ = type; }
  void on_mask_update(uint8_t mask) override;
  void on_diagnostics_update(uint8_t node_addr, bool online) override;
  void dump_config() override;

 protected:
  ZoneSwitch *parent_{nullptr};
  uint8_t zone_{1};
  BinarySensorMetric type_{BINARY_SENSOR_METRIC_ZONE};
};

}  // namespace zoneswitch
}  // namespace esphome
