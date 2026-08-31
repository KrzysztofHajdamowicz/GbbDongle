#include "dongle_core.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>

namespace esphome {
namespace gbb_dongle {

static const char *const TAG = "gbb_dongle";
static constexpr uint32_t KEEPALIVE_INTERVAL_MS = 60 * 1000;
static constexpr uint32_t EMERGENCY_CHECK_INTERVAL_MS = 5 * 1000;
static constexpr size_t LAST_LOG_MAX_BYTES = 3 * 1024;
static const char *const CLIENT_NAME = "GbbDongle";
static constexpr uint8_t LOG_LEVEL_WARN = 2;
static constexpr uint8_t LOG_LEVEL_DEBUG = 5;
static constexpr uint8_t LOG_LEVEL_VERY_VERBOSE = 7;

static std::string trim_copy(const std::string &value) {
  const char *whitespace = " \t\r\n\f\v";
  const size_t begin = value.find_first_not_of(whitespace);
  if (begin == std::string::npos)
    return {};
  return value.substr(begin, value.find_last_not_of(whitespace) - begin + 1);
}

static bool equals_case_insensitive(const std::string &left, const char *right) {
  size_t index = 0;
  while (index < left.size() && right[index] != '\0') {
    char a = left[index];
    char b = right[index];
    if (a >= 'A' && a <= 'Z')
      a = static_cast<char>(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z')
      b = static_cast<char>(b - 'A' + 'a');
    if (a != b)
      return false;
    index++;
  }
  return index == left.size() && right[index] == '\0';
}

void DongleCore::log_(CoreLogLevel level, const char *format, ...) const {
  if (this->logger_ == nullptr)
    return;
  char message[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  this->logger_->core_log(level, TAG, message);
}

void DongleCore::set_clock(CoreClock *clock) {
  this->clock_ = clock;
  this->executor_.set_clock(clock);
}

void DongleCore::set_modbus_port(ModbusPort *port) { this->executor_.set_port(port); }

void DongleCore::set_logger(CoreLogger *logger) {
  this->logger_ = logger;
  this->executor_.set_logger(logger);
}

void DongleCore::set_identity(std::string version, std::string environment, std::string client_environment) {
  this->version_ = std::move(version);
  this->environment_ = std::move(environment);
  this->client_environment_ = std::move(client_environment);
}

void DongleCore::set_topics(std::string from_device, std::string keepalive) {
  this->topic_from_device_ = std::move(from_device);
  this->topic_keepalive_ = std::move(keepalive);
}

bool DongleCore::restore_emergency() {
  if (!this->emergency_store_.load())
    return false;
  this->boot_loaded_awaiting_time_ = true;
  this->emergency_state_ = EmergencyState::ARMED;
  return true;
}

void DongleCore::on_cloud_message(const std::string &payload) {
  this->requests_received_++;
  this->log_(CoreLogLevel::DEBUG, "Cloud -> device: received a request (%u B): %s",
             static_cast<unsigned>(payload.size()), payload.c_str());
  GbbHeader header;
  if (!parse_header(payload, header)) {
    this->log_(CoreLogLevel::WARN, "Ignoring malformed toDevice payload");
    return;
  }
  if (header.has_log_level)
    this->apply_log_level_(header.log_level);
  this->handle_emergency_fields_(header);
  if (header.has_sub_inverter_sn)
    this->log_(CoreLogLevel::DEBUG, "SubInverterSN '%s' requested; executing on the local bus",
               header.sub_inverter_sn.c_str());
  if (this->pending_request_.has_value())
    this->log_(CoreLogLevel::WARN, "Request queue full; replacing queued request with the newer one");
  this->pending_request_ = std::move(header);
}

void DongleCore::apply_log_level_(const std::string &level) {
  uint8_t gate;
  if (equals_case_insensitive(level, "OnlyErrors"))
    gate = LOG_LEVEL_WARN;
  else if (equals_case_insensitive(level, "Min"))
    gate = LOG_LEVEL_DEBUG;
  else if (equals_case_insensitive(level, "Max"))
    gate = LOG_LEVEL_VERY_VERBOSE;
  else {
    this->log_(CoreLogLevel::WARN, "Unknown LogLevel '%s'", level.c_str());
    return;
  }
  if (gate != this->log_buffer_.get_level_gate()) {
    this->log_buffer_.set_level_gate(gate);
    if (this->log_level_changed_)
      this->log_level_changed_(gate);
    this->log_(CoreLogLevel::INFO, "LogLevel set to %s", level.c_str());
  }
}

void DongleCore::publish_response_(GbbHeader &&header) {
  std::string last_log;
  const std::string *last_log_ptr = nullptr;
  if (header.has_send_last_log && header.send_last_log != 0) {
    last_log = this->log_buffer_.read_incremental(LAST_LOG_MAX_BYTES);
    last_log_ptr = &last_log;
  }
  const GbbClientIdentity identity{this->version_.c_str(), this->environment_.c_str(), CLIENT_NAME,
                                   this->client_environment_.c_str()};
  const std::string client_info = this->client_info_provider_ ? this->client_info_provider_() : std::string{};
  GbbJsonResult response = build_response(header, identity, client_info, last_log_ptr);
  if (response.overflow && last_log_ptr != nullptr) {
    this->log_(CoreLogLevel::ERROR, "Response with LastLog exceeded the JSON size cap; sending it without the log");
    response = build_response(header, identity, client_info, nullptr);
  }
  if (response.overflow) {
    this->log_(CoreLogLevel::ERROR, "Response exceeds the JSON size cap even without LastLog; dropping it");
    return;
  }
  this->log_(CoreLogLevel::DEBUG, "Device -> cloud: sending the response back (%u B, %u line(s)%s)",
             static_cast<unsigned>(response.payload.size()), static_cast<unsigned>(header.lines.size()),
             last_log_ptr != nullptr ? ", incl. recent log" : "");
  if (!this->cloud_->cloud_publish(this->topic_from_device_, response.payload.data(), response.payload.size(), 2,
                                   false))
    this->log_(CoreLogLevel::WARN, "Failed to publish fromDevice response (%u B)",
               static_cast<unsigned>(response.payload.size()));
  this->requests_handled_++;
}

void DongleCore::loop() {
  if (this->executor_.has_result()) {
    GbbHeader result = this->executor_.take_result();
    if (result.emergency)
      this->handle_emergency_result_(std::move(result));
    else
      this->publish_response_(std::move(result));
  }
  if (!this->executor_.busy() && this->pending_request_.has_value()) {
    this->executor_.start(std::move(*this->pending_request_));
    this->pending_request_.reset();
  } else if (!this->executor_.busy() && !this->pending_request_.has_value() &&
             this->emergency_state_ == EmergencyState::QUEUED) {
    this->start_next_emergency_set_();
  }
  this->executor_.loop();

  const uint32_t now = this->clock_->monotonic_ms();
  if (this->cloud_configured_ && this->cloud_->cloud_connected() &&
      now - this->last_keepalive_ >= KEEPALIVE_INTERVAL_MS) {
    this->last_keepalive_ = now;
    this->cloud_->cloud_publish(this->topic_keepalive_, "", 0, 1, false);
    this->log_(CoreLogLevel::DEBUG, "Keepalive sent");
  }
  if (now - this->last_emergency_check_ >= EMERGENCY_CHECK_INTERVAL_MS) {
    this->last_emergency_check_ = now;
    this->check_emergency_trigger_();
  }
}

void DongleCore::handle_emergency_fields_(GbbHeader &header) {
  if (header.has_is_inv_setup && header.is_inv_setup != 0) {
    time_t timestamp;
    if (this->clock_->wall_time(timestamp)) {
      this->last_inv_setup_ts_ = timestamp;
      this->inv_setup_awaiting_time_ = false;
    } else {
      this->log_(CoreLogLevel::WARN, "InvSetup received before the clock synced; the emergency check arms on the first sync");
      this->inv_setup_awaiting_time_ = true;
    }
    this->boot_loaded_awaiting_time_ = false;
    switch (this->emergency_state_) {
      case EmergencyState::QUEUED:
      case EmergencyState::BACKOFF:
        this->log_(CoreLogLevel::INFO, "InvSetup received; cancelling the pending emergency send");
        this->emergency_state_ = EmergencyState::ARMED;
        break;
      case EmergencyState::EXECUTING:
        this->log_(CoreLogLevel::INFO, "InvSetup received; aborting the in-flight emergency send");
        this->emergency_cancel_ = true;
        this->executor_.abort_pending_lines();
        break;
      default:
        break;
    }
  }

  if (header.has_lines_on_no_inv_setup) {
    const std::string key = header.has_sub_inverter_sn ? trim_copy(header.sub_inverter_sn) : "";
    const char *target = key.empty() ? "master" : key.c_str();
    const size_t count = header.lines_on_no_inv_setup.size();
    if (this->emergency_store_.set_lines(key, std::move(header.lines_on_no_inv_setup))) {
      this->emergency_store_.sync();
      if (count > 0)
        this->log_(CoreLogLevel::INFO, "Stored emergency command set for %s (%u line(s))", target,
                   static_cast<unsigned>(count));
      else
        this->log_(CoreLogLevel::INFO, "Cleared emergency command set for %s", target);
    } else {
      this->log_(CoreLogLevel::DEBUG, "Emergency command set for %s unchanged", target);
    }
    if (this->emergency_store_.empty()) {
      if (this->emergency_state_ != EmergencyState::EXECUTING)
        this->emergency_state_ = EmergencyState::EMPTY;
    } else if (this->emergency_state_ == EmergencyState::EMPTY) {
      this->emergency_state_ = EmergencyState::ARMED;
    }
  }
}

void DongleCore::check_emergency_trigger_() {
  if (this->emergency_state_ == EmergencyState::BACKOFF) {
    if ((int32_t) (this->clock_->monotonic_ms() - this->emergency_retry_at_) >= 0) {
      this->log_(CoreLogLevel::INFO, "Retrying undelivered emergency command set(s)");
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
  time_t timestamp;
  if (!this->clock_->wall_time(timestamp))
    return;
  if (this->inv_setup_awaiting_time_ || this->boot_loaded_awaiting_time_) {
    this->log_(CoreLogLevel::INFO, "Clock synced; hourly emergency check armed (%s)",
               this->inv_setup_awaiting_time_ ? "InvSetup preceded the sync" : "sets restored from NVS");
    this->inv_setup_awaiting_time_ = false;
    this->boot_loaded_awaiting_time_ = false;
    this->last_inv_setup_ts_ = timestamp;
    return;
  }
  const int minute = static_cast<int>((timestamp % 3600) / 60);
  const time_t top_of_hour = timestamp - (timestamp % 3600);
  if (minute > this->emergency_minute_threshold_ && this->last_inv_setup_ts_ != 0 &&
      this->last_inv_setup_ts_ < top_of_hour) {
    this->log_(CoreLogLevel::WARN, "No InvSetup from GbbOptimizer this hour; sending the emergency command set(s)");
    this->emergency_retry_delay_ms_ = this->emergency_retry_initial_ms_;
    this->emergency_walk_from_start_ = true;
    this->emergency_state_ = EmergencyState::QUEUED;
  }
}

void DongleCore::start_next_emergency_set_() {
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
  header.lines = it->second;
  this->emergency_runs_++;
  this->log_(CoreLogLevel::WARN, "Executing emergency command set for %s (%u line(s))",
             it->first.empty() ? "master" : it->first.c_str(), static_cast<unsigned>(header.lines.size()));
  this->executor_.start(std::move(header));
  this->emergency_state_ = EmergencyState::EXECUTING;
}

void DongleCore::handle_emergency_result_(GbbHeader &&header) {
  if (this->emergency_cancel_) {
    this->emergency_cancel_ = false;
    this->log_(CoreLogLevel::INFO, "Emergency run cancelled (InvSetup arrived); dropping the result");
    this->emergency_state_ = this->emergency_store_.empty() ? EmergencyState::EMPTY : EmergencyState::ARMED;
    return;
  }
  const std::string key = this->current_emergency_key_;
  const char *target = key.empty() ? "master" : key.c_str();
  bool delivered = false;
  for (const auto &line : header.lines) {
    if (!line.error.empty())
      this->log_(CoreLogLevel::ERROR, "Emergency %s: LineNo=%d: %s", target, static_cast<int>(line.line_no),
                 line.error.c_str());
    if (line.error.empty() && line.has_modbus && !line.modbus.empty())
      delivered = true;
  }
  if (delivered) {
    this->emergency_delivered_++;
    if (this->emergency_store_.revision(key) == this->current_emergency_revision_) {
      this->emergency_store_.clear(key);
      this->emergency_store_.sync();
      this->log_(CoreLogLevel::INFO, "Emergency command set for %s delivered; cleared", target);
    } else {
      this->log_(CoreLogLevel::INFO,
                 "Emergency command set for %s delivered, but a newer revision arrived mid-run; keeping it", target);
    }
  } else {
    this->log_(CoreLogLevel::WARN, "Emergency command set for %s got no response from the inverter", target);
  }
  const auto &sets = this->emergency_store_.sets();
  if (sets.upper_bound(key) != sets.cend()) {
    this->emergency_state_ = EmergencyState::QUEUED;
    return;
  }
  if (sets.empty()) {
    this->last_inv_setup_ts_ = 0;
    this->emergency_state_ = EmergencyState::EMPTY;
    this->log_(CoreLogLevel::INFO, "All emergency command sets delivered");
    return;
  }
  this->emergency_state_ = EmergencyState::BACKOFF;
  this->emergency_retry_at_ = this->clock_->monotonic_ms() + this->emergency_retry_delay_ms_;
  this->log_(CoreLogLevel::WARN, "Undelivered emergency command set(s) remain; retrying in %u s",
             static_cast<unsigned>(this->emergency_retry_delay_ms_ / 1000));
  this->emergency_retry_delay_ms_ = std::min(this->emergency_retry_delay_ms_ * 2, this->emergency_retry_max_ms_);
}

}  // namespace gbb_dongle
}  // namespace esphome
