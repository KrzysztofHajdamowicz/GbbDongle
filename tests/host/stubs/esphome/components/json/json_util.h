// Stand-in for esphome/components/json/json_util.h with the same contract as
// ESPHome's json::parse_json(): false for empty input or text that is not
// JSON, otherwise what the callback returns for the root (an unbound
// JsonObject when the root is not an object).
#pragma once
#include <functional>
#include <string>

#include <ArduinoJson.h>

namespace esphome {
namespace json {

using json_parse_t = std::function<bool(JsonObject)>;

inline bool parse_json(const std::string &data, const json_parse_t &f) {
  if (data.empty())
    return false;
  JsonDocument doc;
  if (deserializeJson(doc, data) != DeserializationError::Ok)
    return false;
  if (doc.overflowed() || doc.isNull())
    return false;
  return f(doc.as<JsonObject>());
}

}  // namespace json
}  // namespace esphome
