#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "gbb_protocol.h"

namespace esphome {
namespace gbb_dongle {

/// Holds the emergency ("last will") Modbus command sets received via
/// LinesOnNoInvSetup, keyed by SubInverterSN ("" = master), and optionally
/// persists them to NVS as a single JSON blob. Storage only — the trigger
/// and retry logic lives in GbbDongle.
class EmergencyStore {
 public:
  /// Replace the set for one SubInverterSN; an empty vector clears that key
  /// (GbbConnect2 replace semantics). Returns false when the execution-
  /// relevant content (LineNo + Modbus) is identical to what is stored — the
  /// cloud re-sends the same set hourly — in which case nothing is touched:
  /// no replacement, no revision bump, and the caller can skip sync_nvs().
  bool set_lines(const std::string &sub_inverter_sn, std::vector<GbbLine> &&lines);
  void clear(const std::string &sub_inverter_sn);
  void clear_all();

  bool empty() const { return this->sets_.empty(); }
  size_t size() const { return this->sets_.size(); }
  const std::map<std::string, std::vector<GbbLine>> &sets() const { return this->sets_; }

  /// Monotonic per-key revision, bumped on every store/replace (never reused,
  /// 0 = key absent). Lets an execution result prove the set it ran is still
  /// the stored one before clearing it.
  uint32_t revision(const std::string &sub_inverter_sn) const;

  /// ON: writes the current sets to NVS immediately; OFF: erases the NVS copy
  /// (the RAM copy stays live either way).
  void set_persist_enabled(bool enabled);
  /// Restore sets from NVS. Returns true if non-empty sets were loaded.
  /// A corrupt blob is erased and ignored.
  bool load_from_nvs();
  /// Write the sets to NVS if persistence is on and the serialized content
  /// changed since the last write (flash-wear guard); erases the key when the
  /// store is empty.
  void sync_nvs();

 protected:
  std::map<std::string, std::vector<GbbLine>> sets_;
  std::map<std::string, uint32_t> revisions_;  // values from next_revision_, never repeat
  uint32_t next_revision_{0};
  bool persist_enabled_{false};
  uint32_t persisted_hash_{0};  // fnv1_hash of the last blob written, 0 = none
};

}  // namespace gbb_dongle
}  // namespace esphome
