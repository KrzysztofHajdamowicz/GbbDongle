#pragma once

#include <optional>
#include <string>

#include "esphome/components/mqtt/mqtt_client.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text/text.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/preferences.h"

#include "gbb_protocol.h"
#include "log_ring_buffer.h"
#include "modbus_executor.h"

namespace esphome {
namespace gbb_dongle {

/// GbbOptimizer cloud <-> RS485 Modbus RTU proxy, replicating GbbConnect2.
/// Runs at setup priority LATE so it executes after the mqtt component
/// (AFTER_WIFI) and after template entities restored their NVS state (DATA);
/// with mqtt enable_on_boot=false the broker settings applied here are the
/// ones the esp-mqtt client is initialized with on first connect.
class GbbDongle : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_mqtt_parent(mqtt::MQTTClientComponent *mqtt) { this->mqtt_ = mqtt; }
  void set_flow_control_pin(GPIOPin *pin) { this->flow_control_pin_ = pin; }
  void set_version(const char *version) { this->version_ = version; }
  void set_environment(const char *environment) { this->environment_ = environment; }
  void set_ca_certificate(const char *pem) { this->ca_certificate_ = pem; }

  void set_mqtt_host_text(text::Text *t) { this->mqtt_host_ = t; }
  void set_mqtt_port_number(number::Number *n) { this->mqtt_port_ = n; }
  void set_plant_id_text(text::Text *t) { this->plant_id_ = t; }
  void set_plant_token_text(text::Text *t) { this->plant_token_ = t; }
  void set_cloud_enabled_switch(switch_::Switch *s) { this->cloud_enabled_ = s; }
  void set_tls_enabled_switch(switch_::Switch *s) { this->tls_enabled_ = s; }
  void set_tls_skip_cn_check_switch(switch_::Switch *s) { this->tls_skip_cn_check_ = s; }
  void set_baud_rate_select(select::Select *s) { this->baud_rate_ = s; }
  void set_parity_select(select::Select *s) { this->parity_ = s; }

  void set_response_timeout(uint32_t ms) { this->executor_.set_response_timeout(ms); }
  void set_read_gap(uint32_t ms) { this->executor_.set_read_gap(ms); }
  void set_write_gap(uint32_t ms) { this->executor_.set_write_gap(ms); }
  void set_log_buffer_size(uint32_t size) { this->log_buffer_size_ = size; }

  // For template sensors / binary sensors in YAML.
  bool is_settings_dirty() const { return this->settings_dirty_; }
  bool is_cloud_configured() const { return this->cloud_configured_; }
  uint32_t get_requests_received() const { return this->requests_received_; }
  uint32_t get_requests_handled() const { return this->requests_handled_; }
  uint32_t get_modbus_errors() const { return this->executor_.get_error_count(); }

 protected:
  void configure_mqtt_();
  void apply_uart_settings_();
  void on_cloud_message_(const std::string &payload);
  void apply_log_level_(const std::string &level);
  void publish_response_(GbbHeader &&header);
  void mark_dirty_();

  mqtt::MQTTClientComponent *mqtt_{nullptr};
  GPIOPin *flow_control_pin_{nullptr};
  const char *version_{"dev"};
  const char *environment_{"GbbDongle"};
  const char *ca_certificate_{nullptr};

  text::Text *mqtt_host_{nullptr};
  number::Number *mqtt_port_{nullptr};
  text::Text *plant_id_{nullptr};
  text::Text *plant_token_{nullptr};
  switch_::Switch *cloud_enabled_{nullptr};
  switch_::Switch *tls_enabled_{nullptr};
  switch_::Switch *tls_skip_cn_check_{nullptr};
  select::Select *baud_rate_{nullptr};
  select::Select *parity_{nullptr};

  ModbusExecutor executor_;
  LogRingBuffer log_buffer_;
  uint32_t log_buffer_size_{65536};
  ESPPreferenceObject log_level_pref_;

  std::string topic_from_device_;
  std::string topic_keepalive_;

  std::optional<GbbHeader> pending_request_;
  bool setup_complete_{false};
  bool settings_dirty_{false};
  bool cloud_configured_{false};
  bool cloud_enable_pending_{false};
  uint32_t last_keepalive_{0};
  uint32_t requests_received_{0};
  uint32_t requests_handled_{0};
};

}  // namespace gbb_dongle
}  // namespace esphome
