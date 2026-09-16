#include "zoneswitch_diagnostic_sensor.h"

#include "../zoneswitch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zoneswitch {

static const char* const TAG = "zoneswitch.sensor";

void ZoneSwitchDiagnosticSensor::dump_config() {
  LOG_SENSOR("", "ZoneSwitch Diagnostic Sensor", this);
  const char* metric_name = "rx_bad";
  if (metric_ == DIAGNOSTIC_METRIC_STATUS_AGE) metric_name = "status_age";
  if (metric_ == DIAGNOSTIC_METRIC_TX_COUNT) metric_name = "tx_count";
  if (metric_ == DIAGNOSTIC_METRIC_ACK_TIMEOUTS) metric_name = "ack_timeouts";
  if (metric_ == DIAGNOSTIC_METRIC_RESPONSE_TIMEOUTS) metric_name = "response_timeouts";
  if (metric_ == DIAGNOSTIC_METRIC_REJECTED_BUSY) metric_name = "rejected_busy";

  if (this->metric_ == DIAGNOSTIC_METRIC_NODE_ADDRESS) {
    metric_name = "node_address";
  } else if (this->metric_ == DIAGNOSTIC_METRIC_ONLINE) {
    metric_name = "online";
  } else if (this->metric_ == DIAGNOSTIC_METRIC_RX_OK) {
    metric_name = "rx_ok";
  }
  ESP_LOGCONFIG(TAG, "  Metric: %s", metric_name);
}

void ZoneSwitchDiagnosticSensor::publish_if_changed_(float state) {
  if (this->has_published_state_ && this->last_published_state_ == state) return;

  this->has_published_state_ = true;
  this->last_published_state_ = state;
  this->publish_state(state);
}

void ZoneSwitchDiagnosticSensor::on_diagnostics_update(uint8_t node_addr, bool online, uint32_t rx_ok_count,
                                                       uint32_t rx_bad_count) {
  if (metric_ == DIAGNOSTIC_METRIC_STATUS_AGE) {
    publish_if_changed_(parent_->get_status_age());
    return;
  }
  if (metric_ == DIAGNOSTIC_METRIC_TX_COUNT) {
    publish_if_changed_(parent_->get_tx_count());
    return;
  }
  if (metric_ == DIAGNOSTIC_METRIC_ACK_TIMEOUTS) {
    publish_if_changed_(parent_->get_ack_timeouts());
    return;
  }
  if (metric_ == DIAGNOSTIC_METRIC_RESPONSE_TIMEOUTS) {
    publish_if_changed_(parent_->get_response_timeouts());
    return;
  }
  if (metric_ == DIAGNOSTIC_METRIC_REJECTED_BUSY) {
    publish_if_changed_(parent_->get_rejected_busy());
    return;
  }
  if (this->metric_ == DIAGNOSTIC_METRIC_NODE_ADDRESS) {
    this->publish_if_changed_((float)node_addr);
    return;
  }

  if (this->metric_ == DIAGNOSTIC_METRIC_ONLINE) {
    this->publish_if_changed_(online ? 1.0f : 0.0f);
    return;
  }

  if (this->metric_ == DIAGNOSTIC_METRIC_RX_OK) {
    this->publish_if_changed_((float)rx_ok_count);
    return;
  }

  if (this->metric_ == DIAGNOSTIC_METRIC_RX_BAD) {
    this->publish_if_changed_((float)rx_bad_count);
  }
}

}  // namespace zoneswitch
}  // namespace esphome
