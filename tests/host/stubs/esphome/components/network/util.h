// Stand-in for esphome/components/network/util.h: is_connected() returns a
// flag the test sets.
#pragma once

namespace esphome {
namespace network {
extern bool fake_connected;
inline bool is_connected() { return fake_connected; }
}  // namespace network
}  // namespace esphome
