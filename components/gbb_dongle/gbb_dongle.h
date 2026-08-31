#pragma once

#include <string>

#include "esphome/components/mqtt/mqtt_client.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text/text.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/preferences.h"

#include "dongle_core.h"
#include "nvs_blob_store.h"

namespace esphome {
namespace gbb_dongle {

/// GbbOptimizer cloud <-> RS485 Modbus RTU proxy, replicating GbbConnect2.
/// Runs at setup priority LATE so it executes after the mqtt component
/// (AFTER_WIFI) and after template entities restored their NVS state (DATA);
/// with mqtt enable_on_boot=false the broker settings applied here are the
/// ones the esp-mqtt client is initialized with on first connect.
class GbbDongle : public Component,
                  public uart::UARTDevice,
                  public CoreClock,
                  public ModbusPort,
                  public CloudTransport,
                  public CoreLogger {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_mqtt_parent(mqtt::MQTTClientComponent *mqtt) { this->mqtt_ = mqtt; }
  void set_flow_control_pin(GPIOPin *pin) { this->flow_control_pin_ = pin; }
  void set_version(const char *version) { this->version_ = version; }
  void set_environment(const char *environment) { this->environment_ = environment; }
  void set_client_environment(const char *client_environment) { this->client_environment_ = client_environment; }
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
  void set_wifi_signal_db_sensor(sensor::Sensor *s) { this->wifi_signal_db_ = s; }
  void set_wifi_signal_percent_sensor(sensor::Sensor *s) { this->wifi_signal_percent_ = s; }
  void set_ip_address_text_sensor(text_sensor::TextSensor *t) { this->ip_address_ = t; }
  void set_uptime_text_sensor(text_sensor::TextSensor *t) { this->uptime_text_ = t; }

  void set_response_timeout(uint32_t ms) { this->core_.set_response_timeout(ms); }
  void set_read_gap(uint32_t ms) { this->core_.set_read_gap(ms); }
  void set_write_gap(uint32_t ms) { this->core_.set_write_gap(ms); }
  void set_log_buffer_size(uint32_t size) { this->log_buffer_size_ = size; }

  void set_time_source(time::RealTimeClock *t) { this->time_source_ = t; }
  void set_emergency_persist_switch(switch_::Switch *s) { this->emergency_persist_ = s; }
  void set_emergency_minute_threshold(uint8_t minute) { this->core_.set_emergency_minute_threshold(minute); }
  void set_emergency_retry_initial(uint32_t ms) { this->core_.set_emergency_retry_initial(ms); }
  void set_emergency_retry_max(uint32_t ms) { this->core_.set_emergency_retry_max(ms); }

  // For template sensors / binary sensors in YAML.
  bool is_settings_dirty() const { return this->settings_dirty_; }
  bool is_cloud_configured() const { return this->cloud_configured_; }
  uint32_t get_requests_received() const { return this->core_.get_requests_received(); }
  uint32_t get_requests_handled() const { return this->core_.get_requests_handled(); }
  uint32_t get_modbus_errors() const { return this->core_.get_modbus_errors(); }
  uint32_t get_emergency_sets_stored() const { return this->core_.get_emergency_sets_stored(); }
  uint32_t get_emergency_runs() const { return this->core_.get_emergency_runs(); }

  uint32_t monotonic_ms() const override;
  bool wall_time(time_t &timestamp) const override;
  size_t serial_available() override;
  bool serial_read(uint8_t *byte) override;
  void serial_write(const uint8_t *data, size_t size) override;
  void serial_flush() override;
  void set_transmit_enabled(bool enabled) override;
  bool cloud_connected() const override;
  bool cloud_publish(const std::string &topic, const char *payload, size_t size, uint8_t qos, bool retain) override;
  void core_log(CoreLogLevel level, const char *tag, const char *message) override;

 protected:
  void configure_mqtt_();
  void apply_uart_settings_();
  std::string build_client_info_() const;
  void mark_dirty_();

  mqtt::MQTTClientComponent *mqtt_{nullptr};
  GPIOPin *flow_control_pin_{nullptr};
  const char *version_{"dev"};
  const char *environment_{"GbbDongle"};
  const char *client_environment_{"GbbDongle"};
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
  sensor::Sensor *wifi_signal_db_{nullptr};
  sensor::Sensor *wifi_signal_percent_{nullptr};
  text_sensor::TextSensor *ip_address_{nullptr};
  text_sensor::TextSensor *uptime_text_{nullptr};

  DongleCore core_;
  NvsBlobStore emergency_blob_store_;
  char *log_buffer_storage_{nullptr};
  uint32_t log_buffer_size_{65536};
  ESPPreferenceObject log_level_pref_;
  bool setup_complete_{false};
  bool settings_dirty_{false};
  bool cloud_configured_{false};
  bool cloud_enable_pending_{false};

  time::RealTimeClock *time_source_{nullptr};
  switch_::Switch *emergency_persist_{nullptr};
};

}  // namespace gbb_dongle
}  // namespace esphome
