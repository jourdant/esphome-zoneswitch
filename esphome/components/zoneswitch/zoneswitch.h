#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include <vector>

namespace esphome {
namespace zoneswitch {

class ZoneSwitchBinarySensor;
class ZoneSwitchSwitch;

class ZoneSwitch : public uart::UARTDevice, public Component {
 public:
  float get_setup_priority() const override;
  void dump_config() override;
  void loop() override;

  void register_zone(ZoneSwitchBinarySensor *zone);
  void register_switch(ZoneSwitchSwitch *zone_switch);
  void request_zone_state(uint8_t zone, bool target_on);

  void set_flow_control_pin(GPIOPin *flow_control_pin) { this->flow_control_pin_ = flow_control_pin; }
  void set_debug(bool debug) { this->debug_ = debug; }
  void set_poll_interval(uint32_t interval_ms) { this->poll_interval_ms_ = interval_ms; }
  void set_tx_node_addr(uint8_t tx_node_addr) { this->tx_node_addr_ = tx_node_addr; }
  void set_enable_polling(bool enable_polling) { this->enable_polling_ = enable_polling; }

  uint8_t get_last_mask() const { return this->last_mask_; }
  uint8_t get_node_addr() const { return this->node_addr_; }

 protected:
  static uint8_t crc8_maxim_(const uint8_t *data, size_t len);
  void handle_frame_(const uint8_t *frame);
  void publish_mask_(uint8_t mask);
  void run_poll_cycle_();
  void send_request_(uint8_t arg1);
  uint8_t get_tx_node_() const;

  GPIOPin *flow_control_pin_{nullptr};
  bool debug_{false};

  uint8_t rx_frame_[9]{};
  uint8_t rx_index_{0};

  uint8_t node_addr_{0};
  uint8_t tx_node_addr_{0x48};
  uint8_t last_mask_{0};
  uint8_t desired_mask_{0};
  uint8_t last_seq_{0};
  uint8_t tx_seq_{1};

  bool has_status_{false};
  bool pending_desired_{false};
  bool enable_polling_{true};

  uint32_t poll_interval_ms_{5000};
  uint32_t last_poll_ms_{0};

  uint32_t rx_ok_count_{0};
  uint32_t rx_bad_count_{0};

  std::vector<ZoneSwitchBinarySensor *> zones_;
  std::vector<ZoneSwitchSwitch *> switches_;
};

}  // namespace zoneswitch
}  // namespace esphome
