#pragma once

#include <vector>

#include "esphome/components/uart/uart.h"
#include "esphome/core/gpio.h"

#include "gbb_protocol.h"

namespace esphome {
namespace gbb_dongle {

/// Executes the Modbus lines of one GbbHeader over RS485, one frame at a
/// time, without ever blocking the main loop. Error semantics follow
/// GbbConnect2: on a line failure the line gets Error set, the Modbus of that
/// line and all subsequent lines is cleared and processing stops.
class ModbusExecutor {
 public:
  void set_uart(uart::UARTDevice *uart) { this->uart_ = uart; }
  void set_flow_control_pin(GPIOPin *pin) { this->flow_control_pin_ = pin; }
  void set_response_timeout(uint32_t ms) { this->response_timeout_ms_ = ms; }
  void set_read_gap(uint32_t ms) { this->read_gap_ms_ = ms; }
  void set_write_gap(uint32_t ms) { this->write_gap_ms_ = ms; }
  void set_baud_rate_hint(uint32_t baud) { this->baud_rate_hint_ = baud; }

  bool busy() const { return this->state_ != State::IDLE && this->state_ != State::DONE; }
  bool has_result() const { return this->state_ == State::DONE; }

  /// Take ownership of a parsed request and start executing its lines.
  void start(GbbHeader &&header);
  /// Move the finished header out; resets to IDLE.
  GbbHeader take_result();

  void loop();

  uint32_t get_error_count() const { return this->error_count_; }

 protected:
  enum class State : uint8_t { IDLE, GAP, TRANSMIT, RX_WAIT, DONE };

  void start_next_line_();
  void transmit_current_();
  void handle_rx_();
  void finish_line_ok_();
  void fail_line_(const char *message);
  void finish_all_();
  uint32_t silence_gap_ms_() const;

  uart::UARTDevice *uart_{nullptr};
  GPIOPin *flow_control_pin_{nullptr};
  uint32_t response_timeout_ms_{1000};
  uint32_t read_gap_ms_{100};
  uint32_t write_gap_ms_{3000};
  uint32_t baud_rate_hint_{9600};

  State state_{State::IDLE};
  GbbHeader header_;
  size_t line_index_{0};
  std::vector<uint8_t> tx_frame_;
  std::vector<uint8_t> rx_frame_;
  uint32_t gap_until_{0};
  uint32_t tx_done_at_{0};
  uint32_t last_rx_byte_at_{0};
  uint32_t next_gap_ms_{0};  // required silence before the *next* command
  uint32_t error_count_{0};
};

}  // namespace gbb_dongle
}  // namespace esphome
