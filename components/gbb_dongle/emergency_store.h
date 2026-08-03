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
  /// (GbbConnect2 replace semantics). Call sync_nvs() afterwards.
  void set_lines(const std::string &sub_inverter_sn, std::vector<GbbLine> &&lines);
  void clear(const std::string &sub_inverter_sn);
  void clear_all();

  bool empty() const { return this->sets_.empty(); }
  size_t size() const { return this->sets_.size(); }
  const std::map<std::string, std::vector<GbbLine>> &sets() const { return this->sets_; }

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
  bool persist_enabled_{false};
  uint32_t persisted_hash_{0};  // fnv1_hash of the last blob written, 0 = none
};

}  // namespace gbb_dongle
}  // namespace esphome
