#include "host_adapters.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>
#include <unistd.h>

namespace gbb_host {

HostClock::HostClock() : started_(std::chrono::steady_clock::now()) {}

uint32_t HostClock::monotonic_ms() const {
  if (this->manual_monotonic_)
    return this->manual_ms_;
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_);
  return static_cast<uint32_t>(elapsed.count());
}

bool HostClock::wall_time(time_t &timestamp) const {
  if (!this->manual_wall_) {
    timestamp = std::time(nullptr);
    return true;
  }
  if (!this->wall_valid_)
    return false;
  timestamp = this->manual_timestamp_;
  return true;
}

void HostClock::set_wall_time(time_t timestamp) {
  this->manual_wall_ = true;
  this->wall_valid_ = true;
  this->manual_timestamp_ = timestamp;
}

void HostClock::set_wall_invalid() {
  this->manual_wall_ = true;
  this->wall_valid_ = false;
}

void HostClock::set_monotonic(uint32_t value) {
  this->manual_monotonic_ = true;
  this->manual_ms_ = value;
}

void HostClock::advance(uint32_t milliseconds) {
  if (!this->manual_monotonic_)
    this->set_monotonic(this->monotonic_ms());
  this->manual_ms_ += milliseconds;
  if (this->manual_wall_ && this->wall_valid_)
    this->manual_timestamp_ += static_cast<time_t>(milliseconds / 1000);
}

static speed_t baud_to_speed(unsigned baud) {
  switch (baud) {
    case 2400:
      return B2400;
    case 4800:
      return B4800;
    case 9600:
      return B9600;
    case 19200:
      return B19200;
    case 38400:
      return B38400;
    case 57600:
      return B57600;
    case 115200:
      return B115200;
    default:
      return B0;
  }
}

PosixSerialPort::PosixSerialPort(const std::string &path, unsigned baud, const std::string &parity) {
  this->descriptor_ = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (this->descriptor_ < 0)
    return;
  termios options{};
  if (tcgetattr(this->descriptor_, &options) != 0) {
    ::close(this->descriptor_);
    this->descriptor_ = -1;
    return;
  }
  cfmakeraw(&options);
  if (parity == "NONE") {
    options.c_cflag &= static_cast<tcflag_t>(~PARENB);
  } else if (parity == "EVEN") {
    options.c_cflag |= PARENB;
    options.c_cflag &= static_cast<tcflag_t>(~PARODD);
  } else if (parity == "ODD") {
    options.c_cflag |= static_cast<tcflag_t>(PARENB | PARODD);
  } else {
    ::close(this->descriptor_);
    this->descriptor_ = -1;
    return;
  }
  const speed_t speed = baud_to_speed(baud);
  if (speed == B0 || cfsetispeed(&options, speed) != 0 || cfsetospeed(&options, speed) != 0 ||
      tcsetattr(this->descriptor_, TCSANOW, &options) != 0) {
    ::close(this->descriptor_);
    this->descriptor_ = -1;
  }
}

PosixSerialPort::~PosixSerialPort() {
  if (this->descriptor_ >= 0)
    ::close(this->descriptor_);
}

size_t PosixSerialPort::serial_available() {
  int bytes = 0;
  return ioctl(this->descriptor_, FIONREAD, &bytes) == 0 && bytes > 0 ? static_cast<size_t>(bytes) : 0;
}

bool PosixSerialPort::serial_read(uint8_t *byte) { return ::read(this->descriptor_, byte, 1) == 1; }

void PosixSerialPort::serial_write(const uint8_t *data, size_t size) {
  size_t offset = 0;
  while (offset < size) {
    const ssize_t written = ::write(this->descriptor_, data + offset, size - offset);
    if (written > 0)
      offset += static_cast<size_t>(written);
    else if (errno != EINTR)
      break;
  }
}

void PosixSerialPort::serial_flush() {
  // A PTY has no physical shift register. On macOS tcdrain() waits until the
  // peer consumes the bytes, unlike a UART flush which only waits for TX.
}

bool FileBlobStore::load(std::string &value) {
  std::ifstream stream(this->path_, std::ios::binary);
  if (!stream)
    return false;
  value.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
  return stream.good() || stream.eof();
}

bool FileBlobStore::save(const std::string &value) {
  const std::string temporary = this->path_ + ".tmp";
  {
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream)
      return false;
    stream.write(value.data(), static_cast<std::streamsize>(value.size()));
    if (!stream)
      return false;
  }
  return std::rename(temporary.c_str(), this->path_.c_str()) == 0;
}

bool FileBlobStore::erase() { return std::remove(this->path_.c_str()) == 0 || errno == ENOENT; }

void HostLogger::core_log(CoreLogLevel level, const char *tag, const char *message) {
  const uint8_t ring_level = level == CoreLogLevel::ERROR   ? 1
                             : level == CoreLogLevel::WARN  ? 2
                             : level == CoreLogLevel::INFO  ? 3
                                                            : 5;
  if (this->ring_ != nullptr)
    LogRingBuffer::log_hook(this->ring_, ring_level, tag, message, std::strlen(message));
  std::cerr << '[' << tag << "] " << message << '\n';
}

MosquittoCloud::MosquittoCloud(std::string host, uint16_t port, std::string plant_id, std::string token,
                               DongleCore *core)
    : host_(std::move(host)),
      port_(port),
      plant_id_(std::move(plant_id)),
      token_(std::move(token)),
      topic_to_device_(plant_id_ + "/ModbusInMqtt/toDevice"),
      core_(core) {
  mosquitto_lib_init();
  this->instance_ = mosquitto_new(("GbbConnect2_" + this->plant_id_).c_str(), true, this);
  if (this->instance_ != nullptr) {
    mosquitto_username_pw_set(this->instance_, this->plant_id_.c_str(), this->token_.c_str());
    mosquitto_connect_callback_set(this->instance_, &MosquittoCloud::on_connect_);
    mosquitto_disconnect_callback_set(this->instance_, &MosquittoCloud::on_disconnect_);
    mosquitto_message_callback_set(this->instance_, &MosquittoCloud::on_message_);
    mosquitto_reconnect_delay_set(this->instance_, 1, 5, true);
  }
}

MosquittoCloud::~MosquittoCloud() {
  if (this->instance_ != nullptr) {
    mosquitto_disconnect(this->instance_);
    mosquitto_destroy(this->instance_);
  }
  mosquitto_lib_cleanup();
}

bool MosquittoCloud::start() {
  return this->instance_ != nullptr && mosquitto_connect_async(this->instance_, this->host_.c_str(), this->port_, 15) == MOSQ_ERR_SUCCESS;
}

void MosquittoCloud::loop() {
  if (this->instance_ == nullptr)
    return;
  const int result = mosquitto_loop(this->instance_, 0, 4);
  if (result == MOSQ_ERR_SUCCESS)
    return;
  const auto now = std::chrono::steady_clock::now();
  if (now - this->last_reconnect_ >= std::chrono::seconds(1)) {
    this->last_reconnect_ = now;
    // The host runner only connects to loopback. A synchronous reconnect here
    // avoids requiring libmosquitto's background thread (and therefore avoids
    // callbacks racing DongleCore::loop()). A refused loopback connection
    // returns immediately and is retried on the next interval.
    mosquitto_reconnect(this->instance_);
  }
}

bool MosquittoCloud::cloud_publish(const std::string &topic, const char *payload, size_t size, uint8_t qos,
                                   bool retain) {
  return this->instance_ != nullptr &&
         mosquitto_publish(this->instance_, nullptr, topic.c_str(), static_cast<int>(size), payload, qos, retain) ==
             MOSQ_ERR_SUCCESS;
}

void MosquittoCloud::on_connect_(mosquitto *, void *userdata, int result) {
  auto *self = static_cast<MosquittoCloud *>(userdata);
  self->connected_ = result == 0;
  if (self->connected_)
    mosquitto_subscribe(self->instance_, nullptr, self->topic_to_device_.c_str(), 1);
}

void MosquittoCloud::on_disconnect_(mosquitto *, void *userdata, int) {
  static_cast<MosquittoCloud *>(userdata)->connected_ = false;
}

void MosquittoCloud::on_message_(mosquitto *, void *userdata, const mosquitto_message *message) {
  auto *self = static_cast<MosquittoCloud *>(userdata);
  if (message == nullptr || message->payload == nullptr || message->payloadlen < 0)
    return;
  self->core_->on_cloud_message(
      std::string(static_cast<const char *>(message->payload), static_cast<size_t>(message->payloadlen)));
}

ControlSocket::~ControlSocket() {
  if (this->client_ >= 0)
    ::close(this->client_);
  if (this->listener_ >= 0)
    ::close(this->listener_);
  if (!this->path_.empty())
    ::unlink(this->path_.c_str());
}

bool ControlSocket::start() {
  if (this->path_.empty())
    return true;
  if (this->path_.size() >= sizeof(sockaddr_un::sun_path))
    return false;
  this->listener_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (this->listener_ < 0)
    return false;
  ::unlink(this->path_.c_str());
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  std::strncpy(address.sun_path, this->path_.c_str(), sizeof(address.sun_path) - 1);
  if (::bind(this->listener_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 ||
      ::listen(this->listener_, 1) != 0)
    return false;
  fcntl(this->listener_, F_SETFL, O_NONBLOCK);
  return true;
}

bool ControlSocket::poll(HostClock &clock, const DongleCore &core, bool &stop) {
  if (this->listener_ < 0)
    return true;
  if (this->client_ < 0) {
    this->client_ = ::accept(this->listener_, nullptr, nullptr);
    if (this->client_ < 0)
      return errno == EAGAIN || errno == EWOULDBLOCK;
    fcntl(this->client_, F_SETFL, O_NONBLOCK);
  }
  char buffer[512];
  const ssize_t count = ::read(this->client_, buffer, sizeof(buffer));
  if (count == 0) {
    ::close(this->client_);
    this->client_ = -1;
    return true;
  }
  if (count < 0)
    return errno == EAGAIN || errno == EWOULDBLOCK;
  this->pending_.append(buffer, static_cast<size_t>(count));
  size_t newline;
  while ((newline = this->pending_.find('\n')) != std::string::npos) {
    const std::string command = this->pending_.substr(0, newline);
    this->pending_.erase(0, newline + 1);
    std::string response = "OK\n";
    try {
      if (command.rfind("TIME ", 0) == 0)
        clock.set_wall_time(static_cast<time_t>(std::stoll(command.substr(5))));
      else if (command == "TIME_INVALID")
        clock.set_wall_invalid();
      else if (command.rfind("MONOTONIC ", 0) == 0)
        clock.set_monotonic(static_cast<uint32_t>(std::stoul(command.substr(10))));
      else if (command.rfind("ADVANCE ", 0) == 0)
        clock.advance(static_cast<uint32_t>(std::stoul(command.substr(8))));
      else if (command == "STATS")
        response = "requests=" + std::to_string(core.get_requests_received()) + " handled=" +
                   std::to_string(core.get_requests_handled()) + " errors=" +
                   std::to_string(core.get_modbus_errors()) + " emergency_runs=" +
                   std::to_string(core.get_emergency_runs()) + "\n";
      else if (command == "STOP")
        stop = true;
      else
        response = "ERROR unknown command\n";
    } catch (const std::exception &) {
      response = "ERROR invalid argument\n";
    }
    ::write(this->client_, response.data(), response.size());
  }
  return true;
}

}  // namespace gbb_host
