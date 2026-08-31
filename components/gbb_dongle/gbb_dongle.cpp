#include "gbb_dongle.h"

#include <cmath>
#include <cstdlib>

#include "esphome/components/logger/logger.h"
#include "esphome/components/network/util.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/time.h"

namespace esphome {
namespace gbb_dongle {

static const char *const TAG = "gbb_dongle";

void GbbDongle::setup() {
  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->setup();
    this->flow_control_pin_->digital_write(false);
  }

  this->core_.set_clock(this);
  this->core_.set_modbus_port(this);
  this->core_.set_cloud_transport(this);
  this->core_.set_logger(this);
  this->core_.set_blob_store(&this->emergency_blob_store_);
  this->core_.set_identity(this->version_, this->environment_, this->client_environment_);
  this->core_.set_client_info_provider([this]() { return this->build_client_info_(); });
  this->core_.set_log_level_changed([this](uint8_t gate) { this->log_level_pref_.save(&gate); });

  RAMAllocator<char> allocator;
  this->log_buffer_storage_ = allocator.allocate(this->log_buffer_size_);
  if (!this->core_.log_buffer().init(this->log_buffer_storage_, this->log_buffer_size_)) {
    ESP_LOGE(TAG, "Failed to allocate %" PRIu32 " B log buffer", this->log_buffer_size_);
  } else if (logger::global_logger != nullptr) {
    logger::global_logger->add_log_callback(&this->core_.log_buffer(), LogRingBuffer::log_hook);
  }
  this->log_level_pref_ = global_preferences->make_preference<uint8_t>(fnv1_hash("gbb_dongle_log_level"));
  uint8_t stored_gate;
  if (this->log_level_pref_.load(&stored_gate))
    this->core_.log_buffer().set_level_gate(stored_gate);
  else
    this->core_.log_buffer().set_level_gate(ESPHOME_LOG_LEVEL_DEBUG);

  for (text::Text *text : {this->mqtt_host_, this->plant_id_, this->plant_token_}) {
    if (text != nullptr)
      text->add_on_state_callback([this](const std::string &) { this->mark_dirty_(); });
  }
  if (this->mqtt_port_ != nullptr)
    this->mqtt_port_->add_on_state_callback([this](float) { this->mark_dirty_(); });
  for (switch_::Switch *setting : {this->cloud_enabled_, this->tls_enabled_, this->tls_skip_cn_check_}) {
    if (setting != nullptr)
      setting->add_on_state_callback([this](bool) { this->mark_dirty_(); });
  }
  for (select::Select *setting : {this->baud_rate_, this->parity_}) {
    if (setting != nullptr)
      setting->add_on_state_callback([this](size_t) { this->apply_uart_settings_(); });
  }

  this->apply_uart_settings_();

  if (this->emergency_persist_ != nullptr) {
    this->emergency_persist_->add_on_state_callback([this](bool state) { this->core_.set_persist_enabled(state); });
    if (this->emergency_persist_->state) {
      this->core_.set_persist_enabled(true);
      this->core_.restore_emergency();
    }
  }

  this->configure_mqtt_();
  this->setup_complete_ = true;
}

void GbbDongle::configure_mqtt_() {
  const std::string host = this->mqtt_host_ != nullptr ? this->mqtt_host_->state : "";
  const std::string plant_id = this->plant_id_ != nullptr ? this->plant_id_->state : "";
  const std::string token = this->plant_token_ != nullptr ? this->plant_token_->state : "";
  const uint16_t port =
      this->mqtt_port_ != nullptr && this->mqtt_port_->has_state() ? static_cast<uint16_t>(this->mqtt_port_->state) : 8883;

  if (host.empty() || plant_id.empty() || token.empty()) {
    ESP_LOGW(TAG, "Cloud not configured (MQTT server, Plant Id or Plant Token empty); MQTT stays disabled");
    return;
  }

  this->mqtt_->set_broker_address(host);
  this->mqtt_->set_broker_port(port);
  this->mqtt_->set_username(plant_id);
  this->mqtt_->set_password(token);
  this->mqtt_->set_client_id("GbbConnect2_" + plant_id);

  const bool tls = this->tls_enabled_ == nullptr || this->tls_enabled_->state;
  if (tls && this->ca_certificate_ != nullptr) {
    this->mqtt_->set_ca_certificate(this->ca_certificate_);
    if (this->tls_skip_cn_check_ != nullptr)
      this->mqtt_->set_skip_cert_cn_check(this->tls_skip_cn_check_->state);
  } else if (tls) {
    ESP_LOGE(TAG, "TLS requested but no CA certificate compiled in; connecting WITHOUT TLS");
  }

  this->core_.set_topics(plant_id + "/ModbusInMqtt/fromDevice", plant_id + "/keepalive");
  this->mqtt_->subscribe(
      plant_id + "/ModbusInMqtt/toDevice",
      [this](const std::string &, const std::string &payload) { this->core_.on_cloud_message(payload); }, 1);
  this->cloud_configured_ = true;
  this->core_.set_cloud_configured(true);
  if (this->cloud_enabled_ == nullptr || this->cloud_enabled_->state) {
    ESP_LOGI(TAG, "Cloud configured for GbbOptimizer at %s:%u as plant '%s' (TLS: %s); connecting once network is up",
             host.c_str(), port, plant_id.c_str(), tls && this->ca_certificate_ != nullptr ? "yes" : "NO");
    this->cloud_enable_pending_ = true;
  } else {
    ESP_LOGI(TAG, "Cloud connection disabled by switch");
  }
}

void GbbDongle::apply_uart_settings_() {
  auto *uart = this->parent_;
  if (uart == nullptr)
    return;
  uint32_t baud = 9600;
  if (this->baud_rate_ != nullptr && this->baud_rate_->has_state())
    baud = strtoul(this->baud_rate_->current_option().c_str(), nullptr, 10);
  if (baud == 0)
    baud = 9600;
  uart->set_baud_rate(baud);
  this->core_.set_baud_rate_hint(baud);

  const char *parity = "None";
  if (this->parity_ != nullptr && this->parity_->has_state())
    parity = this->parity_->current_option().c_str();
  if (str_equals_case_insensitive(parity, "Even"))
    uart->set_parity(uart::UART_CONFIG_PARITY_EVEN);
  else if (str_equals_case_insensitive(parity, "Odd"))
    uart->set_parity(uart::UART_CONFIG_PARITY_ODD);
  else
    uart->set_parity(uart::UART_CONFIG_PARITY_NONE);
  uart->load_settings(false);
  ESP_LOGI(TAG, "RS485: %" PRIu32 " baud, parity %s", baud, parity);
}

void GbbDongle::mark_dirty_() {
  if (!this->setup_complete_)
    return;
  this->settings_dirty_ = true;
  ESP_LOGI(TAG, "Cloud settings changed; restart the device to apply them");
}

std::string GbbDongle::build_client_info_() const {
  std::string info;
  const auto append = [&info](const std::string &segment) {
    if (!info.empty())
      info += ", ";
    info += segment;
  };
  if (this->wifi_signal_db_ != nullptr && this->wifi_signal_percent_ != nullptr) {
    if (this->wifi_signal_db_->has_state() && this->wifi_signal_percent_->has_state() &&
        !std::isnan(this->wifi_signal_db_->state) && !std::isnan(this->wifi_signal_percent_->state))
      append(str_sprintf("Wi-Fi %.0f%% (%.0fdBm)", this->wifi_signal_percent_->state, this->wifi_signal_db_->state));
  } else {
    append("Ethernet");
  }
  if (this->ip_address_ != nullptr && this->ip_address_->has_state())
    append("IP " + this->ip_address_->state);
  if (this->uptime_text_ != nullptr && this->uptime_text_->has_state())
    append("uptime " + this->uptime_text_->state);
  return info;
}

void GbbDongle::loop() {
  if (this->cloud_enable_pending_ && network::is_connected()) {
    this->cloud_enable_pending_ = false;
    ESP_LOGI(TAG, "Network is up; connecting to the cloud now");
    this->mqtt_->enable();
  }
  this->core_.loop();
}

uint32_t GbbDongle::monotonic_ms() const { return millis(); }

bool GbbDongle::wall_time(time_t &timestamp) const {
  if (this->time_source_ == nullptr)
    return false;
  const ESPTime now = this->time_source_->now();
  if (!now.is_valid())
    return false;
  timestamp = now.timestamp;
  return true;
}

size_t GbbDongle::serial_available() { return this->available(); }
bool GbbDongle::serial_read(uint8_t *byte) { return this->read_byte(byte); }
void GbbDongle::serial_write(const uint8_t *data, size_t size) { this->write_array(data, size); }
void GbbDongle::serial_flush() { this->flush(); }
void GbbDongle::set_transmit_enabled(bool enabled) {
  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(enabled);
}

bool GbbDongle::cloud_connected() const { return this->mqtt_->is_connected(); }
bool GbbDongle::cloud_publish(const std::string &topic, const char *payload, size_t size, uint8_t qos, bool retain) {
  return this->mqtt_->publish(topic, payload, size, qos, retain);
}

void GbbDongle::core_log(CoreLogLevel level, const char *tag, const char *message) {
  switch (level) {
    case CoreLogLevel::ERROR:
      ESP_LOGE(tag, "%s", message);
      break;
    case CoreLogLevel::WARN:
      ESP_LOGW(tag, "%s", message);
      break;
    case CoreLogLevel::INFO:
      ESP_LOGI(tag, "%s", message);
      break;
    case CoreLogLevel::DEBUG:
      ESP_LOGD(tag, "%s", message);
      break;
  }
}

void GbbDongle::dump_config() {
  ESP_LOGCONFIG(TAG, "GbbDongle:");
  ESP_LOGCONFIG(TAG, "  Version: %s (%s)", this->version_, this->environment_);
  ESP_LOGCONFIG(TAG, "  Cloud configured: %s", YESNO(this->cloud_configured_));
  ESP_LOGCONFIG(TAG, "  CA certificate compiled in: %s", YESNO(this->ca_certificate_ != nullptr));
  ESP_LOGCONFIG(TAG, "  Log buffer: %" PRIu32 " B", this->log_buffer_size_);
  ESP_LOGCONFIG(TAG, "  Emergency: threshold minute %u, %" PRIu32 " set(s) stored, persist %s",
                this->core_.get_emergency_minute_threshold(), this->core_.get_emergency_sets_stored(),
                ONOFF(this->emergency_persist_ != nullptr && this->emergency_persist_->state));
  if (this->flow_control_pin_ != nullptr)
    LOG_PIN("  Flow control pin: ", this->flow_control_pin_);
}

}  // namespace gbb_dongle
}  // namespace esphome
