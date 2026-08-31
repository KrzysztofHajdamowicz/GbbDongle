#include "emergency_store.h"

namespace esphome {
namespace gbb_dongle {

static uint32_t fnv1_hash_local(const std::string &value) {
  uint32_t hash = 2166136261UL;
  for (const char raw_byte : value) {
    const auto byte = static_cast<unsigned char>(raw_byte);
    hash *= 16777619UL;
    hash ^= byte;
  }
  return hash;
}

// Only LineNo and the Modbus payload affect emergency execution; Tag and
// Timestamp are ignored so an hourly re-send with fresh timestamps still
// counts as unchanged.
static bool same_execution_content(const std::vector<GbbLine> &a, const std::vector<GbbLine> &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); i++) {
    if (a[i].line_no != b[i].line_no || a[i].has_modbus != b[i].has_modbus || a[i].modbus != b[i].modbus)
      return false;
  }
  return true;
}

bool EmergencyStore::set_lines(const std::string &sub_inverter_sn, std::vector<GbbLine> &&lines) {
  if (lines.empty()) {
    if (this->sets_.find(sub_inverter_sn) == this->sets_.end())
      return false;
    this->clear(sub_inverter_sn);
    return true;
  }
  auto it = this->sets_.find(sub_inverter_sn);
  if (it != this->sets_.end() && same_execution_content(it->second, lines))
    return false;
  this->sets_[sub_inverter_sn] = std::move(lines);
  this->revisions_[sub_inverter_sn] = ++this->next_revision_;
  return true;
}

void EmergencyStore::clear(const std::string &sub_inverter_sn) {
  this->sets_.erase(sub_inverter_sn);
  this->revisions_.erase(sub_inverter_sn);
}

void EmergencyStore::clear_all() {
  this->sets_.clear();
  this->revisions_.clear();
}

uint32_t EmergencyStore::revision(const std::string &sub_inverter_sn) const {
  auto it = this->revisions_.find(sub_inverter_sn);
  return it != this->revisions_.end() ? it->second : 0;
}

void EmergencyStore::set_persist_enabled(bool enabled) {
  if (enabled == this->persist_enabled_)
    return;
  this->persist_enabled_ = enabled;
  if (enabled) {
    this->persisted_hash_ = 0;  // force a write of the current content
    this->sync();
  } else {
    if (this->blob_store_ != nullptr)
      this->blob_store_->erase();
    this->persisted_hash_ = 0;
  }
}

bool EmergencyStore::load() {
  if (this->blob_store_ == nullptr)
    return false;
  std::string blob;
  if (!this->blob_store_->load(blob) || blob.empty())
    return false;
  if (!parse_emergency_sets(blob, this->sets_)) {
    this->sets_.clear();
    this->blob_store_->erase();
    return false;
  }
  this->persisted_hash_ = fnv1_hash_local(blob);
  for (const auto &entry : this->sets_) {
    this->revisions_[entry.first] = ++this->next_revision_;
  }
  return !this->sets_.empty();
}

void EmergencyStore::sync() {
  if (!this->persist_enabled_ || this->blob_store_ == nullptr)
    return;
  if (this->sets_.empty()) {
    if (this->persisted_hash_ != 0) {
      this->blob_store_->erase();
      this->persisted_hash_ = 0;
    }
    return;
  }
  const GbbJsonResult encoded = build_emergency_sets(this->sets_);
  if (encoded.overflow)
    return;
  const uint32_t hash = fnv1_hash_local(encoded.payload);
  if (hash == this->persisted_hash_)
    return;
  if (!this->blob_store_->save(encoded.payload))
    return;
  this->persisted_hash_ = hash;
}

}  // namespace gbb_dongle
}  // namespace esphome
