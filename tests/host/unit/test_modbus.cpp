#include <catch2/catch_test_macros.hpp>

#include "fakes.h"
#include "modbus_executor.h"

using namespace esphome::gbb_dongle;
using namespace gbb_test;

static GbbHeader request_with(const std::string &frame) {
  GbbHeader header;
  GbbLine line;
  line.line_no = 1;
  line.has_modbus = true;
  line.modbus = frame;
  header.lines.push_back(line);
  return header;
}

TEST_CASE("executor transmits and accepts a valid response") {
  FakeClock clock;
  FakeModbusPort port;
  ModbusExecutor executor;
  executor.set_clock(&clock);
  executor.set_port(&port);
  executor.start(request_with("010300000002C40B"));
  executor.loop();
  REQUIRE(port.writes.size() == 1);
  CHECK(port.direction == std::vector<bool>{true, false});
  port.inject({0x01, 0x03, 0x04, 0x00, 0x0A, 0x01, 0x02, 0x5A, 0x60});
  executor.loop();
  REQUIRE(executor.has_result());
  GbbHeader result = executor.take_result();
  CHECK(result.lines[0].modbus == "010304000A01025A60");
  CHECK(result.lines[0].error.empty());
}

TEST_CASE("executor applies timeout semantics to remaining lines") {
  FakeClock clock;
  FakeModbusPort port;
  ModbusExecutor executor;
  executor.set_clock(&clock);
  executor.set_port(&port);
  executor.set_response_timeout(50);
  GbbHeader header = request_with("010300000002C40B");
  header.lines.push_back(header.lines.front());
  executor.start(std::move(header));
  executor.loop();
  clock.monotonic = 50;
  executor.loop();
  REQUIRE(executor.has_result());
  GbbHeader result = executor.take_result();
  CHECK(result.lines[0].error == "Response timeout");
  CHECK_FALSE(result.lines[0].has_modbus);
  CHECK_FALSE(result.lines[1].has_modbus);
  CHECK(executor.get_error_count() == 1);
}

TEST_CASE("executor rejects bad CRC and malformed hex") {
  FakeClock clock;
  FakeModbusPort port;
  ModbusExecutor executor;
  executor.set_clock(&clock);
  executor.set_port(&port);
  executor.start(request_with("010300000002C40B"));
  executor.loop();
  port.inject({0x01, 0x03, 0x02, 0x04, 0x00, 0x03, 0x00, 0x00});
  executor.loop();
  CHECK(executor.take_result().lines[0].error == "Invalid CRC in response");

  executor.start(request_with("XYZ"));
  REQUIRE(executor.has_result());
  CHECK(executor.take_result().lines[0].error == "Invalid Modbus hex string");
}

TEST_CASE("executor aborts only at a line boundary") {
  FakeClock clock;
  FakeModbusPort port;
  ModbusExecutor executor;
  executor.set_clock(&clock);
  executor.set_port(&port);
  executor.start(request_with("010300000002C40B"));
  executor.abort_pending_lines();
  executor.loop();
  CHECK(executor.has_result());
  CHECK(port.writes.empty());
}

TEST_CASE("executor honors a read gap across uint32 rollover") {
  FakeClock clock;
  FakeModbusPort port;
  ModbusExecutor executor;
  executor.set_clock(&clock);
  executor.set_port(&port);
  executor.set_read_gap(100);
  GbbHeader header = request_with("010300000002C40B");
  header.lines.push_back(header.lines.front());
  executor.start(std::move(header));
  executor.loop();
  clock.monotonic = 0xFFFFFFF0U;
  port.inject({0x01, 0x03, 0x04, 0x00, 0x0A, 0x01, 0x02, 0x5A, 0x60});
  executor.loop();
  CHECK(port.writes.size() == 1);
  executor.loop();
  CHECK(port.writes.size() == 1);
  clock.monotonic = 84;
  executor.loop();
  CHECK(port.writes.size() == 2);
}

TEST_CASE("executor accepts a valid Modbus exception response") {
  FakeClock clock;
  FakeModbusPort port;
  ModbusExecutor executor;
  executor.set_clock(&clock);
  executor.set_port(&port);
  executor.start(request_with("010300000002C40B"));
  executor.loop();
  port.inject({0x01, 0x83, 0x02, 0xC0, 0xF1});
  executor.loop();
  REQUIRE(executor.has_result());
  const GbbHeader result = executor.take_result();
  CHECK(result.lines[0].error.empty());
  CHECK(result.lines[0].modbus == "018302C0F1");
}

TEST_CASE("executor times out an incomplete response") {
  FakeClock clock;
  FakeModbusPort port;
  ModbusExecutor executor;
  executor.set_clock(&clock);
  executor.set_port(&port);
  executor.set_response_timeout(50);
  executor.start(request_with("010300000002C40B"));
  executor.loop();
  port.inject({0x01, 0x03, 0x04, 0x00, 0x0A});
  executor.loop();
  clock.monotonic = 50;
  executor.loop();
  REQUIRE(executor.has_result());
  CHECK(executor.take_result().lines[0].error == "Response timeout");
}

TEST_CASE("executor frames unknown functions after bus silence") {
  FakeClock clock;
  FakeModbusPort port;
  ModbusExecutor executor;
  executor.set_clock(&clock);
  executor.set_port(&port);
  executor.start(request_with("010300000002C40B"));
  executor.loop();
  std::vector<uint8_t> response{0x01, 0x41, 0xAA};
  const uint16_t crc = modbus_crc16(response.data(), response.size());
  response.push_back(static_cast<uint8_t>(crc & 0xFF));
  response.push_back(static_cast<uint8_t>(crc >> 8));
  port.inject(response);
  executor.loop();
  CHECK_FALSE(executor.has_result());
  clock.monotonic = 5;
  executor.loop();
  REQUIRE(executor.has_result());
  CHECK(executor.take_result().lines[0].modbus == bytes_to_hex(response.data(), response.size()));
}

TEST_CASE("executor caps an overlong frame at 300 bytes") {
  FakeClock clock;
  FakeModbusPort port;
  ModbusExecutor executor;
  executor.set_clock(&clock);
  executor.set_port(&port);
  executor.start(request_with("010300000002C40B"));
  executor.loop();
  std::vector<uint8_t> response(299, 0x55);
  response[0] = 0x01;
  response[1] = 0x41;
  const uint16_t crc = modbus_crc16(response.data(), response.size());
  response.push_back(static_cast<uint8_t>(crc & 0xFF));
  response.push_back(static_cast<uint8_t>(crc >> 8));
  port.inject(response);
  executor.loop();
  CHECK(port.serial_available() == 1);
  clock.monotonic = 5;
  executor.loop();
  REQUIRE(executor.has_result());
  CHECK(executor.take_result().lines[0].error == "Invalid CRC in response");
}

TEST_CASE("executor applies the longer write gap") {
  FakeClock clock;
  FakeModbusPort port;
  ModbusExecutor executor;
  executor.set_clock(&clock);
  executor.set_port(&port);
  executor.set_write_gap(100);
  GbbHeader header = request_with("010600000001480A");
  header.lines.push_back(header.lines.front());
  executor.start(std::move(header));
  executor.loop();
  port.inject({0x01, 0x06, 0x00, 0x00, 0x00, 0x01, 0x48, 0x0A});
  executor.loop();
  clock.monotonic = 99;
  executor.loop();
  CHECK(port.writes.size() == 1);
  clock.monotonic = 100;
  executor.loop();
  CHECK(port.writes.size() == 2);
}
