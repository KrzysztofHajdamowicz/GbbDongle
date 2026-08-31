#include "modbus_executor.h"

#include <cstdio>

namespace esphome {
namespace gbb_dongle {

static const char *const TAG = "gbb_dongle.modbus";

// Response frames: 1 addr + 1 fn + 250 data max + 2 CRC, with headroom.
static const size_t MAX_FRAME_SIZE = 300;

void ModbusExecutor::log_(CoreLogLevel level, const std::string &message) const {
  if (this->logger_ != nullptr)
    this->logger_->core_log(level, TAG, message.c_str());
}

// Space-separated uppercase hex for the human-readable debug log lines, e.g.
// "01 03 02 04 00 03 45 B2". Easier to eyeball byte-by-byte than a run-on
// hex string. Returns a shared static buffer: ESP_LOG* arguments are
// evaluated even below the runtime log level, so this must not allocate on
// every frame; the executor only ever runs on the single main-loop task, and
// no log statement formats two frames at once.
static const char *frame_to_log_hex(const std::vector<uint8_t> &frame) {
  static const char HEX[] = "0123456789ABCDEF";
  static char buf[MAX_FRAME_SIZE * 3 + 1];
  const size_t n = frame.size() < MAX_FRAME_SIZE ? frame.size() : MAX_FRAME_SIZE;
  char *p = buf;
  for (size_t i = 0; i < n; i++) {
    if (i != 0)
      *p++ = ' ';
    *p++ = HEX[frame[i] >> 4];
    *p++ = HEX[frame[i] & 0x0F];
  }
  *p = '\0';
  return buf;
}

void ModbusExecutor::start(GbbHeader &&header) {
  this->header_ = std::move(header);
  this->line_index_ = 0;
  this->abort_requested_ = false;
  // One-time worst-case capacity; clear() never shrinks, so the steady state
  // stays allocation-free regardless of frame sizes seen so far.
  this->tx_frame_.reserve(MAX_FRAME_SIZE);
  this->rx_frame_.reserve(MAX_FRAME_SIZE);
  this->state_ = State::GAP;  // honors the gap left over from the previous batch
  this->start_next_line_();
}

GbbHeader ModbusExecutor::take_result() {
  this->state_ = State::IDLE;
  return std::move(this->header_);
}

void ModbusExecutor::loop() {
  switch (this->state_) {
    case State::IDLE:
    case State::DONE:
      break;
    case State::GAP:
      // Safe boundary: the next frame has not been transmitted yet.
      if (this->abort_requested_) {
        this->abort_batch_();
        break;
      }
      // Signed difference, not >=: gap_until_ may sit across the ~49.7-day
      // millis() rollover, where the absolute compare either skips the gap
      // or parks the executor until the next wrap.
      if ((int32_t) (this->clock_->monotonic_ms() - this->gap_until_) >= 0)
        this->transmit_current_();
      break;
    case State::TRANSMIT:
      // transmit_current_() completes synchronously; state never rests here
      break;
    case State::RX_WAIT:
      this->handle_rx_();
      break;
  }
}

void ModbusExecutor::start_next_line_() {
  // Safe boundary: the previous line's response (or timeout) is complete and
  // the next frame is not on the bus yet.
  if (this->abort_requested_) {
    this->abort_batch_();
    return;
  }
  // Skip lines without a Modbus payload (GbbConnect2 only processes lines
  // that carry one).
  while (this->line_index_ < this->header_.lines.size() &&
         (!this->header_.lines[this->line_index_].has_modbus || this->header_.lines[this->line_index_].modbus.empty())) {
    this->line_index_++;
  }
  if (this->line_index_ >= this->header_.lines.size()) {
    this->finish_all_();
    return;
  }

  GbbLine &line = this->header_.lines[this->line_index_];
  if (!hex_to_bytes(line.modbus, this->tx_frame_) || this->tx_frame_.size() < 4) {
    char message[420];
    snprintf(message, sizeof(message), "Line %d: invalid Modbus frame ('%s'); skipping this and the rest",
             static_cast<int>(line.line_no), line.modbus.c_str());
    this->log_(CoreLogLevel::WARN, message);
    this->fail_line_("Invalid Modbus hex string");
    return;
  }
  this->state_ = State::GAP;  // gap_until_ still holds the deadline from the previous command
}

void ModbusExecutor::transmit_current_() {
  // Drain any stale bytes so the response frame starts clean.
  while (this->port_->serial_available()) {
    uint8_t discard;
    this->port_->serial_read(&discard);
  }

  const uint8_t function = this->tx_frame_[1];
  // GbbConnect2 semantics: function >= 5 && != 23 is a write -> longer gap.
  const bool is_write = function >= 5 && function != 23;
  this->next_gap_ms_ = is_write ? this->write_gap_ms_ : this->read_gap_ms_;

  // This is the raw Modbus RTU frame that goes onto the RS485 bus. Unlike
  // GbbConnect2 there is no SolarmanV5 wrapper, so this frame is exactly the
  // Modbus hex unpacked from the request line.
  char message[960];
  snprintf(message, sizeof(message), "Line %d -> inverter: %s",
           static_cast<int>(this->header_.lines[this->line_index_].line_no), frame_to_log_hex(this->tx_frame_));
  this->log_(CoreLogLevel::DEBUG, message);

  this->port_->set_transmit_enabled(true);
  this->port_->serial_write(this->tx_frame_.data(), this->tx_frame_.size());
  this->port_->serial_flush();  // blocks only for the frame TX time (~10 ms at 9600 for 8 bytes)
  this->port_->set_transmit_enabled(false);

  this->rx_frame_.clear();
  this->tx_done_at_ = this->clock_->monotonic_ms();
  this->last_rx_byte_at_ = this->tx_done_at_;
  this->state_ = State::RX_WAIT;
}

uint32_t ModbusExecutor::silence_gap_ms_() const {
  // 3.5 character times (11 bits/char -> 38.5 bits), min 5 ms for timer resolution.
  uint32_t t35 = (39 * 1000 + this->baud_rate_hint_ / 2) / this->baud_rate_hint_ + 1;
  return t35 < 5 ? 5 : t35;
}

void ModbusExecutor::handle_rx_() {
  const uint32_t now = this->clock_->monotonic_ms();

  while (this->port_->serial_available() && this->rx_frame_.size() < MAX_FRAME_SIZE) {
    uint8_t byte;
    if (!this->port_->serial_read(&byte))
      break;
    this->rx_frame_.push_back(byte);
    this->last_rx_byte_at_ = now;
  }

  // Expected-length framing once the header is in.
  if (this->rx_frame_.size() >= 3) {
    const uint8_t function = this->rx_frame_[1];
    size_t expected = 0;
    if (function & 0x80) {
      expected = 5;  // exception response
    } else if (function >= 0x01 && function <= 0x04) {
      expected = 3 + this->rx_frame_[2] + 2;
    } else if (function == 0x05 || function == 0x06 || function == 0x0F || function == 0x10) {
      expected = 8;
    }
    if (expected > 0 && this->rx_frame_.size() >= expected) {
      this->rx_frame_.resize(expected);
      this->finish_line_ok_();
      return;
    }
    // Unknown function code: fall back to end-of-frame silence detection.
    if (expected == 0 && now - this->last_rx_byte_at_ >= this->silence_gap_ms_()) {
      this->finish_line_ok_();
      return;
    }
  }

  if (now - this->tx_done_at_ >= this->response_timeout_ms_) {
    if (this->rx_frame_.empty()) {
      char message[260];
      snprintf(message, sizeof(message), "Line %d: no reply from the inverter after %u ms",
               static_cast<int>(this->header_.lines[this->line_index_].line_no),
               static_cast<unsigned>(this->response_timeout_ms_));
      this->log_(CoreLogLevel::WARN, message);
    } else {
      char message[960];
      snprintf(message, sizeof(message), "Line %d: incomplete reply after %u ms, got %u byte(s): %s",
               static_cast<int>(this->header_.lines[this->line_index_].line_no),
               static_cast<unsigned>(this->response_timeout_ms_), static_cast<unsigned>(this->rx_frame_.size()),
               frame_to_log_hex(this->rx_frame_));
      this->log_(CoreLogLevel::WARN, message);
    }
    this->fail_line_("Response timeout");
  }
}

void ModbusExecutor::finish_line_ok_() {
  if (this->rx_frame_.size() < 4) {
    char message[960];
    snprintf(message, sizeof(message), "Line %d: inverter reply too short (%u byte(s)): %s",
             static_cast<int>(this->header_.lines[this->line_index_].line_no),
             static_cast<unsigned>(this->rx_frame_.size()), frame_to_log_hex(this->rx_frame_));
    this->log_(CoreLogLevel::WARN, message);
    this->fail_line_("Response too short");
    return;
  }
  const size_t n = this->rx_frame_.size();
  const uint16_t crc = modbus_crc16(this->rx_frame_.data(), n - 2);
  const uint16_t got = static_cast<uint16_t>(static_cast<uint16_t>(this->rx_frame_[n - 2]) |
                                             static_cast<uint16_t>(this->rx_frame_[n - 1] << 8));
  if (crc != got) {
    char message[960];
    snprintf(message, sizeof(message), "Line %d <- inverter: %s (bad checksum: calculated %04X, frame says %04X)",
             static_cast<int>(this->header_.lines[this->line_index_].line_no), frame_to_log_hex(this->rx_frame_), crc,
             got);
    this->log_(CoreLogLevel::WARN, message);
    this->fail_line_("Invalid CRC in response");
    return;
  }

  GbbLine &line = this->header_.lines[this->line_index_];
  line.modbus = bytes_to_hex(this->rx_frame_.data(), n);
  char message[960];
  snprintf(message, sizeof(message), "Line %d <- inverter: %s (OK, %u byte(s))", static_cast<int>(line.line_no),
           frame_to_log_hex(this->rx_frame_), static_cast<unsigned>(n));
  this->log_(CoreLogLevel::DEBUG, message);

  this->gap_until_ = this->clock_->monotonic_ms() + this->next_gap_ms_;
  this->line_index_++;
  this->start_next_line_();
}

void ModbusExecutor::fail_line_(const char *message) {
  this->error_count_++;
  // GbbConnect2: set Error on the failing line, clear Modbus on it and every
  // subsequent line, stop the batch.
  GbbLine &line = this->header_.lines[this->line_index_];
  line.error = message;
  for (size_t i = this->line_index_; i < this->header_.lines.size(); i++) {
    this->header_.lines[i].has_modbus = false;
    this->header_.lines[i].modbus.clear();
  }
  this->gap_until_ = this->clock_->monotonic_ms() + this->next_gap_ms_;
  this->finish_all_();
}

void ModbusExecutor::finish_all_() {
  this->tx_frame_.clear();
  this->rx_frame_.clear();
  this->state_ = State::DONE;
}

void ModbusExecutor::abort_batch_() {
  char message[160];
  snprintf(message, sizeof(message), "Batch aborted at a line boundary; %u of %u line(s) not sent",
           static_cast<unsigned>(this->header_.lines.size() - this->line_index_),
           static_cast<unsigned>(this->header_.lines.size()));
  this->log_(CoreLogLevel::INFO, message);
  this->finish_all_();
}

}  // namespace gbb_dongle
}  // namespace esphome
