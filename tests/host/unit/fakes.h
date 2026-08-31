#pragma once

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "core_interfaces.h"

namespace gbb_test {
using namespace esphome::gbb_dongle;

class FakeClock final : public CoreClock {
 public:
  uint32_t monotonic_ms() const override { return monotonic; }
  bool wall_time(time_t &timestamp) const override {
    if (!wall_valid)
      return false;
    timestamp = wall;
    return true;
  }
  uint32_t monotonic{0};
  time_t wall{0};
  bool wall_valid{true};
};

class FakeModbusPort final : public ModbusPort {
 public:
  size_t serial_available() override { return rx.size(); }
  bool serial_read(uint8_t *byte) override {
    if (rx.empty())
      return false;
    *byte = rx.front();
    rx.pop_front();
    return true;
  }
  void serial_write(const uint8_t *data, size_t size) override { writes.emplace_back(data, data + size); }
  void serial_flush() override { flushes++; }
  void set_transmit_enabled(bool enabled) override { direction.push_back(enabled); }
  void inject(std::initializer_list<uint8_t> bytes) { rx.insert(rx.end(), bytes.begin(), bytes.end()); }
  void inject(const std::vector<uint8_t> &bytes) { rx.insert(rx.end(), bytes.begin(), bytes.end()); }

  std::deque<uint8_t> rx;
  std::vector<std::vector<uint8_t>> writes;
  std::vector<bool> direction;
  size_t flushes{0};
};

class FakeBlobStore final : public BlobStore {
 public:
  bool load(std::string &value) override {
    load_calls++;
    if (!present)
      return false;
    value = blob;
    return true;
  }
  bool save(const std::string &value) override {
    save_calls++;
    blob = value;
    present = true;
    return save_ok;
  }
  bool erase() override {
    erase_calls++;
    present = false;
    blob.clear();
    return true;
  }
  std::string blob;
  bool present{false};
  bool save_ok{true};
  size_t load_calls{0};
  size_t save_calls{0};
  size_t erase_calls{0};
};

struct PublishedMessage {
  std::string topic;
  std::string payload;
  uint8_t qos;
  bool retain;
};

class FakeCloud final : public CloudTransport {
 public:
  bool cloud_connected() const override { return connected; }
  bool cloud_publish(const std::string &topic, const char *payload, size_t size, uint8_t qos, bool retain) override {
    messages.push_back({topic, std::string(payload, size), qos, retain});
    return publish_ok;
  }
  bool connected{true};
  bool publish_ok{true};
  std::vector<PublishedMessage> messages;
};

class FakeLogger final : public CoreLogger {
 public:
  void core_log(CoreLogLevel level, const char *tag, const char *message) override {
    entries.push_back(std::to_string(static_cast<unsigned>(level)) + ":" + tag + ":" + message);
  }
  std::vector<std::string> entries;
};
}  // namespace gbb_test
