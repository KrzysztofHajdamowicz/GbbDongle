#include "emergency_manager.h"

#include <algorithm>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/time.h"

namespace esphome {
namespace gbb_dongle {

static const char *const TAG = "gbb_dongle";

static const uint32_t CHECK_INTERVAL_MS = 5 * 1000;

// .NET string.Trim() equivalent (GbbConnect2 trims SubInverterSN this way).
static std::string str_trim_copy(const std::string &s) {
  const char *ws = " \t\r\n\f\v";
  const size_t begin = s.find_first_not_of(ws);
  if (begin == std::string::npos)
    return "";
  const size_t end = s.find_last_not_of(ws);
  return s.substr(begin, end - begin + 1);
}

void EmergencyManager::setup() {
  if (this->persist_ == nullptr)
    return;
  this->persist_->add_on_state_callback([this](bool state) { this->store_.set_persist_enabled(state); });
  if (this->persist_->state) {
    this->store_.set_persist_enabled(true);
    if (this->store_.load_from_nvs()) {
      this->boot_loaded_awaiting_time_ = true;
      this->state_ = State::ARMED;
    }
  }
}

void EmergencyManager::loop() {
  const uint32_t now = millis();
  if (now - this->last_check_ >= CHECK_INTERVAL_MS) {
    this->last_check_ = now;
    this->check_trigger_();
  }
}

void EmergencyManager::handle_fields(GbbHeader &header) {
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
    switch (this->state_) {
      case State::BACKOFF:
        // Nothing of this attempt reached the inverter; the cloud's own
        // setup supersedes the retry.
        ESP_LOGI(TAG, "InvSetup received; cancelling the pending emergency send");
        this->state_ = State::ARMED;
        break;
      case State::QUEUED:
      case State::EXECUTING:
        // GbbOptimizer semantics: an emergency set, once its send started,
        // goes out in full and only then gets overwritten by the cloud's
        // commands. Interrupting here would leave the inverter with a
        // half-applied state, so the cycle runs on; the cloud request waits.
        ESP_LOGI(TAG, "InvSetup received during the emergency send; finishing the set(s) first");
        this->cloud_back_ = true;
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
    const bool changed = this->store_.set_lines(key, std::move(header.lines_on_no_inv_setup));
    if (changed) {
      this->store_.sync_nvs();
      if (count > 0) {
        ESP_LOGI(TAG, "Stored emergency command set for %s (%u line(s))", target, count);
      } else {
        ESP_LOGI(TAG, "Cleared emergency command set for %s", target);
      }
    } else {
      ESP_LOGD(TAG, "Emergency command set for %s unchanged", target);
    }
    if (this->store_.empty()) {
      if (this->state_ != State::EXECUTING)
        this->state_ = State::EMPTY;
    } else if (this->state_ == State::EMPTY) {
      this->state_ = State::ARMED;
    }
  }
}

void EmergencyManager::check_trigger_() {
  if (this->state_ == State::BACKOFF) {
    if ((int32_t) (millis() - this->retry_at_) >= 0) {
      ESP_LOGI(TAG, "Retrying undelivered emergency command set(s)");
      this->walk_from_start_ = true;
      this->state_ = State::QUEUED;
    }
    return;
  }
  if (this->state_ != State::ARMED)
    return;
  if (this->store_.empty()) {
    this->state_ = State::EMPTY;
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
  if (minute > this->minute_threshold_ && this->last_inv_setup_ts_ != 0 && this->last_inv_setup_ts_ < top_of_hour) {
    ESP_LOGW(TAG, "No InvSetup from GbbOptimizer this hour; sending the emergency command set(s)");
    this->retry_delay_ms_ = this->retry_initial_ms_;
    this->walk_from_start_ = true;
    this->state_ = State::QUEUED;
  }
}

void EmergencyManager::start_next_set() {
  const auto &sets = this->store_.sets();
  auto it = this->walk_from_start_ ? sets.cbegin() : sets.upper_bound(this->current_key_);
  if (it == sets.cend()) {
    this->cloud_back_ = false;
    this->state_ = sets.empty() ? State::EMPTY : State::ARMED;
    return;
  }
  if (this->executor_ == nullptr)
    return;
  this->walk_from_start_ = false;
  this->current_key_ = it->first;
  this->current_revision_ = this->store_.revision(it->first);

  GbbHeader header;
  header.emergency = true;
  if (!it->first.empty()) {
    header.has_sub_inverter_sn = true;
    header.sub_inverter_sn = it->first;
  }
  // Copy, not move: the stored set survives until delivery is confirmed.
  header.lines = it->second;
  this->runs_++;
  ESP_LOGW(TAG, "Executing emergency command set for %s (%u line(s))",
           it->first.empty() ? "master" : it->first.c_str(), header.lines.size());
  this->executor_->start(std::move(header));
  this->state_ = State::EXECUTING;
}

void EmergencyManager::handle_result(GbbHeader &&header) {
  const std::string key = this->current_key_;
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
    this->delivered_++;
    if (this->store_.revision(key) == this->current_revision_) {
      this->store_.clear(key);
      this->store_.sync_nvs();
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

  const auto &sets = this->store_.sets();
  if (sets.upper_bound(key) != sets.cend()) {
    this->state_ = State::QUEUED;  // next set of this cycle
    return;
  }
  if (sets.empty()) {
    // Send once: stay quiet until GbbOptimizer delivers a new set.
    this->last_inv_setup_ts_ = 0;
    this->cloud_back_ = false;
    this->state_ = State::EMPTY;
    ESP_LOGI(TAG, "All emergency command sets delivered");
    return;
  }
  if (this->cloud_back_) {
    this->cloud_back_ = false;
    this->state_ = State::ARMED;
    ESP_LOGW(TAG, "Undelivered emergency command set(s) remain, but the cloud is back; not retrying");
    return;
  }
  this->state_ = State::BACKOFF;
  this->retry_at_ = millis() + this->retry_delay_ms_;
  ESP_LOGW(TAG, "Undelivered emergency command set(s) remain; retrying in %" PRIu32 " s", this->retry_delay_ms_ / 1000);
  this->retry_delay_ms_ = std::min(this->retry_delay_ms_ * 2, this->retry_max_ms_);
}

}  // namespace gbb_dongle
}  // namespace esphome
