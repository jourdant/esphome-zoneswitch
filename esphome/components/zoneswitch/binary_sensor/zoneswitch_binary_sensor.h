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
  void on_diagnostics_update(uint8_t node_addr, bool online, uint32_t rx_ok_count, uint32_t rx_bad_count) override;
  void dump_config() override;
  void setup() override;

 protected:
  void publish_if_changed_(bool state);

  ZoneSwitch *parent_{nullptr};
  uint8_t zone_{1};
  BinarySensorMetric type_{BINARY_SENSOR_METRIC_ZONE};
  bool has_published_state_{false};
  bool last_published_state_{false};
};

}  // namespace zoneswitch
}  // namespace esphome
