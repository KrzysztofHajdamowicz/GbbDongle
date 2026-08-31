#include "nvs_blob_store.h"

#include <nvs.h>

#include "esphome/core/log.h"

namespace esphome {
namespace gbb_dongle {

static const char *const TAG = "gbb_dongle.nvs";
static const char *const NVS_NAMESPACE = "gbb_dongle";
static const char *const NVS_KEY = "emerg_sets";

bool NvsBlobStore::load(std::string &value) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK)
    return false;
  size_t length = 0;
  err = nvs_get_blob(handle, NVS_KEY, nullptr, &length);
  if (err != ESP_OK || length == 0) {
    nvs_close(handle);
    return false;
  }
  value.resize(length);
  err = nvs_get_blob(handle, NVS_KEY, value.data(), &length);
  nvs_close(handle);
  return err == ESP_OK;
}

bool NvsBlobStore::save(const std::string &value) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK)
    return false;
  err = nvs_set_blob(handle, NVS_KEY, value.data(), value.size());
  if (err == ESP_OK)
    err = nvs_commit(handle);
  nvs_close(handle);
  if (err != ESP_OK)
    ESP_LOGW(TAG, "Persisting emergency sets failed: %s", esp_err_to_name(err));
  return err == ESP_OK;
}

bool NvsBlobStore::erase() {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK)
    return false;
  err = nvs_erase_key(handle, NVS_KEY);
  if (err == ESP_OK)
    err = nvs_commit(handle);
  nvs_close(handle);
  return err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND;
}

}  // namespace gbb_dongle
}  // namespace esphome
