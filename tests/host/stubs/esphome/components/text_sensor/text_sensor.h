// Stand-in for ESPHome's text sensor: keeps the current state and every
// published value, in order.
#pragma once
#include <string>
#include <vector>

namespace esphome {
namespace text_sensor {

class TextSensor {
 public:
  std::string state;
  std::vector<std::string> history;
  bool has_state() const { return !this->history.empty(); }
  void publish_state(const std::string &s) {
    this->state = s;
    this->history.push_back(s);
  }
};

}  // namespace text_sensor
}  // namespace esphome
