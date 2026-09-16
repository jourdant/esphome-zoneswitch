#pragma once
#include "transport_v1.h"
#include "zoneswitch.h"
namespace esphome {
namespace zoneswitch {
class V1ZoneSwitch : public ZoneSwitch {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  void set_v1_direction_pin(uint8_t pin) { v1_direction_pin_ = pin; }

 protected:
  void queue_zone_state_(uint8_t zone, bool target_on) override;
  void status_expired_() override;
  void receive_v1_byte_(uint8_t byte);
  bool handle_v1_status_(const uint8_t* frame);
  void fail_v1_transaction_();
  virtual V1TransportResult transact_v1_(uint8_t mask);
  uint8_t v1_direction_pin_{0};
  uint8_t v1_pending_zone_{0};
  uint8_t v1_prefix_index_{0};
  bool v1_target_on_{false};
  bool v1_write_ready_{false};
  bool v1_has_tx_{false};
};

}  // namespace zoneswitch
}  // namespace esphome
