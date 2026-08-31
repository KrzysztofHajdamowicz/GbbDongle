#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "log_ring_buffer.h"

using namespace esphome::gbb_dongle;

TEST_CASE("log ring filters levels and strips ANSI") {
  std::vector<char> storage(64);
  LogRingBuffer buffer;
  REQUIRE(buffer.init(storage.data(), storage.size()));
  buffer.set_level_gate(3);
  LogRingBuffer::log_hook(&buffer, 4, "ignored", "ignored", 7);
  const std::string colored = "\033[31merror\033[0m";
  LogRingBuffer::log_hook(&buffer, 2, "tag", colored.data(), colored.size());
  CHECK(buffer.read_incremental(64) == "error\n");
  CHECK(buffer.read_incremental(64).empty());
}

TEST_CASE("log ring returns newest surviving bytes after wrap") {
  std::vector<char> storage(8);
  LogRingBuffer buffer;
  REQUIRE(buffer.init(storage.data(), storage.size()));
  buffer.set_level_gate(255);
  for (const char *message : {"a", "b", "c", "d", "e"})
    LogRingBuffer::log_hook(&buffer, 1, "tag", message, 1);
  CHECK(buffer.read_incremental(8) == "b\nc\nd\ne\n");
}
