#pragma once

#include "core_interfaces.h"

namespace esphome {
namespace gbb_dongle {

class NvsBlobStore final : public BlobStore {
 public:
  bool load(std::string &value) override;
  bool save(const std::string &value) override;
  bool erase() override;
};

}  // namespace gbb_dongle
}  // namespace esphome
