#include "log_ring_buffer.h"

#include <cctype>

#include "esphome/core/helpers.h"

namespace esphome {
namespace gbb_dongle {

bool LogRingBuffer::init(size_t capacity) {
  RAMAllocator<char> allocator;
  this->buffer_ = allocator.allocate(capacity);
  if (this->buffer_ == nullptr)
    return false;
  this->capacity_ = capacity;
  return true;
}

void LogRingBuffer::log_hook(void *self, uint8_t level, const char *tag, const char *message, size_t message_len) {
  static_cast<LogRingBuffer *>(self)->append_(level, message, message_len);
}

void LogRingBuffer::append_(uint8_t level, const char *message, size_t message_len) {
  if (this->buffer_ == nullptr || level > this->level_gate_ || message_len == 0)
    return;
  if (message_len > this->capacity_ / 4)
    message_len = this->capacity_ / 4;

  std::lock_guard<std::mutex> guard(this->mutex_);
  for (size_t i = 0; i <= message_len; i++) {
    // Strip ANSI color sequences (ESC ... letter) so LastLog is plain text.
    if (i < message_len && message[i] == '\033') {
      while (i < message_len && !isalpha(static_cast<unsigned char>(message[i])))
        i++;
      continue;
    }
    const char c = (i == message_len) ? '\n' : message[i];
    this->buffer_[this->write_off_ % this->capacity_] = c;
    this->write_off_++;
  }
}

std::string LogRingBuffer::read_incremental(size_t max_bytes) {
  if (this->buffer_ == nullptr)
    return {};

  std::lock_guard<std::mutex> guard(this->mutex_);
  uint64_t start = this->read_off_;
  const uint64_t oldest = this->write_off_ > this->capacity_ ? this->write_off_ - this->capacity_ : 0;
  if (start < oldest)
    start = oldest;  // writer lapped the reader; resume at oldest surviving byte
  uint64_t count = this->write_off_ - start;
  if (count > max_bytes) {
    // Keep the newest max_bytes; the freshest logs are the useful ones.
    start = this->write_off_ - max_bytes;
    count = max_bytes;
  }

  std::string out;
  out.resize(count);
  for (uint64_t i = 0; i < count; i++)
    out[i] = this->buffer_[(start + i) % this->capacity_];
  this->read_off_ = this->write_off_;
  return out;
}

}  // namespace gbb_dongle
}  // namespace esphome
