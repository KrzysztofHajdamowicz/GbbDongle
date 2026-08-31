#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#if __has_include(<mosquitto/libmosquitto.h>)
#include <mosquitto/libmosquitto.h>
#else
#include <mosquitto.h>
#endif

#include "core_interfaces.h"
#include "dongle_core.h"

namespace gbb_host {
using namespace esphome::gbb_dongle;

class HostClock final : public CoreClock {
 public:
  HostClock();
  uint32_t monotonic_ms() const override;
  bool wall_time(time_t &timestamp) const override;
  void set_wall_time(time_t timestamp);
  void set_wall_invalid();
  void set_monotonic(uint32_t value);
  void advance(uint32_t milliseconds);

 private:
  std::chrono::steady_clock::time_point started_;
  bool manual_monotonic_{false};
  uint32_t manual_ms_{0};
  bool manual_wall_{false};
  bool wall_valid_{true};
  time_t manual_timestamp_{0};
};

class PosixSerialPort final : public ModbusPort {
 public:
  PosixSerialPort(const std::string &path, unsigned baud, const std::string &parity);
  ~PosixSerialPort() override;
  bool valid() const { return descriptor_ >= 0; }
  size_t serial_available() override;
  bool serial_read(uint8_t *byte) override;
  void serial_write(const uint8_t *data, size_t size) override;
  void serial_flush() override;
  void set_transmit_enabled(bool) override {}

 private:
  int descriptor_{-1};
};

class FileBlobStore final : public BlobStore {
 public:
  explicit FileBlobStore(std::string path) : path_(std::move(path)) {}
  bool load(std::string &value) override;
  bool save(const std::string &value) override;
  bool erase() override;

 private:
  std::string path_;
};

class HostLogger final : public CoreLogger {
 public:
  explicit HostLogger(LogRingBuffer *ring = nullptr) : ring_(ring) {}
  void core_log(CoreLogLevel level, const char *tag, const char *message) override;

 private:
  LogRingBuffer *ring_;
};

class MosquittoCloud final : public CloudTransport {
 public:
  MosquittoCloud(std::string host, uint16_t port, std::string plant_id, std::string token, DongleCore *core);
  ~MosquittoCloud() override;
  bool start();
  void loop();
  bool cloud_connected() const override { return connected_; }
  bool cloud_publish(const std::string &topic, const char *payload, size_t size, uint8_t qos, bool retain) override;

 private:
  static void on_connect_(mosquitto *instance, void *userdata, int result);
  static void on_disconnect_(mosquitto *instance, void *userdata, int result);
  static void on_message_(mosquitto *instance, void *userdata, const mosquitto_message *message);

  std::string host_;
  uint16_t port_;
  std::string plant_id_;
  std::string token_;
  std::string topic_to_device_;
  DongleCore *core_;
  mosquitto *instance_{nullptr};
  bool connected_{false};
  std::chrono::steady_clock::time_point last_reconnect_{};
};

class ControlSocket {
 public:
  explicit ControlSocket(std::string path) : path_(std::move(path)) {}
  ~ControlSocket();
  bool start();
  bool poll(HostClock &clock, const DongleCore &core, bool &stop);

 private:
  std::string path_;
  std::string pending_;
  int listener_{-1};
  int client_{-1};
};

}  // namespace gbb_host
