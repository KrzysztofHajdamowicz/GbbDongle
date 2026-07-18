#include "gbb_protocol.h"

#include "esphome/components/json/json_util.h"

namespace esphome {
namespace gbb_dongle {

bool parse_header(const std::string &payload, GbbHeader &out) {
  return json::parse_json(payload, [&out](JsonObject root) -> bool {
    if (root["Error"].is<const char *>()) {
      out.error = root["Error"].as<const char *>();
    }
    if (root["OrderId"].is<const char *>()) {
      out.has_order_id = true;
      out.order_id = root["OrderId"].as<const char *>();
    }
    if (root["LogLevel"].is<const char *>()) {
      out.has_log_level = true;
      out.log_level = root["LogLevel"].as<const char *>();
    }
    if (root["SendLastLog"].is<int32_t>()) {
      out.has_send_last_log = true;
      out.send_last_log = root["SendLastLog"].as<int32_t>();
    }
    if (root["SubInverterSN"].is<const char *>()) {
      out.has_sub_inverter_sn = true;
      out.sub_inverter_sn = root["SubInverterSN"].as<const char *>();
    }
    if (root["Lines"].is<JsonArray>()) {
      JsonArray lines_array = root["Lines"].as<JsonArray>();
      for (JsonObject line_obj : lines_array) {
        GbbLine line;
        line.line_no = line_obj["LineNo"] | 0;
        if (line_obj["Tag"].is<const char *>()) {
          line.has_tag = true;
          line.tag = line_obj["Tag"].as<const char *>();
        }
        if (line_obj["Timestamp"].is<int64_t>()) {
          line.has_timestamp = true;
          line.timestamp = line_obj["Timestamp"].as<int64_t>();
        }
        if (line_obj["Modbus"].is<const char *>()) {
          line.has_modbus = true;
          line.modbus = line_obj["Modbus"].as<const char *>();
        }
        if (line_obj["Error"].is<const char *>()) {
          line.error = line_obj["Error"].as<const char *>();
        }
        out.lines.push_back(std::move(line));
      }
    }
    return true;
  });
}

std::string build_response(const GbbHeader &header, const std::string &version, const std::string &environment,
                           const std::string *last_log) {
  return json::build_json([&](JsonObject root) {
    if (!header.error.empty())
      root["Error"] = header.error;
    if (header.has_order_id)
      root["OrderId"] = header.order_id;
    if (header.has_log_level)
      root["LogLevel"] = header.log_level;
    if (header.has_send_last_log)
      root["SendLastLog"] = header.send_last_log;
    if (header.has_sub_inverter_sn)
      root["SubInverterSN"] = header.sub_inverter_sn;
    if (!header.lines.empty()) {
      JsonArray lines = root["Lines"].to<JsonArray>();
      for (const auto &line : header.lines) {
        JsonObject obj = lines.add<JsonObject>();
        obj["LineNo"] = line.line_no;
        if (line.has_tag)
          obj["Tag"] = line.tag;
        if (line.has_timestamp)
          obj["Timestamp"] = line.timestamp;
        if (line.has_modbus)
          obj["Modbus"] = line.modbus;
        if (!line.error.empty())
          obj["Error"] = line.error;
      }
    }
    root["GbbVersion"] = version;
    root["GbbEnvironment"] = environment;
    if (last_log != nullptr)
      root["LastLog"] = *last_log;
  });
}

std::string bytes_to_hex(const uint8_t *data, size_t len) {
  static const char HEX[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; i++) {
    out.push_back(HEX[data[i] >> 4]);
    out.push_back(HEX[data[i] & 0x0F]);
  }
  return out;
}

static int hex_nibble(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return -1;
}

bool hex_to_bytes(const std::string &hex, std::vector<uint8_t> &out) {
  if (hex.size() % 2 != 0)
    return false;
  out.clear();
  out.reserve(hex.size() / 2);
  for (size_t i = 0; i < hex.size(); i += 2) {
    int hi = hex_nibble(hex[i]);
    int lo = hex_nibble(hex[i + 1]);
    if (hi < 0 || lo < 0)
      return false;
    out.push_back(static_cast<uint8_t>((hi << 4) | lo));
  }
  return true;
}

uint16_t modbus_crc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

}  // namespace gbb_dongle
}  // namespace esphome
