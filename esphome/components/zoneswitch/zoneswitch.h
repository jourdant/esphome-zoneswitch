#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include <vector>

namespace esphome {
namespace zoneswitch {

class ZoneSwitchBinarySensor;

class ZoneSwitch : public uart::UARTDevice, public Component {
 public:
  float get_setup_priority() const override;
  void dump_config() override;
  void loop() override;

  void register_zone(ZoneSwitchBinarySensor *zone);

  void set_flow_control_pin(GPIOPin *flow_control_pin) { this->flow_control_pin_ = flow_control_pin; }
  void set_debug(bool debug) { this->debug_ = debug; }
  void set_poll_interval(uint32_t interval_ms) { this->poll_interval_ms_ = interval_ms; }

  uint8_t get_last_mask() const { return this->last_mask_; }
  uint8_t get_node_addr() const { return this->node_addr_; }

 protected:
  static uint8_t crc8_maxim_(const uint8_t *data, size_t len);
  void handle_frame_(const uint8_t *frame);
  void publish_mask_(uint8_t mask);

  GPIOPin *flow_control_pin_{nullptr};
  bool debug_{false};

  uint8_t rx_frame_[9]{};
  uint8_t rx_index_{0};

  uint8_t node_addr_{0};
  uint8_t last_mask_{0};
  uint8_t last_seq_{0};

  uint32_t poll_interval_ms_{5000};
  uint32_t last_poll_ms_{0};

  uint32_t rx_ok_count_{0};
  uint32_t rx_bad_count_{0};

  std::vector<ZoneSwitchBinarySensor *> zones_;
};

}  // namespace zoneswitch
}  // namespace esphome
