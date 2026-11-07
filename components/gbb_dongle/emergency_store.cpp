#include "emergency_store.h"

#include <nvs.h>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gbb_dongle {

static const char *const TAG = "gbb_dongle.emergency";

// Raw ESP-IDF NVS instead of ESPPreferenceObject: the blob is variable-size
// and can exceed 1 KB (several sets of long hex Modbus frames), while
// make_preference<T> only handles fixed-size types.
static const char *const NVS_NAMESPACE = "gbb_dongle";
static const char *const NVS_KEY = "emerg_sets";

static bool nvs_open_rw(nvs_handle_t &handle) {
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

static void nvs_erase_sets(nvs_handle_t handle) {
  esp_err_t err = nvs_erase_key(handle, NVS_KEY);
  if (err == ESP_OK)
    nvs_commit(handle);
  else if (err != ESP_ERR_NVS_NOT_FOUND)
    ESP_LOGW(TAG, "nvs_erase_key failed: %s", esp_err_to_name(err));
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

static bool has_executable_line(const std::vector<GbbLine> &lines) {
  for (const auto &line : lines) {
    if (line.has_modbus && !line.modbus.empty())
      return true;
  }
  return false;
}

bool EmergencyStore::set_lines(const std::string &sub_inverter_sn, std::vector<GbbLine> &&lines) {
  if (!lines.empty() && !has_executable_line(lines)) {
    // The executor skips lines without a Modbus frame, so such a set could
    // never be confirmed as delivered and would be retried forever.
    ESP_LOGW(TAG, "Emergency set without any Modbus frame; treating it as a clear");
    lines.clear();
  }
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
    this->sync_nvs();
  } else {
    nvs_handle_t handle;
    if (nvs_open_rw(handle)) {
      nvs_erase_sets(handle);
      nvs_close(handle);
    }
    this->persisted_hash_ = 0;
    ESP_LOGI(TAG, "Persistence disabled; NVS copy erased");
  }
}

bool EmergencyStore::load_from_nvs() {
  nvs_handle_t handle;
  if (!nvs_open_rw(handle))
    return false;
  size_t length = 0;
  esp_err_t err = nvs_get_blob(handle, NVS_KEY, nullptr, &length);
  if (err == ESP_ERR_NVS_NOT_FOUND || length == 0) {
    nvs_close(handle);
    return false;
  }
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "nvs_get_blob failed: %s", esp_err_to_name(err));
    nvs_close(handle);
    return false;
  }
  std::string blob;
  blob.resize(length);
  err = nvs_get_blob(handle, NVS_KEY, &blob[0], &length);
  if (err != ESP_OK) {
    // A failed read says nothing about the stored content; keep it for the
    // next boot instead of erasing it.
    ESP_LOGW(TAG, "nvs_get_blob failed: %s", esp_err_to_name(err));
    nvs_close(handle);
    return false;
  }
  if (!parse_emergency_sets(blob, this->sets_)) {
    ESP_LOGW(TAG, "Stored emergency sets unreadable; erasing");
    this->sets_.clear();
    nvs_erase_sets(handle);
    nvs_close(handle);
    return false;
  }
  nvs_close(handle);
  this->persisted_hash_ = fnv1_hash(blob);
  size_t total_lines = 0;
  for (const auto &entry : this->sets_) {
    this->revisions_[entry.first] = ++this->next_revision_;
    total_lines += entry.second.size();
  }
  ESP_LOGI(TAG, "Restored %u emergency set(s) (%u line(s)) from NVS", this->sets_.size(), total_lines);
  return !this->sets_.empty();
}

void EmergencyStore::sync_nvs() {
  if (!this->persist_enabled_)
    return;
  nvs_handle_t handle;
  if (!nvs_open_rw(handle))
    return;
  if (this->sets_.empty()) {
    if (this->persisted_hash_ != 0) {
      nvs_erase_sets(handle);
      this->persisted_hash_ = 0;
      ESP_LOGD(TAG, "Emergency sets cleared from NVS");
    }
    nvs_close(handle);
    return;
  }
  const std::string blob = build_emergency_sets(this->sets_);
  if (json_build_failed(blob)) {
    // Truncated or "{}": persisting it would store a blob that load_from_nvs()
    // rejects or that restores no sets. The previous NVS content is no longer
    // what RAM holds either (it may be a set that has since been replaced or
    // delivered and cleared), so erase it rather than let a reboot resurrect
    // it. The RAM copy stays complete and usable until the next reboot.
    ESP_LOGE(TAG, "Emergency sets could not be serialized (%u B, cap %u B); NVS copy erased, sets kept in RAM only",
             blob.size(), (unsigned) JSON_BUILD_TRUNCATED_SIZE);
    nvs_erase_sets(handle);
    this->persisted_hash_ = 0;
    nvs_close(handle);
    return;
  }
  const uint32_t hash = fnv1_hash(blob);
  if (hash == this->persisted_hash_) {
    // The cloud re-sends the same set every hour; skip identical writes to
    // limit flash wear.
    nvs_close(handle);
    return;
  }
  esp_err_t err = nvs_set_blob(handle, NVS_KEY, blob.data(), blob.size());
  if (err == ESP_OK)
    err = nvs_commit(handle);
  nvs_close(handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Persisting emergency sets failed: %s", esp_err_to_name(err));
    return;
  }
  this->persisted_hash_ = hash;
  ESP_LOGI(TAG, "Emergency sets persisted to NVS (%u B)", blob.size());
}

}  // namespace gbb_dongle
}  // namespace esphome
