#include "gbb_dongle.h"

#include <cstdlib>

#include "esphome/components/logger/logger.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gbb_dongle {

static const char *const TAG = "gbb_dongle";

static const uint32_t KEEPALIVE_INTERVAL_MS = 60 * 1000;
static const size_t LAST_LOG_MAX_BYTES = 8 * 1024;

void GbbDongle::setup() {
  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->setup();
    this->flow_control_pin_->digital_write(false);
  }

  if (!this->log_buffer_.init(this->log_buffer_size_)) {
    ESP_LOGE(TAG, "Failed to allocate %" PRIu32 " B log buffer", this->log_buffer_size_);
  } else if (logger::global_logger != nullptr) {
    logger::global_logger->add_log_callback(&this->log_buffer_, LogRingBuffer::log_hook);
  }
  this->log_level_pref_ = global_preferences->make_preference<uint8_t>(fnv1_hash("gbb_dongle_log_level"));
  uint8_t stored_gate;
  if (this->log_level_pref_.load(&stored_gate)) {
    this->log_buffer_.set_level_gate(stored_gate);
  } else {
    this->log_buffer_.set_level_gate(ESPHOME_LOG_LEVEL_DEBUG);
  }

  // Any change to the cloud settings only takes effect after a restart,
  // because the esp-mqtt client config is built once on first connect.
  for (text::Text *t : {this->mqtt_host_, this->plant_id_, this->plant_token_}) {
    if (t != nullptr)
      t->add_on_state_callback([this](const std::string &) { this->mark_dirty_(); });
  }
  if (this->mqtt_port_ != nullptr)
    this->mqtt_port_->add_on_state_callback([this](float) { this->mark_dirty_(); });
  for (switch_::Switch *s : {this->cloud_enabled_, this->tls_enabled_, this->tls_skip_cn_check_}) {
    if (s != nullptr)
      s->add_on_state_callback([this](bool) { this->mark_dirty_(); });
  }
  // Serial settings on the other hand are applied live.
  for (select::Select *s : {this->baud_rate_, this->parity_}) {
    if (s != nullptr)
      s->add_on_state_callback([this](size_t) { this->apply_uart_settings_(); });
  }

  this->apply_uart_settings_();
  this->executor_.set_uart(this);
  this->executor_.set_flow_control_pin(this->flow_control_pin_);

  this->configure_mqtt_();
  this->setup_complete_ = true;
}

void GbbDongle::configure_mqtt_() {
  const std::string host = this->mqtt_host_ != nullptr ? this->mqtt_host_->state : "";
  const std::string plant_id = this->plant_id_ != nullptr ? this->plant_id_->state : "";
  const std::string token = this->plant_token_ != nullptr ? this->plant_token_->state : "";
  const uint16_t port =
      this->mqtt_port_ != nullptr && this->mqtt_port_->has_state() ? (uint16_t) this->mqtt_port_->state : 8883;

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

  this->topic_from_device_ = plant_id + "/ModbusInMqtt/fromDevice";
  this->topic_keepalive_ = plant_id + "/keepalive";
  this->mqtt_->subscribe(
      plant_id + "/ModbusInMqtt/toDevice",
      [this](const std::string &topic, const std::string &payload) { this->on_cloud_message_(payload); }, 1);

  this->cloud_configured_ = true;
  if (this->cloud_enabled_ == nullptr || this->cloud_enabled_->state) {
    ESP_LOGI(TAG, "Connecting to GbbOptimizer at %s:%u as plant '%s' (TLS: %s)", host.c_str(), port, plant_id.c_str(),
             tls && this->ca_certificate_ != nullptr ? "yes" : "NO");
    this->mqtt_->enable();
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
  this->executor_.set_baud_rate_hint(baud);

  const char *parity = "None";
  if (this->parity_ != nullptr && this->parity_->has_state())
    parity = this->parity_->current_option().c_str();
  if (str_equals_case_insensitive(parity, "Even")) {
    uart->set_parity(uart::UART_CONFIG_PARITY_EVEN);
  } else if (str_equals_case_insensitive(parity, "Odd")) {
    uart->set_parity(uart::UART_CONFIG_PARITY_ODD);
  } else {
    uart->set_parity(uart::UART_CONFIG_PARITY_NONE);
  }
  uart->load_settings(false);
  ESP_LOGI(TAG, "RS485: %" PRIu32 " baud, parity %s", baud, parity);
}

void GbbDongle::mark_dirty_() {
  if (!this->setup_complete_)
    return;
  this->settings_dirty_ = true;
  ESP_LOGI(TAG, "Cloud settings changed; restart the device to apply them");
}

void GbbDongle::on_cloud_message_(const std::string &payload) {
  this->requests_received_++;
  ESP_LOGD(TAG, "toDevice message (%u B)", payload.size());

  GbbHeader header;
  if (!parse_header(payload, header)) {
    // GbbConnect2 ignores unparseable/null messages.
    ESP_LOGW(TAG, "Ignoring malformed toDevice payload");
    return;
  }

  if (header.has_log_level)
    this->apply_log_level_(header.log_level);
  if (header.has_sub_inverter_sn) {
    // Single RS485 bus: the slave address inside each RTU frame already
    // routes to the right inverter, so SubInverterSN needs no special
    // handling here (GbbConnect2 used it to pick a different TCP dongle).
    ESP_LOGD(TAG, "SubInverterSN '%s' requested; executing on the local bus", header.sub_inverter_sn.c_str());
  }

  if (this->pending_request_.has_value()) {
    ESP_LOGW(TAG, "Request queue full; replacing queued request with the newer one");
  }
  this->pending_request_ = std::move(header);
}

void GbbDongle::apply_log_level_(const std::string &level) {
  uint8_t gate;
  if (str_equals_case_insensitive(level, "OnlyErrors")) {
    gate = ESPHOME_LOG_LEVEL_WARN;
  } else if (str_equals_case_insensitive(level, "Min")) {
    gate = ESPHOME_LOG_LEVEL_DEBUG;
  } else if (str_equals_case_insensitive(level, "Max")) {
    gate = ESPHOME_LOG_LEVEL_VERY_VERBOSE;
  } else {
    ESP_LOGW(TAG, "Unknown LogLevel '%s'", level.c_str());
    return;
  }
  if (gate != this->log_buffer_.get_level_gate()) {
    this->log_buffer_.set_level_gate(gate);
    this->log_level_pref_.save(&gate);
    ESP_LOGI(TAG, "LogLevel set to %s", level.c_str());
  }
}

void GbbDongle::publish_response_(GbbHeader &&header) {
  std::string last_log;
  const std::string *last_log_ptr = nullptr;
  if (header.has_send_last_log && header.send_last_log != 0) {
    last_log = this->log_buffer_.read_incremental(LAST_LOG_MAX_BYTES);
    last_log_ptr = &last_log;
  }
  const std::string response = build_response(header, this->version_, this->environment_, last_log_ptr);
  if (!this->mqtt_->publish(this->topic_from_device_, response.data(), response.size(), 2, false)) {
    ESP_LOGW(TAG, "Failed to publish fromDevice response (%u B)", response.size());
  }
  this->requests_handled_++;
}

void GbbDongle::loop() {
  if (this->executor_.has_result()) {
    this->publish_response_(this->executor_.take_result());
  }
  if (!this->executor_.busy() && this->pending_request_.has_value()) {
    this->executor_.start(std::move(*this->pending_request_));
    this->pending_request_.reset();
  }
  this->executor_.loop();

  if (this->cloud_configured_ && this->mqtt_->is_connected()) {
    const uint32_t now = millis();
    if (now - this->last_keepalive_ >= KEEPALIVE_INTERVAL_MS) {
      this->last_keepalive_ = now;
      this->mqtt_->publish(this->topic_keepalive_, "", 0, 1, false);
      ESP_LOGD(TAG, "Keepalive sent");
    }
  }
}

void GbbDongle::dump_config() {
  ESP_LOGCONFIG(TAG, "GbbDongle:");
  ESP_LOGCONFIG(TAG, "  Version: %s (%s)", this->version_, this->environment_);
  ESP_LOGCONFIG(TAG, "  Cloud configured: %s", YESNO(this->cloud_configured_));
  ESP_LOGCONFIG(TAG, "  CA certificate compiled in: %s", YESNO(this->ca_certificate_ != nullptr));
  ESP_LOGCONFIG(TAG, "  Log buffer: %" PRIu32 " B", this->log_buffer_size_);
  if (this->flow_control_pin_ != nullptr)
    LOG_PIN("  Flow control pin: ", this->flow_control_pin_);
}

}  // namespace gbb_dongle
}  // namespace esphome
