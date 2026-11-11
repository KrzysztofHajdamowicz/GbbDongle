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

// esphome json::build_json() stops growing its buffer at 5120 B and returns a
// TRUNCATED (invalid) document of exactly this many bytes. Any serialization
// this long must be treated as corrupt, and payload budgets must stay below it.
static constexpr size_t JSON_BUILD_TRUNCATED_SIZE = 5119;

// Upper bound on the elements of one Lines / LinesOnNoInvSetup array. The
// parser allocates one GbbLine per element before anything is validated and
// the element count comes straight from the broker, so without a cap a large
// array exhausts the heap and aborts the firmware (C++ exceptions are off).
// Real cloud batches carry ~10 lines; above a few dozen the fromDevice
// response could not fit JSON_BUILD_TRUNCATED_SIZE anyway. An array over the
// limit makes the whole payload unparseable (ignored, like malformed JSON).
static constexpr size_t MAX_LINES_PER_ARRAY = 50;

// Upper bound on a toDevice payload, checked before it is parsed. The JSON
// document grows with the payload in internal RAM on boards without PSRAM,
// and an oversized one can starve WiFi/TLS of heap before ArduinoJson gives
// up gracefully. MAX_LINES_PER_ARRAY lines with Tag and Timestamp take well
// under half of this.
static constexpr size_t MAX_TODEVICE_PAYLOAD = 16 * 1024;

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

/// Line Error for results cut by cut_response_to_cap().
static constexpr const char *RESPONSE_TOO_LARGE = "Response too large";

/// Shrink a response built without LastLog under the build_json() cap the
/// way GbbConnect2 reports a failing line: walking back from the last line
/// that carries a frame, that line gets Error = RESPONSE_TOO_LARGE and loses
/// its Modbus (later lines already have none, and only the first failing
/// line keeps an Error), until the rebuilt `response` fits or no line with a
/// frame is left. Returns the number of lines cut.
size_t cut_response_to_cap(GbbHeader &header, const GbbClientIdentity &identity, const std::string &client_info,
                           std::string &response);

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

/// True when an uppercase-hex RTU reply is a Modbus exception (function code
/// with bit 7 set), i.e. the slave answered but rejected the request.
bool modbus_hex_is_exception(const std::string &hex);

/// build_json() yields "{}" when ArduinoJson cannot allocate the document,
/// and a document truncated to JSON_BUILD_TRUNCATED_SIZE when it outgrows
/// the cap; neither is a usable serialization of a non-empty object.
inline bool json_build_failed(const std::string &json) {
  return json.size() <= 2 || json.size() >= JSON_BUILD_TRUNCATED_SIZE;
}

}  // namespace gbb_dongle
}  // namespace esphome
