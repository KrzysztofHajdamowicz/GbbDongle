#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "dongle_core.h"
#include "fakes.h"

using namespace esphome::gbb_dongle;
using namespace gbb_test;

struct CoreFixture {
  CoreFixture() {
    core.set_clock(&clock);
    core.set_modbus_port(&port);
    core.set_cloud_transport(&cloud);
    core.set_logger(&logger);
    core.set_blob_store(&blob);
    core.set_identity("test", "GbbDongle", "GbbDongle/host");
    core.set_topics("plant/ModbusInMqtt/fromDevice", "plant/keepalive");
    core.set_cloud_configured(true);
    core.set_response_timeout(50);
    log_storage.resize(256);
    core.log_buffer().init(log_storage.data(), log_storage.size());
  }
  FakeClock clock;
  FakeModbusPort port;
  FakeCloud cloud;
  FakeLogger logger;
  FakeBlobStore blob;
  DongleCore core;
  std::vector<char> log_storage;
};

TEST_CASE("core runs cloud request and publishes QoS 2 response") {
  CoreFixture fixture;
  fixture.core.on_cloud_message(
      R"({"OrderId":"one","Lines":[{"LineNo":0,"Modbus":"010300000002C40B"}]})");
  fixture.core.loop();
  REQUIRE(fixture.port.writes.size() == 1);
  fixture.port.inject({0x01, 0x03, 0x04, 0x00, 0x0A, 0x01, 0x02, 0x5A, 0x60});
  fixture.core.loop();
  fixture.core.loop();
  REQUIRE(fixture.cloud.messages.size() == 1);
  CHECK(fixture.cloud.messages[0].topic == "plant/ModbusInMqtt/fromDevice");
  CHECK(fixture.cloud.messages[0].qos == 2);
  CHECK(fixture.cloud.messages[0].payload.find("010304000A01025A60") != std::string::npos);
}

TEST_CASE("core ignores malformed request and emits keepalive") {
  CoreFixture fixture;
  fixture.core.on_cloud_message("{");
  CHECK(fixture.core.get_requests_received() == 1);
  CHECK(fixture.port.writes.empty());
  fixture.clock.monotonic = 60000;
  fixture.core.loop();
  REQUIRE(fixture.cloud.messages.size() == 1);
  CHECK(fixture.cloud.messages[0].topic == "plant/keepalive");
  CHECK(fixture.cloud.messages[0].qos == 1);
}

TEST_CASE("latest queued cloud request replaces an older one") {
  CoreFixture fixture;
  fixture.core.on_cloud_message(R"({"OrderId":"old","Lines":[]})");
  fixture.core.on_cloud_message(R"({"OrderId":"new","Lines":[]})");
  fixture.core.loop();
  fixture.core.loop();
  REQUIRE(fixture.cloud.messages.size() == 1);
  CHECK(fixture.cloud.messages[0].payload.find("new") != std::string::npos);
  CHECK(fixture.cloud.messages[0].payload.find("old") == std::string::npos);
}

TEST_CASE("emergency set triggers next hour and clears after delivery") {
  CoreFixture fixture;
  fixture.clock.wall = 3600;
  fixture.core.on_cloud_message(
      R"({"IsInvSetup":1,"LinesOnNoInvSetup":[{"LineNo":7,"Modbus":"010300000002C40B"}]})");
  fixture.core.loop();
  fixture.core.loop();
  REQUIRE(fixture.core.get_emergency_sets_stored() == 1);

  fixture.clock.wall = 2 * 3600 + 11 * 60;
  fixture.clock.monotonic = 5000;
  fixture.core.loop();
  fixture.core.loop();
  REQUIRE(fixture.port.writes.size() == 1);
  fixture.port.inject({0x01, 0x03, 0x04, 0x00, 0x0A, 0x01, 0x02, 0x5A, 0x60});
  fixture.core.loop();
  fixture.core.loop();
  CHECK(fixture.core.get_emergency_runs() == 1);
  CHECK(fixture.core.get_emergency_delivered() == 1);
  CHECK(fixture.core.get_emergency_sets_stored() == 0);
}

TEST_CASE("LastLog is attached once and consumed incrementally") {
  CoreFixture fixture;
  fixture.core.log_buffer().set_level_gate(5);
  const std::string message = "diagnostic line";
  LogRingBuffer::log_hook(&fixture.core.log_buffer(), 2, "tag", message.data(), message.size());
  fixture.core.on_cloud_message(R"({"OrderId":"logs","SendLastLog":1,"Lines":[]})");
  fixture.core.loop();
  fixture.core.loop();
  REQUIRE(fixture.cloud.messages.size() == 1);
  CHECK(fixture.cloud.messages[0].payload.find("diagnostic line\\n") != std::string::npos);

  fixture.core.on_cloud_message(R"({"OrderId":"empty","SendLastLog":1,"Lines":[]})");
  fixture.core.loop();
  fixture.core.loop();
  REQUIRE(fixture.cloud.messages.size() == 2);
  CHECK(fixture.cloud.messages[1].payload.find("\"LastLog\":\"\"") != std::string::npos);
}

TEST_CASE("failed emergency runs retry with capped exponential backoff") {
  CoreFixture fixture;
  fixture.core.set_emergency_retry_initial(6000);
  fixture.core.set_emergency_retry_max(12000);
  fixture.clock.wall = 3600;
  fixture.core.on_cloud_message(
      R"({"IsInvSetup":1,"LinesOnNoInvSetup":[{"LineNo":7,"Modbus":"010300000002C40B"}]})");
  fixture.core.loop();
  fixture.core.loop();

  fixture.clock.wall = 2 * 3600 + 11 * 60;
  fixture.clock.monotonic = 5000;
  fixture.core.loop();
  fixture.core.loop();
  REQUIRE(fixture.port.writes.size() == 1);
  fixture.clock.monotonic = 5050;
  fixture.core.loop();
  fixture.core.loop();

  fixture.clock.monotonic = 10000;
  fixture.core.loop();
  CHECK(fixture.port.writes.size() == 1);
  fixture.clock.monotonic = 15000;
  fixture.core.loop();
  fixture.core.loop();
  REQUIRE(fixture.port.writes.size() == 2);
  fixture.clock.monotonic = 15050;
  fixture.core.loop();
  fixture.core.loop();

  fixture.clock.monotonic = 20000;
  fixture.core.loop();
  fixture.clock.monotonic = 25000;
  fixture.core.loop();
  CHECK(fixture.port.writes.size() == 2);
  fixture.clock.monotonic = 30000;
  fixture.core.loop();
  fixture.core.loop();
  CHECK(fixture.port.writes.size() == 3);
  CHECK(fixture.core.get_emergency_runs() == 3);
  CHECK(fixture.core.get_emergency_delivered() == 0);
}

TEST_CASE("fresh cloud setup cancels emergency at a frame boundary") {
  CoreFixture fixture;
  fixture.clock.wall = 3600;
  fixture.core.on_cloud_message(
      R"({"IsInvSetup":1,"LinesOnNoInvSetup":[{"LineNo":1,"Modbus":"010300000002C40B"},{"LineNo":2,"Modbus":"010300000002C40B"}]})");
  fixture.core.loop();
  fixture.core.loop();
  fixture.clock.wall = 2 * 3600 + 11 * 60;
  fixture.clock.monotonic = 5000;
  fixture.core.loop();
  fixture.core.loop();
  REQUIRE(fixture.port.writes.size() == 1);

  fixture.core.on_cloud_message(R"({"OrderId":"cancel","IsInvSetup":1,"Lines":[]})");
  fixture.port.inject({0x01, 0x03, 0x04, 0x00, 0x0A, 0x01, 0x02, 0x5A, 0x60});
  fixture.core.loop();
  fixture.core.loop();
  fixture.core.loop();
  CHECK(fixture.port.writes.size() == 1);
  CHECK(fixture.core.get_emergency_sets_stored() == 1);
  CHECK(fixture.core.get_emergency_delivered() == 0);
  REQUIRE_FALSE(fixture.cloud.messages.empty());
  CHECK(fixture.cloud.messages.back().payload.find("cancel") != std::string::npos);
}
