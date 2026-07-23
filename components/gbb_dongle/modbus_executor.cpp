#include "modbus_executor.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gbb_dongle {

static const char *const TAG = "gbb_dongle.modbus";

// Response frames: 1 addr + 1 fn + 250 data max + 2 CRC, with headroom.
static const size_t MAX_FRAME_SIZE = 300;

// Space-separated uppercase hex for the human-readable debug log lines, e.g.
// "01 03 02 04 00 03 45 B2". Easier to eyeball byte-by-byte than a run-on
// hex string.
static std::string frame_to_log_hex(const std::vector<uint8_t> &frame) {
  static const char HEX[] = "0123456789ABCDEF";
  std::string out;
  if (frame.empty())
    return out;
  out.reserve(frame.size() * 3 - 1);
  for (size_t i = 0; i < frame.size(); i++) {
    if (i != 0)
      out.push_back(' ');
    out.push_back(HEX[frame[i] >> 4]);
    out.push_back(HEX[frame[i] & 0x0F]);
  }
  return out;
}

void ModbusExecutor::start(GbbHeader &&header) {
  this->header_ = std::move(header);
  this->line_index_ = 0;
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
      if (millis() >= this->gap_until_)
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
    ESP_LOGW(TAG, "Line %" PRId32 ": the request carried an invalid Modbus frame ('%s'); skipping this and the rest",
             line.line_no, line.modbus.c_str());
    this->fail_line_("Invalid Modbus hex string");
    return;
  }
  this->state_ = State::GAP;  // gap_until_ still holds the deadline from the previous command
}

void ModbusExecutor::transmit_current_() {
  // Drain any stale bytes so the response frame starts clean.
  while (this->uart_->available()) {
    uint8_t discard;
    this->uart_->read_byte(&discard);
  }

  const uint8_t function = this->tx_frame_[1];
  // GbbConnect2 semantics: function >= 5 && != 23 is a write -> longer gap.
  const bool is_write = function >= 5 && function != 23;
  this->next_gap_ms_ = is_write ? this->write_gap_ms_ : this->read_gap_ms_;

  // This is the raw Modbus RTU frame that goes onto the RS485 bus. Unlike
  // GbbConnect2 there is no SolarmanV5 wrapper, so this frame is exactly the
  // Modbus hex unpacked from the request line.
  ESP_LOGD(TAG, "Line %" PRId32 " -> inverter: %s", this->header_.lines[this->line_index_].line_no,
           frame_to_log_hex(this->tx_frame_).c_str());

  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(true);
  this->uart_->write_array(this->tx_frame_);
  this->uart_->flush();  // blocks only for the frame TX time (~10 ms at 9600 for 8 bytes)
  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(false);

  this->rx_frame_.clear();
  this->tx_done_at_ = millis();
  this->last_rx_byte_at_ = this->tx_done_at_;
  this->state_ = State::RX_WAIT;
}

uint32_t ModbusExecutor::silence_gap_ms_() const {
  // 3.5 character times (11 bits/char -> 38.5 bits), min 5 ms for timer resolution.
  uint32_t t35 = (39 * 1000 + this->baud_rate_hint_ / 2) / this->baud_rate_hint_ + 1;
  return t35 < 5 ? 5 : t35;
}

void ModbusExecutor::handle_rx_() {
  const uint32_t now = millis();

  while (this->uart_->available() && this->rx_frame_.size() < MAX_FRAME_SIZE) {
    uint8_t byte;
    if (!this->uart_->read_byte(&byte))
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
      ESP_LOGW(TAG,
               "Line %" PRId32 ": no reply from the inverter after %" PRIu32 " ms. "
               "Check the RS485 wiring (A/B may be swapped), the baud rate and the parity.",
               this->header_.lines[this->line_index_].line_no, this->response_timeout_ms_);
    } else {
      ESP_LOGW(TAG,
               "Line %" PRId32 ": incomplete reply after %" PRIu32 " ms, got %u byte(s): %s",
               this->header_.lines[this->line_index_].line_no, this->response_timeout_ms_,
               this->rx_frame_.size(), frame_to_log_hex(this->rx_frame_).c_str());
    }
    this->fail_line_("Response timeout");
  }
}

void ModbusExecutor::finish_line_ok_() {
  if (this->rx_frame_.size() < 4) {
    ESP_LOGW(TAG, "Line %" PRId32 ": inverter reply too short to be valid (%u byte(s)): %s",
             this->header_.lines[this->line_index_].line_no, this->rx_frame_.size(),
             frame_to_log_hex(this->rx_frame_).c_str());
    this->fail_line_("Response too short");
    return;
  }
  const size_t n = this->rx_frame_.size();
  const uint16_t crc = modbus_crc16(this->rx_frame_.data(), n - 2);
  const uint16_t got = static_cast<uint16_t>(this->rx_frame_[n - 2]) | (static_cast<uint16_t>(this->rx_frame_[n - 1]) << 8);
  if (crc != got) {
    ESP_LOGW(TAG, "Line %" PRId32 " <- inverter: %s (bad checksum: calculated %04X, frame says %04X)",
             this->header_.lines[this->line_index_].line_no, frame_to_log_hex(this->rx_frame_).c_str(), crc, got);
    this->fail_line_("Invalid CRC in response");
    return;
  }

  GbbLine &line = this->header_.lines[this->line_index_];
  line.modbus = bytes_to_hex(this->rx_frame_.data(), n);
  ESP_LOGD(TAG, "Line %" PRId32 " <- inverter: %s (OK, %u byte(s))", line.line_no,
           frame_to_log_hex(this->rx_frame_).c_str(), n);

  this->gap_until_ = millis() + this->next_gap_ms_;
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
  this->gap_until_ = millis() + this->next_gap_ms_;
  this->finish_all_();
}

void ModbusExecutor::finish_all_() {
  this->tx_frame_.clear();
  this->rx_frame_.clear();
  this->state_ = State::DONE;
}

}  // namespace gbb_dongle
}  // namespace esphome
