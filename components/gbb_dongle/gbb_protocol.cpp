#include "gbb_protocol.h"

#include "esphome/components/json/json_util.h"

namespace esphome {
namespace gbb_dongle {

static void parse_line_array(JsonArray lines_array, std::vector<GbbLine> &out) {
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
    out.push_back(std::move(line));
  }
}

static void serialize_line_array(JsonArray lines, const std::vector<GbbLine> &source) {
  for (const auto &line : source) {
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
      parse_line_array(root["Lines"].as<JsonArray>(), out.lines);
    }
    if (root["IsInvSetup"].is<int32_t>()) {
      out.has_is_inv_setup = true;
      out.is_inv_setup = root["IsInvSetup"].as<int32_t>();
    }
    if (root["LinesOnNoInvSetup"].is<JsonArray>()) {
      out.has_lines_on_no_inv_setup = true;
      parse_line_array(root["LinesOnNoInvSetup"].as<JsonArray>(), out.lines_on_no_inv_setup);
    }
    return true;
  });
}

std::string build_response(const GbbHeader &header, const GbbClientIdentity &identity, const std::string &client_info,
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
      serialize_line_array(root["Lines"].to<JsonArray>(), header.lines);
    }
    // LinesOnNoInvSetup is deliberately not echoed back (GbbConnect2 does,
    // but only as a serializer side effect; the cloud ignores it).
    root["ProtocolVersion"] = CURR_PROTOCOL_VERSION;
    root["GbbVersion"] = identity.version;
    root["GbbEnvironment"] = identity.environment;
    root["ClientVersion"] = identity.version;
    root["ClientEnvironment"] = identity.client_environment;
    root["ClientName"] = identity.client_name;
    if (!client_info.empty())
      root["ClientInfo"] = client_info;
    if (last_log != nullptr)
      root["LastLog"] = *last_log;
  });
}

std::string build_emergency_sets(const std::map<std::string, std::vector<GbbLine>> &sets) {
  return json::build_json([&](JsonObject root) {
    JsonArray sets_array = root["Sets"].to<JsonArray>();
    for (const auto &entry : sets) {
      JsonObject set_obj = sets_array.add<JsonObject>();
      set_obj["SubInverterSN"] = entry.first;
      serialize_line_array(set_obj["Lines"].to<JsonArray>(), entry.second);
    }
  });
}

bool parse_emergency_sets(const std::string &payload, std::map<std::string, std::vector<GbbLine>> &out) {
  return json::parse_json(payload, [&out](JsonObject root) -> bool {
    if (!root["Sets"].is<JsonArray>())
      return false;
    JsonArray sets_array = root["Sets"].as<JsonArray>();
    for (JsonObject set_obj : sets_array) {
      std::string sn;
      if (set_obj["SubInverterSN"].is<const char *>())
        sn = set_obj["SubInverterSN"].as<const char *>();
      std::vector<GbbLine> lines;
      if (set_obj["Lines"].is<JsonArray>())
        parse_line_array(set_obj["Lines"].as<JsonArray>(), lines);
      if (!lines.empty())
        out[sn] = std::move(lines);
    }
    return true;
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
