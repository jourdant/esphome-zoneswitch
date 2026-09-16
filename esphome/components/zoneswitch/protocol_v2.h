#pragma once
#include "esphome/core/preferences.h"
#include "zoneswitch.h"
namespace esphome {
namespace zoneswitch {
class V2ZoneSwitch : public ZoneSwitch {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  uint8_t get_node_addr() const override { return node_addr_; }
  void set_tx_node_addr(uint8_t tx_node_addr) { this->tx_node_addr_ = tx_node_addr; }
  void set_offline_miss_threshold(uint8_t threshold) { this->offline_miss_threshold_ = threshold; }
  void set_node_confirmations(uint8_t confirmations) { this->node_confirmations_required_ = confirmations; }
  void set_node_mismatch_threshold(uint8_t threshold) { this->node_mismatch_threshold_ = threshold; }
  void set_restore_node(bool restore_node) { this->restore_node_ = restore_node; }
  void set_preference_key(uint32_t preference_key) { this->preference_key_ = preference_key; }

 protected:
  void queue_zone_state_(uint8_t zone, bool target_on) override;
  void status_expired_() override { require_fresh_status_before_write_ = pending_desired_; }
  static uint8_t crc8_maxim_(const uint8_t* data, size_t len);
  bool handle_frame_(const uint8_t* frame);
  void run_poll_cycle_();
  void service_flow_control_();
  void service_response_timeout_(uint32_t now);
  bool tx_retry_due_(uint32_t now) const;
  bool send_request_(uint8_t arg1);
  uint8_t get_tx_node_() const;
  void save_locked_node_();
  struct NodePreference {
    uint8_t magic;
    uint8_t node;
    uint8_t arg0;
  };
  uint8_t node_addr_{0};
  uint8_t tx_node_addr_{0x48};
  uint8_t last_seq_{0};
  uint8_t tx_seq_{1};
  uint8_t last_tx_seq_{0};
  uint8_t learned_arg0_{0x00};
  uint8_t candidate_node_addr_{0x00};
  uint8_t candidate_arg0_{0x00};
  uint8_t candidate_confirmations_{0};
  uint8_t node_confirmations_required_{3};
  uint8_t node_mismatch_count_{0};
  uint8_t node_mismatch_threshold_{5};
  uint8_t restored_node_addr_{0x00};
  uint8_t restored_arg0_{0x00};
  bool node_locked_{false};
  bool restore_node_{false};
  bool restored_node_valid_{false};
  bool pending_desired_{false};
  bool require_fresh_status_before_write_{false};
  bool has_last_tx_seq_{false};
  uint8_t consecutive_misses_{0};
  uint8_t offline_miss_threshold_{5};
  uint32_t next_tx_retry_ms_{0};
  uint32_t tx_de_assert_delay_ms_{20};
  uint32_t tx_de_assert_at_ms_{0};
  uint32_t preference_key_{0x5A510001UL};
  bool tx_de_assert_pending_{false};
  ESPPreferenceObject node_pref_{};
};

}  // namespace zoneswitch
}  // namespace esphome
