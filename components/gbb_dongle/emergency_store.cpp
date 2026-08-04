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

void EmergencyStore::set_lines(const std::string &sub_inverter_sn, std::vector<GbbLine> &&lines) {
  if (lines.empty()) {
    this->clear(sub_inverter_sn);
    return;
  }
  this->sets_[sub_inverter_sn] = std::move(lines);
}

void EmergencyStore::clear(const std::string &sub_inverter_sn) { this->sets_.erase(sub_inverter_sn); }

void EmergencyStore::clear_all() { this->sets_.clear(); }

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
  if (err != ESP_OK || !parse_emergency_sets(blob, this->sets_)) {
    ESP_LOGW(TAG, "Stored emergency sets unreadable; erasing");
    this->sets_.clear();
    nvs_erase_sets(handle);
    nvs_close(handle);
    return false;
  }
  nvs_close(handle);
  this->persisted_hash_ = fnv1_hash(blob);
  size_t total_lines = 0;
  for (const auto &entry : this->sets_)
    total_lines += entry.second.size();
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
  if (blob.size() >= JSON_BUILD_TRUNCATED_SIZE) {
    // build_json truncated the document; persisting it would store invalid
    // JSON that load_from_nvs() erases on the next boot. Keep the previous
    // NVS content (the RAM copy stays complete and usable).
    ESP_LOGE(TAG, "Emergency sets exceed the JSON size cap (%u B); not persisting", blob.size());
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
