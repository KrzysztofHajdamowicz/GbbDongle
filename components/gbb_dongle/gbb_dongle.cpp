#include "gbb_dongle.h"

#include <algorithm>
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

static const uint32_t KEEPALIVE_INTERVAL_MS = 60 * 1000;
static const uint32_t EMERGENCY_CHECK_INTERVAL_MS = 5 * 1000;
// Budgeted against json::build_json()'s 5120 B truncation cap: 3 KiB of raw
// log with ~1.3x JSON escaping plus the ~0.5 KiB rest of the response stays
// safely below it. Steady-state increments between the cloud's ~1/min
// SendLastLog polls are ~1-1.5 KiB, so this only shortens the backlog
// served after a connectivity gap.
static const size_t LAST_LOG_MAX_BYTES = 3 * 1024;

// Product identity for the fromDevice ClientName field — not configurable,
// it names this firmware regardless of board or transport.
static const char *const CLIENT_NAME = "GbbDongle";

// .NET string.Trim() equivalent (GbbConnect2 trims SubInverterSN this way).
static std::string str_trim_copy(const std::string &s) {
  const char *ws = " \t\r\n\f\v";
  const size_t begin = s.find_first_not_of(ws);
  if (begin == std::string::npos)
    return "";
  const size_t end = s.find_last_not_of(ws);
  return s.substr(begin, end - begin + 1);
}

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

  if (this->emergency_persist_ != nullptr) {
    // LATE priority: the switch has restored its NVS state by now.
    this->emergency_persist_->add_on_state_callback(
        [this](bool state) { this->emergency_store_.set_persist_enabled(state); });
    if (this->emergency_persist_->state) {
      this->emergency_store_.set_persist_enabled(true);
      if (this->emergency_store_.load_from_nvs()) {
        this->boot_loaded_awaiting_time_ = true;
        this->emergency_state_ = EmergencyState::ARMED;
      }
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
    // Defer the actual enable() until the network is up: setup() runs before
    // WiFi finishes associating, so connecting here just fails DNS with -6 and
    // spams warnings until the first retry. loop() flips this on once
    // network::is_connected() returns true.
    ESP_LOGI(TAG, "Cloud configured for GbbOptimizer at %s:%u as plant '%s' (TLS: %s); connecting once WiFi is up",
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
  // Full request as it arrived from the cloud, so on the bench you can see
  // exactly what was asked for and match it against the Modbus frames the
  // executor sends out below.
  ESP_LOGD(TAG, "Cloud -> device: received a request (%u B): %s", payload.size(), payload.c_str());

  GbbHeader header;
  if (!parse_header(payload, header)) {
    // GbbConnect2 ignores unparseable/null messages.
    ESP_LOGW(TAG, "Ignoring malformed toDevice payload");
    return;
  }

  if (header.has_log_level)
    this->apply_log_level_(header.log_level);
  this->handle_emergency_fields_(header);
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

std::string GbbDongle::build_client_info_() const {
  std::string info;
  const auto append = [&info](const std::string &segment) {
    if (!info.empty())
      info += ", ";
    info += segment;
  };
  if (this->wifi_signal_db_ != nullptr && this->wifi_signal_percent_ != nullptr) {
    if (this->wifi_signal_db_->has_state() && this->wifi_signal_percent_->has_state() &&
        !std::isnan(this->wifi_signal_db_->state) && !std::isnan(this->wifi_signal_percent_->state)) {
      append(str_sprintf("Wi-Fi %.0f%% (%.0fdBm)", this->wifi_signal_percent_->state, this->wifi_signal_db_->state));
    }
  } else {
    // The only boards without the WiFi signal sensors wired in are the
    // Ethernet-only ones (see common/wifi.yaml vs boards/kamami-*.yaml).
    append("Ethernet");
  }
  if (this->ip_address_ != nullptr && this->ip_address_->has_state())
    append("IP " + this->ip_address_->state);
  if (this->uptime_text_ != nullptr && this->uptime_text_->has_state())
    append("uptime " + this->uptime_text_->state);
  return info;
}

void GbbDongle::publish_response_(GbbHeader &&header) {
  std::string last_log;
  const std::string *last_log_ptr = nullptr;
  if (header.has_send_last_log && header.send_last_log != 0) {
    last_log = this->log_buffer_.read_incremental(LAST_LOG_MAX_BYTES);
    last_log_ptr = &last_log;
  }
  const GbbClientIdentity identity{this->version_, this->environment_, CLIENT_NAME, this->client_environment_};
  std::string response = build_response(header, identity, this->build_client_info_(), last_log_ptr);
  if (response.size() >= JSON_BUILD_TRUNCATED_SIZE && last_log_ptr != nullptr) {
    // build_json truncated the document (invalid JSON). The log is the only
    // sizeable payload, so drop it and answer with a valid response instead;
    // the dropped lines stay lost (the ring's read cursor already advanced).
    ESP_LOGE(TAG, "Response with LastLog exceeded the JSON size cap; sending it without the log");
    response = build_response(header, identity, this->build_client_info_(), nullptr);
  }
  if (response.size() >= JSON_BUILD_TRUNCATED_SIZE) {
    ESP_LOGE(TAG, "Response exceeds the JSON size cap even without LastLog; dropping it");
    return;
  }
  // Summary rather than the whole JSON: with SendLastLog the response also
  // carries up to 8 KB of recent log lines, and the per-line results are
  // already visible in the executor's "<- inverter" log lines above.
  ESP_LOGD(TAG, "Device -> cloud: sending the response back (%u B, %u line(s)%s)", response.size(),
           header.lines.size(), last_log_ptr != nullptr ? ", incl. recent log" : "");
  if (!this->mqtt_->publish(this->topic_from_device_, response.data(), response.size(), 2, false)) {
    ESP_LOGW(TAG, "Failed to publish fromDevice response (%u B)", response.size());
  }
  this->requests_handled_++;
}

void GbbDongle::loop() {
  if (this->cloud_enable_pending_ && network::is_connected()) {
    this->cloud_enable_pending_ = false;
    ESP_LOGI(TAG, "Network is up; connecting to the cloud now");
    this->mqtt_->enable();
  }

  if (this->executor_.has_result()) {
    GbbHeader result = this->executor_.take_result();
    if (result.emergency) {
      this->handle_emergency_result_(std::move(result));
    } else {
      this->publish_response_(std::move(result));
    }
  }
  if (!this->executor_.busy() && this->pending_request_.has_value()) {
    this->executor_.start(std::move(*this->pending_request_));
    this->pending_request_.reset();
  } else if (!this->executor_.busy() && !this->pending_request_.has_value() &&
             this->emergency_state_ == EmergencyState::QUEUED) {
    // Cloud requests take priority; emergency sets go out only when idle.
    this->start_next_emergency_set_();
  }
  this->executor_.loop();

  const uint32_t now = millis();
  if (this->cloud_configured_ && this->mqtt_->is_connected()) {
    if (now - this->last_keepalive_ >= KEEPALIVE_INTERVAL_MS) {
      this->last_keepalive_ = now;
      this->mqtt_->publish(this->topic_keepalive_, "", 0, 1, false);
      ESP_LOGD(TAG, "Keepalive sent");
    }
  }

  if (now - this->last_emergency_check_ >= EMERGENCY_CHECK_INTERVAL_MS) {
    this->last_emergency_check_ = now;
    this->check_emergency_trigger_();
  }
}

void GbbDongle::handle_emergency_fields_(GbbHeader &header) {
  if (header.has_is_inv_setup && header.is_inv_setup != 0) {
    const ESPTime now = this->time_source_ != nullptr ? this->time_source_->now() : ESPTime{};
    if (now.is_valid()) {
      this->last_inv_setup_ts_ = now.timestamp;
      this->inv_setup_awaiting_time_ = false;
    } else {
      ESP_LOGW(TAG, "InvSetup received before the clock synced; the emergency check arms on the first sync");
      this->inv_setup_awaiting_time_ = true;
    }
    this->boot_loaded_awaiting_time_ = false;
    switch (this->emergency_state_) {
      case EmergencyState::QUEUED:
      case EmergencyState::BACKOFF:
        ESP_LOGI(TAG, "InvSetup received; cancelling the pending emergency send");
        this->emergency_state_ = EmergencyState::ARMED;
        break;
      case EmergencyState::EXECUTING:
        // The cloud is back: stop putting stale emergency lines on the bus
        // (the executor yields at the next safe line boundary; EXECUTING
        // always means the running batch is ours) and make sure the stale
        // result does not clear a freshly received set.
        ESP_LOGI(TAG, "InvSetup received; aborting the in-flight emergency send");
        this->emergency_cancel_ = true;
        this->executor_.abort_pending_lines();
        break;
      default:
        break;
    }
  }

  if (header.has_lines_on_no_inv_setup) {
    // GbbConnect2 matches SubInverterSN with .NET Trim() semantics.
    const std::string key = header.has_sub_inverter_sn ? str_trim_copy(header.sub_inverter_sn) : "";
    const char *target = key.empty() ? "master" : key.c_str();
    const size_t count = header.lines_on_no_inv_setup.size();
    const bool changed = this->emergency_store_.set_lines(key, std::move(header.lines_on_no_inv_setup));
    if (changed) {
      this->emergency_store_.sync_nvs();
      if (count > 0) {
        ESP_LOGI(TAG, "Stored emergency command set for %s (%u line(s))", target, count);
      } else {
        ESP_LOGI(TAG, "Cleared emergency command set for %s", target);
      }
    } else {
      ESP_LOGD(TAG, "Emergency command set for %s unchanged", target);
    }
    if (this->emergency_store_.empty()) {
      if (this->emergency_state_ != EmergencyState::EXECUTING)
        this->emergency_state_ = EmergencyState::EMPTY;
    } else if (this->emergency_state_ == EmergencyState::EMPTY) {
      this->emergency_state_ = EmergencyState::ARMED;
    }
  }
}

void GbbDongle::check_emergency_trigger_() {
  if (this->emergency_state_ == EmergencyState::BACKOFF) {
    if ((int32_t) (millis() - this->emergency_retry_at_) >= 0) {
      ESP_LOGI(TAG, "Retrying undelivered emergency command set(s)");
      this->emergency_walk_from_start_ = true;
      this->emergency_state_ = EmergencyState::QUEUED;
    }
    return;
  }
  if (this->emergency_state_ != EmergencyState::ARMED)
    return;
  if (this->emergency_store_.empty()) {
    this->emergency_state_ = EmergencyState::EMPTY;
    return;
  }
  if (this->time_source_ == nullptr)
    return;
  const ESPTime now = this->time_source_->now();
  if (!now.is_valid())
    return;
  const time_t ts = now.timestamp;
  if (this->inv_setup_awaiting_time_ || this->boot_loaded_awaiting_time_) {
    // The last InvSetup receive time is unknown (it arrived before the clock
    // synced, or the sets were restored from NVS after a reboot): approximate
    // it with the sync moment, so the hourly deadline counts from now. Late
    // stamping can only delay the trigger, never fire it early.
    ESP_LOGI(TAG, "Clock synced; hourly emergency check armed (%s)",
             this->inv_setup_awaiting_time_ ? "InvSetup preceded the sync" : "sets restored from NVS");
    this->inv_setup_awaiting_time_ = false;
    this->boot_loaded_awaiting_time_ = false;
    this->last_inv_setup_ts_ = ts;
    return;
  }
  // GbbConnect2 semantics: GbbOptimizer sends InvSetup during the first
  // <threshold> minutes of every hour; past that window with no InvSetup
  // this hour, the emergency sets go out.
  const int minute = (int) ((ts % 3600) / 60);
  const time_t top_of_hour = ts - (ts % 3600);
  if (minute > this->emergency_minute_threshold_ && this->last_inv_setup_ts_ != 0 &&
      this->last_inv_setup_ts_ < top_of_hour) {
    ESP_LOGW(TAG, "No InvSetup from GbbOptimizer this hour; sending the emergency command set(s)");
    this->emergency_retry_delay_ms_ = this->emergency_retry_initial_ms_;
    this->emergency_walk_from_start_ = true;
    this->emergency_state_ = EmergencyState::QUEUED;
  }
}

void GbbDongle::start_next_emergency_set_() {
  const auto &sets = this->emergency_store_.sets();
  auto it = this->emergency_walk_from_start_ ? sets.cbegin() : sets.upper_bound(this->current_emergency_key_);
  if (it == sets.cend()) {
    this->emergency_state_ = sets.empty() ? EmergencyState::EMPTY : EmergencyState::ARMED;
    return;
  }
  this->emergency_walk_from_start_ = false;
  this->current_emergency_key_ = it->first;
  this->current_emergency_revision_ = this->emergency_store_.revision(it->first);

  GbbHeader header;
  header.emergency = true;
  if (!it->first.empty()) {
    header.has_sub_inverter_sn = true;
    header.sub_inverter_sn = it->first;
  }
  // Copy, not move: the stored set survives until delivery is confirmed.
  header.lines = it->second;
  this->emergency_runs_++;
  ESP_LOGW(TAG, "Executing emergency command set for %s (%u line(s))",
           it->first.empty() ? "master" : it->first.c_str(), header.lines.size());
  this->executor_.start(std::move(header));
  this->emergency_state_ = EmergencyState::EXECUTING;
}

void GbbDongle::handle_emergency_result_(GbbHeader &&header) {
  if (this->emergency_cancel_) {
    this->emergency_cancel_ = false;
    ESP_LOGI(TAG, "Emergency run cancelled (InvSetup arrived); dropping the result");
    this->emergency_state_ = this->emergency_store_.empty() ? EmergencyState::EMPTY : EmergencyState::ARMED;
    return;
  }
  const std::string key = this->current_emergency_key_;
  const char *target = key.empty() ? "master" : key.c_str();

  // "Delivered" = the inverter answered at least one line (the executor
  // overwrites Modbus with the response frame and clears it on failure).
  bool delivered = false;
  for (const auto &line : header.lines) {
    if (!line.error.empty())
      ESP_LOGE(TAG, "Emergency %s: LineNo=%" PRId32 ": %s", target, line.line_no, line.error.c_str());
    if (line.error.empty() && line.has_modbus && !line.modbus.empty())
      delivered = true;
  }
  if (delivered) {
    this->emergency_delivered_++;
    if (this->emergency_store_.revision(key) == this->current_emergency_revision_) {
      this->emergency_store_.clear(key);
      this->emergency_store_.sync_nvs();
      ESP_LOGI(TAG, "Emergency command set for %s delivered; cleared", target);
    } else {
      // The set was replaced while this run was on the bus; the replacement
      // was never executed, so it must stay stored.
      ESP_LOGI(TAG, "Emergency command set for %s delivered, but a newer revision arrived mid-run; keeping it",
               target);
    }
  } else {
    ESP_LOGW(TAG, "Emergency command set for %s got no response from the inverter", target);
  }

  const auto &sets = this->emergency_store_.sets();
  if (sets.upper_bound(key) != sets.cend()) {
    this->emergency_state_ = EmergencyState::QUEUED;  // next set of this cycle
    return;
  }
  if (sets.empty()) {
    // Send once: stay quiet until GbbOptimizer delivers a new set.
    this->last_inv_setup_ts_ = 0;
    this->emergency_state_ = EmergencyState::EMPTY;
    ESP_LOGI(TAG, "All emergency command sets delivered");
    return;
  }
  this->emergency_state_ = EmergencyState::BACKOFF;
  this->emergency_retry_at_ = millis() + this->emergency_retry_delay_ms_;
  ESP_LOGW(TAG, "Undelivered emergency command set(s) remain; retrying in %" PRIu32 " s",
           this->emergency_retry_delay_ms_ / 1000);
  this->emergency_retry_delay_ms_ = std::min(this->emergency_retry_delay_ms_ * 2, this->emergency_retry_max_ms_);
}

void GbbDongle::dump_config() {
  ESP_LOGCONFIG(TAG, "GbbDongle:");
  ESP_LOGCONFIG(TAG, "  Version: %s (%s)", this->version_, this->environment_);
  ESP_LOGCONFIG(TAG, "  Cloud configured: %s", YESNO(this->cloud_configured_));
  ESP_LOGCONFIG(TAG, "  CA certificate compiled in: %s", YESNO(this->ca_certificate_ != nullptr));
  ESP_LOGCONFIG(TAG, "  Log buffer: %" PRIu32 " B", this->log_buffer_size_);
  ESP_LOGCONFIG(TAG, "  Emergency: threshold minute %u, %u set(s) stored, persist %s", this->emergency_minute_threshold_,
                this->emergency_store_.size(),
                ONOFF(this->emergency_persist_ != nullptr && this->emergency_persist_->state));
  if (this->flow_control_pin_ != nullptr)
    LOG_PIN("  Flow control pin: ", this->flow_control_pin_);
}

}  // namespace gbb_dongle
}  // namespace esphome
