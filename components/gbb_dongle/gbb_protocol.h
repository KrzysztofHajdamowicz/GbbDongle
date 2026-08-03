#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace esphome {
namespace gbb_dongle {

// Mirrors GbbConnect2Protocol/Protocol.cs. Fields are serialized with
// PascalCase keys; absent/null fields are omitted (System.Text.Json
// WhenWritingNull semantics), which the has_* flags reproduce.

// Header.CURR_PROTOCOL_VERSION in GbbConnect2 2.0.0.
static constexpr int32_t CURR_PROTOCOL_VERSION = 2;

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
  bool has_is_inv_setup{false};
  int32_t is_inv_setup{0};
  // Emergency ("last will") command set, parse-only: an empty array present
  // in the payload means "clear the stored set" (original replace semantics).
  bool has_lines_on_no_inv_setup{false};
  std::vector<GbbLine> lines_on_no_inv_setup;
  // Internal routing flag, never serialized: marks a locally-generated
  // emergency run whose result is logged instead of published.
  bool emergency{false};
};

/// Parse a toDevice JSON payload. Returns false on malformed JSON.
bool parse_header(const std::string &payload, GbbHeader &out);

/// Compile-time identity stamped on every fromDevice response. version is
/// emitted as both ClientVersion and the legacy GbbVersion; environment is
/// the legacy GbbEnvironment. The legacy keys stay until the cloud fully
/// migrates to the Client* names.
struct GbbClientIdentity {
  const char *version;
  const char *environment;
  const char *client_name;
  const char *client_environment;
};

/// Serialize a response Header for fromDevice. The identity fields are always
/// stamped; client_info (built fresh per response) and last_log are attached
/// only when non-empty / non-null.
std::string build_response(const GbbHeader &header, const GbbClientIdentity &identity, const std::string &client_info,
                           const std::string *last_log);

/// Serialize / parse the emergency ("last will") sets for NVS persistence:
/// {"Sets":[{"SubInverterSN":"","Lines":[...]}]}. Key "" = master.
/// parse_emergency_sets returns false on malformed JSON.
std::string build_emergency_sets(const std::map<std::string, std::vector<GbbLine>> &sets);
bool parse_emergency_sets(const std::string &payload, std::map<std::string, std::vector<GbbLine>> &out);

/// Uppercase hex <-> bytes ("0103009C0003D5CA"). Decode returns false on
/// non-hex characters or odd length.
std::string bytes_to_hex(const uint8_t *data, size_t len);
bool hex_to_bytes(const std::string &hex, std::vector<uint8_t> &out);

/// Modbus CRC-16 (poly 0xA001, init 0xFFFF), as in GbbConnect2 ModBus.GetCRC.
uint16_t modbus_crc16(const uint8_t *data, size_t len);

}  // namespace gbb_dongle
}  // namespace esphome
