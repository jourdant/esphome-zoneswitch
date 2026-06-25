#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"
#include <vector>

namespace esphome {
namespace zoneswitch {

class ZoneSwitchMaskListener {
 public:
  virtual void on_mask_update(uint8_t mask) = 0;
  virtual ~ZoneSwitchMaskListener() = default;
};

class ZoneSwitchDiagnosticListener {
 public:
  virtual void on_diagnostics_update(uint8_t node_addr, bool online, uint32_t rx_ok_count, uint32_t rx_bad_count) = 0;
  virtual ~ZoneSwitchDiagnosticListener() = default;
};

class ZoneSwitch : public uart::UARTDevice, public Component {
 public:
  float get_setup_priority() const override;
  void setup() override;
  void dump_config() override;
  void loop() override;

  void register_zone(ZoneSwitchMaskListener *zone);
  void register_switch(ZoneSwitchMaskListener *zone_switch);
  void register_diagnostic(ZoneSwitchDiagnosticListener *diagnostic);
  void request_zone_state(uint8_t zone, bool target_on);

  void set_flow_control_pin(GPIOPin *flow_control_pin) { this->flow_control_pin_ = flow_control_pin; }
  void set_debug(bool debug) { this->debug_ = debug; }
  void set_poll_interval(uint32_t interval_ms) { this->poll_interval_ms_ = interval_ms; }
  void set_tx_node_addr(uint8_t tx_node_addr) { this->tx_node_addr_ = tx_node_addr; }
  void set_enable_polling(bool enable_polling) { this->enable_polling_ = enable_polling; }
  void set_offline_miss_threshold(uint8_t threshold) { this->offline_miss_threshold_ = threshold; }
  void set_spill_zone(uint8_t spill_zone) { this->spill_zone_ = spill_zone; }
  void set_tx_idle_guard(uint32_t guard_ms) { this->tx_idle_guard_ms_ = guard_ms; }
  void set_node_confirmations(uint8_t confirmations) { this->node_confirmations_required_ = confirmations; }
  void set_node_mismatch_threshold(uint8_t threshold) { this->node_mismatch_threshold_ = threshold; }
  void set_restore_node(bool restore_node) { this->restore_node_ = restore_node; }

  uint8_t get_last_mask() const { return this->last_mask_; }
  uint8_t get_node_addr() const { return this->node_addr_; }
  bool is_online() const { return this->online_; }

 protected:
  static uint8_t crc8_maxim_(const uint8_t *data, size_t len);
  bool handle_frame_(const uint8_t *frame);
  void publish_mask_(uint8_t mask);
  void publish_diagnostics_();
  void run_poll_cycle_();
  void service_flow_control_();
  bool send_request_(uint8_t arg1);
  uint8_t get_tx_node_() const;
  uint8_t apply_spill_guard_(uint8_t diff) const;
  void save_locked_node_();

  struct NodePreference {
    uint8_t magic;
    uint8_t node;
    uint8_t arg0;
  };

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
  uint8_t last_tx_seq_{0};
  uint8_t spill_zone_{0};

  // Learned protocol variant: frame[5] value in status responses.
  // 0x00 means not yet locked. Current captures confirm 0x01.
  uint8_t learned_arg0_{0x00};
  uint8_t candidate_node_addr_{0x00};
  uint8_t candidate_arg0_{0x00};
  uint8_t candidate_confirmations_{0};
  uint8_t node_confirmations_required_{3};
  uint8_t node_mismatch_count_{0};
  uint8_t node_mismatch_threshold_{5};
  uint8_t restored_node_addr_{0x00};
  uint8_t restored_arg0_{0x00};
 
  bool has_status_{false};
  bool node_locked_{false};
  bool restore_node_{false};
  bool restored_node_valid_{false};
  bool pending_desired_{false};
  bool enable_polling_{true};
  bool online_{false};
  bool waiting_for_response_{false};
  bool waiting_for_write_response_{false};
  bool require_fresh_status_before_write_{false};
  bool has_last_tx_seq_{false};

  uint8_t consecutive_misses_{0};
  uint8_t offline_miss_threshold_{5};

  uint32_t poll_interval_ms_{5000};
  uint32_t last_poll_ms_{0};
  uint32_t last_rx_byte_ms_{0};
  uint32_t tx_idle_guard_ms_{20};
  uint32_t tx_de_assert_delay_ms_{20};
  uint32_t tx_de_assert_at_ms_{0};

  bool tx_de_assert_pending_{false};

  uint32_t rx_ok_count_{0};
  uint32_t rx_bad_count_{0};

  ESPPreferenceObject node_pref_{};

  std::vector<ZoneSwitchMaskListener *> zones_;
  std::vector<ZoneSwitchMaskListener *> switches_;
  std::vector<ZoneSwitchDiagnosticListener *> diagnostics_;
};

}  // namespace zoneswitch
}  // namespace esphome
