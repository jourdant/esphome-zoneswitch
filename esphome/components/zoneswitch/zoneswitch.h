#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace zoneswitch {

enum class Protocol : uint8_t { V1, V2 };

class ZoneSwitchMaskListener {
 public:
  virtual void on_mask_update(uint8_t mask) = 0;
  virtual ~ZoneSwitchMaskListener() = default;

 protected:
  friend class ZoneSwitch;
  ZoneSwitchMaskListener* next_mask_listener_{nullptr};
};

class ZoneSwitchDiagnosticListener {
 public:
  virtual void on_diagnostics_update(uint8_t node_addr, bool online, uint32_t rx_ok_count, uint32_t rx_bad_count) = 0;
  virtual ~ZoneSwitchDiagnosticListener() = default;

 protected:
  friend class ZoneSwitch;
  ZoneSwitchDiagnosticListener* next_diagnostic_listener_{nullptr};
};

class ZoneSwitch : public uart::UARTDevice, public Component {
 public:
  float get_setup_priority() const override;
  void setup() override;
  void dump_config() override;
  void loop() override;

  void register_zone(ZoneSwitchMaskListener* zone);
  void register_switch(ZoneSwitchMaskListener* zone_switch);
  void register_diagnostic(ZoneSwitchDiagnosticListener* diagnostic);
  void request_zone_state(uint8_t zone, bool target_on);
  void request_refresh() { this->refresh_requested_ = true; }
  void set_protocol(Protocol protocol) { this->protocol_ = protocol; }
  void set_v1_direction_pin(uint8_t pin) { this->v1_direction_pin_ = pin; }

  void set_flow_control_pin(GPIOPin* flow_control_pin) { this->flow_control_pin_ = flow_control_pin; }
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
  void set_preference_key(uint32_t preference_key) { this->preference_key_ = preference_key; }
  void set_status_timeout(uint32_t timeout_ms) { this->status_timeout_ms_ = timeout_ms; }
  void set_diagnostic_update_interval(uint32_t interval_ms) { this->diagnostic_update_interval_ms_ = interval_ms; }

  uint8_t get_last_mask() const { return this->last_mask_; }
  uint8_t get_node_addr() const { return this->node_addr_; }
  bool is_online() const { return this->online_; }

 protected:
  void loop_v1_();
  void receive_v1_byte_(uint8_t byte);
  bool handle_v1_status_(const uint8_t* frame);
  void fail_v1_transaction_();
  // Virtual boundary supports deterministic host tests without ESP-IDF hardware.
  virtual bool transact_v1_(uint8_t mask);
  bool send_v1_byte_(uint8_t byte);

  Protocol protocol_{Protocol::V2};
  bool refresh_requested_{false};
  uint8_t v1_direction_pin_{0};
  uint8_t v1_pending_zone_{0};
  uint8_t v1_prefix_index_{0};
  bool v1_target_on_{false};
  bool v1_write_ready_{false};
  bool v1_has_tx_{false};

  static uint8_t crc8_maxim_(const uint8_t* data, size_t len);
  bool handle_frame_(const uint8_t* frame);
  void publish_mask_(uint8_t mask);
  void publish_diagnostics_(bool force = false);
  void run_poll_cycle_();
  void service_flow_control_();
  void service_status_timeout_(uint32_t now);
  void service_response_timeout_(uint32_t now);
  bool tx_retry_due_(uint32_t now) const;
  static bool deadline_reached_(uint32_t now, uint32_t deadline);
  bool send_request_(uint8_t arg1);
  uint8_t get_tx_node_() const;
  uint8_t apply_spill_guard_(uint8_t diff) const;
  void save_locked_node_();

  struct NodePreference {
    uint8_t magic;
    uint8_t node;
    uint8_t arg0;
  };

  GPIOPin* flow_control_pin_{nullptr};
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
  uint32_t last_status_ms_{0};
  uint32_t last_tx_ms_{0};
  uint32_t next_tx_retry_ms_{0};
  uint32_t last_diagnostic_publish_ms_{0};
  uint32_t tx_idle_guard_ms_{20};
  uint32_t tx_de_assert_delay_ms_{20};
  uint32_t tx_de_assert_at_ms_{0};
  uint32_t status_timeout_ms_{30000};
  uint32_t diagnostic_update_interval_ms_{10000};
  uint32_t preference_key_{0x5A510001UL};

  bool tx_de_assert_pending_{false};
  bool diagnostics_dirty_{false};

  uint32_t rx_ok_count_{0};
  uint32_t rx_bad_count_{0};

  ESPPreferenceObject node_pref_{};

  ZoneSwitchMaskListener* mask_listeners_{nullptr};
  ZoneSwitchDiagnosticListener* diagnostic_listeners_{nullptr};
  uint8_t zone_count_{0};
  uint8_t switch_count_{0};
};

}  // namespace zoneswitch
}  // namespace esphome
