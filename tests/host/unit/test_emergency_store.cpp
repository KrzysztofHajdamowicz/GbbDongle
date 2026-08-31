#include <catch2/catch_test_macros.hpp>

#include "emergency_store.h"
#include "fakes.h"

using namespace esphome::gbb_dongle;
using namespace gbb_test;

static std::vector<GbbLine> lines(const std::string &modbus, int32_t line_no = 1) {
  GbbLine line;
  line.line_no = line_no;
  line.has_modbus = true;
  line.modbus = modbus;
  return {line};
}

TEST_CASE("emergency store revisions only change with execution content") {
  EmergencyStore store;
  REQUIRE(store.set_lines("", lines("0103")));
  const uint32_t revision = store.revision("");
  auto same = lines("0103");
  same[0].has_timestamp = true;
  same[0].timestamp = 123;
  CHECK_FALSE(store.set_lines("", std::move(same)));
  CHECK(store.revision("") == revision);
  REQUIRE(store.set_lines("", lines("0104")));
  CHECK(store.revision("") > revision);
}

TEST_CASE("emergency persistence skips identical writes and restores") {
  FakeBlobStore blob;
  EmergencyStore store;
  store.set_blob_store(&blob);
  store.set_persist_enabled(true);
  store.set_lines("master", lines("0103009C0003D5CA"));
  store.sync();
  REQUIRE(blob.save_calls == 1);
  store.sync();
  CHECK(blob.save_calls == 1);

  EmergencyStore restored;
  restored.set_blob_store(&blob);
  REQUIRE(restored.load());
  CHECK(restored.size() == 1);
  CHECK(restored.revision("master") != 0);
}

TEST_CASE("corrupt persistence is erased") {
  FakeBlobStore blob;
  blob.present = true;
  blob.blob = "not-json";
  EmergencyStore store;
  store.set_blob_store(&blob);
  CHECK_FALSE(store.load());
  CHECK(blob.erase_calls == 1);
}
