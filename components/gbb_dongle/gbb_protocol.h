#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace esphome {
namespace gbb_dongle {

// Mirrors GbbConnect2Protocol/Protocol.cs. Fields are serialized with
// PascalCase keys; absent/null fields are omitted (System.Text.Json
// WhenWritingNull semantics), which the has_* flags reproduce.

struct GbbLine {
  int32_t line_no{0};
  bool has_tag{false};
  std::string tag;
  bool has_timestamp{false};
  int64_t timestamp{0};
  bool has_modbus{false};
  std::string modbus;  // uppercase hex Modbus RTU frame including CRC
  std::string error;   // empty = no error
};

struct GbbHeader {
  std::string error;  // empty = no error
  bool has_order_id{false};
  std::string order_id;
  bool has_log_level{false};
  std::string log_level;  // "OnlyErrors" | "Min" | "Max"
  bool has_send_last_log{false};
  int32_t send_last_log{0};
  bool has_sub_inverter_sn{false};
  std::string sub_inverter_sn;
  std::vector<GbbLine> lines;
};

/// Parse a toDevice JSON payload. Returns false on malformed JSON.
bool parse_header(const std::string &payload, GbbHeader &out);

/// Serialize a response Header for fromDevice. version/environment are always
/// stamped; last_log is attached only when non-null.
std::string build_response(const GbbHeader &header, const std::string &version, const std::string &environment,
                           const std::string *last_log);

/// Uppercase hex <-> bytes ("0103009C0003D5CA"). Decode returns false on
/// non-hex characters or odd length.
std::string bytes_to_hex(const uint8_t *data, size_t len);
bool hex_to_bytes(const std::string &hex, std::vector<uint8_t> &out);

/// Modbus CRC-16 (poly 0xA001, init 0xFFFF), as in GbbConnect2 ModBus.GetCRC.
uint16_t modbus_crc16(const uint8_t *data, size_t len);

}  // namespace gbb_dongle
}  // namespace esphome
