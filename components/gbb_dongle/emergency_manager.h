#pragma once

#include <cstdint>
#include <string>

#include "esphome/components/switch/switch.h"
#include "esphome/components/time/real_time_clock.h"

#include "emergency_store.h"
#include "gbb_protocol.h"
#include "modbus_executor.h"

namespace esphome {
namespace gbb_dongle {

/// Emergency ("last will") delivery lifecycle: the hourly InvSetup deadline,
/// the send cycle over the stored sets and the retry/backoff when the
/// inverter does not answer (see docs/protocol.md). Storage lives in
/// EmergencyStore; the bus is driven through the ModbusExecutor owned by
/// GbbDongle, which arbitrates it (a running send cycle first, then the
/// pending cloud request).
class EmergencyManager {
 public:
  // EMPTY: nothing stored. ARMED: sets stored, watching the hourly InvSetup
  // deadline. QUEUED: a send cycle is due (or between two sets of one),
  // waiting for the executor. EXECUTING: an emergency set is on the RS485
  // bus. BACKOFF: the inverter did not respond; waiting for the retry timer.
  // A cycle (QUEUED/EXECUTING) is committed: once triggered it runs over
  // every stored set in full, the cloud's commands only follow afterwards.
  enum class State : uint8_t { EMPTY, ARMED, QUEUED, EXECUTING, BACKOFF };

  void set_time_source(time::RealTimeClock *t) { this->time_source_ = t; }
  void set_persist_switch(switch_::Switch *s) { this->persist_ = s; }
  void set_executor(ModbusExecutor *e) { this->executor_ = e; }
  void set_minute_threshold(uint8_t minute) { this->minute_threshold_ = minute; }
  void set_retry_initial(uint32_t ms) { this->retry_initial_ms_ = ms; }
  void set_retry_max(uint32_t ms) { this->retry_max_ms_ = ms; }

  /// Call from GbbDongle::setup() (priority LATE: the persist switch has
  /// restored its NVS state by then).
  void setup();
  /// Hourly trigger and retry timer; rate-limited internally.
  void loop();
  /// Apply the IsInvSetup / LinesOnNoInvSetup fields of a cloud request.
  void handle_fields(GbbHeader &header);
  /// A send cycle is due (or mid-way) and waits for the bus.
  bool wants_bus() const { return this->state_ == State::QUEUED; }
  /// Hand the next stored set to the executor; the caller made sure the
  /// executor is idle.
  void start_next_set();
  /// Consume the executor result of an emergency run (header.emergency).
  void handle_result(GbbHeader &&header);

  State state() const { return this->state_; }
  size_t sets_stored() const { return this->store_.size(); }
  uint32_t runs() const { return this->runs_; }
  uint8_t minute_threshold() const { return this->minute_threshold_; }
  bool persist_enabled() const { return this->persist_ != nullptr && this->persist_->state; }

 protected:
  void check_trigger_();

  time::RealTimeClock *time_source_{nullptr};
  switch_::Switch *persist_{nullptr};
  ModbusExecutor *executor_{nullptr};
  EmergencyStore store_;
  State state_{State::EMPTY};
  uint8_t minute_threshold_{10};
  uint32_t retry_initial_ms_{60 * 1000};
  uint32_t retry_max_ms_{15 * 60 * 1000};
  time_t last_inv_setup_ts_{0};  // epoch UTC, 0 = no InvSetup seen (never fires)
  // Persisted sets restored on boot: stamp last_inv_setup_ts_ with the first
  // valid wall time so a reboot during an outage still fires next hour.
  bool boot_loaded_awaiting_time_{false};
  // IsInvSetup arrived before SNTP synced (MQTT can beat NTP after a power
  // cycle): stamp last_inv_setup_ts_ with the first valid wall time, else an
  // outage starting before the sync would leave the check disarmed forever.
  bool inv_setup_awaiting_time_{false};
  // InvSetup arrived while a send cycle was running: the cycle still
  // completes (the inverter must get the whole set), but an undelivered
  // remainder is not retried afterwards — the cloud is back in charge.
  bool cloud_back_{false};
  bool walk_from_start_{false};
  std::string current_key_;
  // Revision of the set handed to the executor; a delivered result may clear
  // the stored set only while these still match (a LinesOnNoInvSetup replace
  // mid-run — possible without IsInvSetup, so without the cancel flag — must
  // not be wiped by the stale run's success).
  uint32_t current_revision_{0};
  uint32_t retry_delay_ms_{60 * 1000};
  uint32_t retry_at_{0};
  uint32_t last_check_{0};
  uint32_t runs_{0};
  uint32_t delivered_{0};
};

}  // namespace gbb_dongle
}  // namespace esphome
