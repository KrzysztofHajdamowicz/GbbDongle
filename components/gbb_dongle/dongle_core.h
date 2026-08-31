#pragma once

#include <functional>
#include <optional>
#include <string>

#include "core_interfaces.h"
#include "emergency_store.h"
#include "gbb_protocol.h"
#include "log_ring_buffer.h"
#include "modbus_executor.h"

namespace esphome {
namespace gbb_dongle {

class DongleCore {
 public:
  void set_clock(CoreClock *clock);
  void set_modbus_port(ModbusPort *port);
  void set_cloud_transport(CloudTransport *cloud) { this->cloud_ = cloud; }
  void set_logger(CoreLogger *logger);
  void set_blob_store(BlobStore *store) { this->emergency_store_.set_blob_store(store); }

  void set_identity(std::string version, std::string environment, std::string client_environment);
  void set_topics(std::string from_device, std::string keepalive);
  void set_client_info_provider(std::function<std::string()> provider) {
    this->client_info_provider_ = std::move(provider);
  }
  void set_log_level_changed(std::function<void(uint8_t)> callback) {
    this->log_level_changed_ = std::move(callback);
  }
  void set_cloud_configured(bool configured) { this->cloud_configured_ = configured; }
  void set_response_timeout(uint32_t ms) { this->executor_.set_response_timeout(ms); }
  void set_read_gap(uint32_t ms) { this->executor_.set_read_gap(ms); }
  void set_write_gap(uint32_t ms) { this->executor_.set_write_gap(ms); }
  void set_baud_rate_hint(uint32_t baud) { this->executor_.set_baud_rate_hint(baud); }
  void set_emergency_minute_threshold(uint8_t minute) { this->emergency_minute_threshold_ = minute; }
  void set_emergency_retry_initial(uint32_t ms) { this->emergency_retry_initial_ms_ = ms; }
  void set_emergency_retry_max(uint32_t ms) { this->emergency_retry_max_ms_ = ms; }

  LogRingBuffer &log_buffer() { return this->log_buffer_; }
  EmergencyStore &emergency_store() { return this->emergency_store_; }

  void set_persist_enabled(bool enabled) { this->emergency_store_.set_persist_enabled(enabled); }
  bool restore_emergency();
  void on_cloud_message(const std::string &payload);
  void loop();

  uint32_t get_requests_received() const { return this->requests_received_; }
  uint32_t get_requests_handled() const { return this->requests_handled_; }
  uint32_t get_modbus_errors() const { return this->executor_.get_error_count(); }
  uint32_t get_emergency_sets_stored() const { return static_cast<uint32_t>(this->emergency_store_.size()); }
  uint32_t get_emergency_runs() const { return this->emergency_runs_; }
  uint32_t get_emergency_delivered() const { return this->emergency_delivered_; }
  uint8_t get_emergency_minute_threshold() const { return this->emergency_minute_threshold_; }

 protected:
  enum class EmergencyState : uint8_t { EMPTY, ARMED, QUEUED, EXECUTING, BACKOFF };

  void apply_log_level_(const std::string &level);
  void publish_response_(GbbHeader &&header);
  void handle_emergency_fields_(GbbHeader &header);
  void check_emergency_trigger_();
  void start_next_emergency_set_();
  void handle_emergency_result_(GbbHeader &&header);
  void log_(CoreLogLevel level, const char *format, ...) const;

  CoreClock *clock_{nullptr};
  CloudTransport *cloud_{nullptr};
  CoreLogger *logger_{nullptr};
  ModbusExecutor executor_;
  LogRingBuffer log_buffer_;
  EmergencyStore emergency_store_;

  std::string version_{"dev"};
  std::string environment_{"GbbDongle"};
  std::string client_environment_{"GbbDongle"};
  std::string topic_from_device_;
  std::string topic_keepalive_;
  std::function<std::string()> client_info_provider_;
  std::function<void(uint8_t)> log_level_changed_;

  std::optional<GbbHeader> pending_request_;
  bool cloud_configured_{false};
  uint32_t last_keepalive_{0};
  uint32_t requests_received_{0};
  uint32_t requests_handled_{0};

  EmergencyState emergency_state_{EmergencyState::EMPTY};
  uint8_t emergency_minute_threshold_{10};
  uint32_t emergency_retry_initial_ms_{60 * 1000};
  uint32_t emergency_retry_max_ms_{15 * 60 * 1000};
  time_t last_inv_setup_ts_{0};
  bool boot_loaded_awaiting_time_{false};
  bool inv_setup_awaiting_time_{false};
  bool emergency_cancel_{false};
  bool emergency_walk_from_start_{false};
  std::string current_emergency_key_;
  uint32_t current_emergency_revision_{0};
  uint32_t emergency_retry_delay_ms_{60 * 1000};
  uint32_t emergency_retry_at_{0};
  uint32_t last_emergency_check_{0};
  uint32_t emergency_runs_{0};
  uint32_t emergency_delivered_{0};
};

}  // namespace gbb_dongle
}  // namespace esphome
