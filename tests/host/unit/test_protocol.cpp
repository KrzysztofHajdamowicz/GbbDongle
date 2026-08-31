#include <catch2/catch_test_macros.hpp>

#include "gbb_protocol.h"

using namespace esphome::gbb_dongle;

TEST_CASE("protocol parses optional fields and Modbus lines") {
  GbbHeader header;
  REQUIRE(parse_header(
      R"({"OrderId":"order","SendLastLog":1,"SubInverterSN":" 7 ","Lines":[{"LineNo":3,"Tag":"x","Timestamp":1784392208,"Modbus":"0103009C0003D5CA"}]})",
      header));
  CHECK(header.has_order_id);
  CHECK(header.order_id == "order");
  CHECK(header.has_send_last_log);
  REQUIRE(header.lines.size() == 1);
  CHECK(header.lines[0].line_no == 3);
  CHECK(header.lines[0].has_timestamp);
  CHECK(header.lines[0].modbus == "0103009C0003D5CA");
}

TEST_CASE("protocol rejects malformed and non-object JSON") {
  GbbHeader header;
  CHECK_FALSE(parse_header("", header));
  CHECK_FALSE(parse_header("{", header));
  CHECK_FALSE(parse_header("null", header));
  CHECK_FALSE(parse_header("[]", header));
}

TEST_CASE("response keeps protocol identity and escaping") {
  GbbHeader header;
  header.has_order_id = true;
  header.order_id = "quote\"line\n";
  GbbClientIdentity identity{"1.2.3", "GbbDongle", "GbbDongle", "GbbDongle/test"};
  const GbbJsonResult encoded = build_response(header, identity, "Ethernet", nullptr);
  REQUIRE_FALSE(encoded.overflow);
  CHECK(encoded.payload.find("\\\"") != std::string::npos);
  CHECK(encoded.payload.find("\"ProtocolVersion\":2") != std::string::npos);
  CHECK(encoded.payload.find("\"ClientVersion\":\"1.2.3\"") != std::string::npos);
}

TEST_CASE("response reports overflow instead of returning truncated JSON") {
  GbbHeader header;
  header.has_order_id = true;
  header.order_id.assign(JSON_BUILD_TRUNCATED_SIZE, 'x');
  GbbClientIdentity identity{"dev", "GbbDongle", "GbbDongle", "GbbDongle/test"};
  const GbbJsonResult encoded = build_response(header, identity, "", nullptr);
  CHECK(encoded.overflow);
  CHECK(encoded.payload.empty());
}

TEST_CASE("emergency sets round-trip escaped keys and reject oversized blobs") {
  GbbLine line;
  line.line_no = 7;
  line.has_modbus = true;
  line.modbus = "010300000002C40B";
  std::map<std::string, std::vector<GbbLine>> sets{{"serial\"number", {line}}};
  const GbbJsonResult encoded = build_emergency_sets(sets);
  REQUIRE_FALSE(encoded.overflow);
  CHECK(encoded.payload.find("serial\\\"number") != std::string::npos);

  std::map<std::string, std::vector<GbbLine>> decoded;
  REQUIRE(parse_emergency_sets(encoded.payload, decoded));
  REQUIRE(decoded.at("serial\"number").size() == 1);
  CHECK(decoded.at("serial\"number")[0].modbus == line.modbus);

  sets.clear();
  line.modbus.assign(JSON_BUILD_TRUNCATED_SIZE, 'A');
  sets["large"] = {line};
  const GbbJsonResult oversized = build_emergency_sets(sets);
  CHECK(oversized.overflow);
  CHECK(oversized.payload.empty());
}

TEST_CASE("hex and Modbus CRC match captured vector") {
  std::vector<uint8_t> bytes;
  REQUIRE(hex_to_bytes("010300000002", bytes));
  CHECK(bytes_to_hex(bytes.data(), bytes.size()) == "010300000002");
  CHECK(modbus_crc16(bytes.data(), bytes.size()) == 0x0BC4);
  CHECK_FALSE(hex_to_bytes("123", bytes));
  CHECK_FALSE(hex_to_bytes("GG", bytes));
}
