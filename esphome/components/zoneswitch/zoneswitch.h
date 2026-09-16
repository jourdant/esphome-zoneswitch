#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace zoneswitch {

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
  void register_zone(ZoneSwitchMaskListener* zone);
  void register_switch(ZoneSwitchMaskListener* zone_switch);
  void register_diagnostic(ZoneSwitchDiagnosticListener* diagnostic);
  void request_zone_state(uint8_t zone, bool target_on);
  void request_refresh();
  void set_listen_only(bool value) { listen_only_ = value; }
  void set_flow_control_pin(GPIOPin* pin) { flow_control_pin_ = pin; }
  void set_debug(bool value) { debug_ = value; }
  void set_poll_interval(uint32_t value) { poll_interval_ms_ = value; }
  void set_enable_polling(bool value) { enable_polling_ = value; }
  void set_spill_zone(uint8_t value) { spill_zone_ = value; }
  void set_tx_idle_guard(uint32_t value) { tx_idle_guard_ms_ = value; }
  void set_status_timeout(uint32_t value) { status_timeout_ms_ = value; }
  void set_diagnostic_update_interval(uint32_t value) { diagnostic_update_interval_ms_ = value; }
  uint8_t get_last_mask() const { return last_mask_; }
  virtual uint8_t get_node_addr() const { return 0; }
  bool is_online() const { return online_; }
  float get_status_age() const;
  const char* get_transaction_result() const { return transaction_result_; }
  uint32_t get_tx_count() const { return tx_count_; }
  uint32_t get_ack_timeouts() const { return ack_timeouts_; }
  uint32_t get_response_timeouts() const { return response_timeouts_; }
  uint32_t get_rejected_busy() const { return rejected_busy_; }

 protected:
  virtual void queue_zone_state_(uint8_t zone, bool target_on) = 0;
  virtual void status_expired_() = 0;
  void publish_mask_(uint8_t mask);
  void publish_diagnostics_(bool force = false);
  void service_diagnostics_();
  void set_transaction_result_(const char* result);
  void service_status_timeout_(uint32_t now);
  static bool deadline_reached_(uint32_t now, uint32_t deadline);
  uint8_t apply_spill_guard_(uint8_t diff) const;
  GPIOPin* flow_control_pin_{nullptr};
  bool debug_{false};
  bool listen_only_{false};
  bool refresh_requested_{false};
  bool has_status_{false};
  bool enable_polling_{true};
  bool online_{false};
  bool waiting_for_response_{false};
  bool waiting_for_write_response_{false};
  uint8_t last_mask_{0};
  uint8_t desired_mask_{0};
  uint8_t spill_zone_{0};
  uint8_t rx_frame_[9]{};
  uint8_t rx_index_{0};
  uint32_t poll_interval_ms_{5000};
  uint32_t last_poll_ms_{0};
  uint32_t last_rx_byte_ms_{0};
  uint32_t last_status_ms_{0};
  uint32_t last_tx_ms_{0};
  uint32_t last_diagnostic_publish_ms_{0};
  uint32_t tx_idle_guard_ms_{20};
  uint32_t status_timeout_ms_{30000};
  uint32_t diagnostic_update_interval_ms_{10000};
  uint32_t rx_ok_count_{0};
  uint32_t rx_bad_count_{0};
  uint32_t tx_count_{0};
  uint32_t ack_timeouts_{0};
  uint32_t response_timeouts_{0};
  uint32_t rejected_busy_{0};
  const char* transaction_result_{"none"};
  ZoneSwitchMaskListener* mask_listeners_{nullptr};
  ZoneSwitchDiagnosticListener* diagnostic_listeners_{nullptr};
  uint8_t zone_count_{0};
  uint8_t switch_count_{0};
};

}  // namespace zoneswitch
}  // namespace esphome
