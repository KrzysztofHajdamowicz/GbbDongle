#pragma once

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>

namespace esphome {
namespace gbb_dongle {

class CoreClock {
 public:
  virtual ~CoreClock() = default;
  virtual uint32_t monotonic_ms() const = 0;
  virtual bool wall_time(time_t &timestamp) const = 0;
};

class ModbusPort {
 public:
  virtual ~ModbusPort() = default;
  virtual size_t serial_available() = 0;
  virtual bool serial_read(uint8_t *byte) = 0;
  virtual void serial_write(const uint8_t *data, size_t size) = 0;
  virtual void serial_flush() = 0;
  virtual void set_transmit_enabled(bool enabled) = 0;
};

class BlobStore {
 public:
  virtual ~BlobStore() = default;
  virtual bool load(std::string &value) = 0;
  virtual bool save(const std::string &value) = 0;
  virtual bool erase() = 0;
};

class CloudTransport {
 public:
  virtual ~CloudTransport() = default;
  virtual bool cloud_connected() const = 0;
  virtual bool cloud_publish(const std::string &topic, const char *payload, size_t size, uint8_t qos,
                             bool retain) = 0;
};

enum class CoreLogLevel : uint8_t { ERROR, WARN, INFO, DEBUG };

class CoreLogger {
 public:
  virtual ~CoreLogger() = default;
  virtual void core_log(CoreLogLevel level, const char *tag, const char *message) = 0;
};

}  // namespace gbb_dongle
}  // namespace esphome
