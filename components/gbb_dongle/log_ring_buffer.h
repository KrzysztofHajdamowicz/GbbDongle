#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace esphome {
namespace gbb_dongle {

/// Ring buffer of recent log lines kept in PSRAM, feeding the LastLog field
/// of the GbbOptimizer protocol. The logger callback can fire from any task
/// (never from an ISR), so a mutex is sufficient. A monotonically increasing
/// byte offset gives cheap incremental reads: each SendLastLog request
/// returns everything appended since the previous one.
class LogRingBuffer {
 public:
  bool init(size_t capacity);

  /// Registered with logger::Logger::add_log_callback().
  static void log_hook(void *self, uint8_t level, const char *tag, const char *message, size_t message_len);

  /// Everything since the previous call (bounded by max_bytes and by what is
  /// still in the buffer); advances the read cursor.
  std::string read_incremental(size_t max_bytes);

  /// Only records messages with level <= gate (ESPHOME_LOG_LEVEL_*).
  void set_level_gate(uint8_t level) { this->level_gate_ = level; }
  uint8_t get_level_gate() const { return this->level_gate_; }

 protected:
  void append_(uint8_t level, const char *message, size_t message_len);

  char *buffer_{nullptr};
  size_t capacity_{0};
  uint64_t write_off_{0};
  uint64_t read_off_{0};
  uint8_t level_gate_{255};
  std::mutex mutex_;
};

}  // namespace gbb_dongle
}  // namespace esphome
