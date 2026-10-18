#pragma once

#include <array>
#include <utility>

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/display/display.h"
#include "esphome/components/i2c/i2c.h"
#include "pcf8562_panel.h"

namespace esphome::pcf8562 {

class PCF8562Display;
using pcf8562_writer_t = display::DisplayWriter<PCF8562Display>;

// Fixed panel wiring: S0..S16 -> SEG0..SEG16, BP0..BP3 -> COM1..COM4.
class PCF8562Display : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void set_writer(pcf8562_writer_t &&writer) { this->writer_ = std::move(writer); }
  void set_bias(uint8_t bias) { this->bias_ = bias; }
  void set_hardware_subaddress(uint8_t subaddress) { this->hardware_subaddress_ = subaddress; }
  void set_rtc_storage_key(uint32_t key) { this->rtc_storage_key_ = key; this->restore_from_rtc_ = true; }

  // These methods edit the local frame; update() sends it after running the lambda.
  void clear();
  void all_on();
  // Diagnostic patterns cover all 32 chip outputs, independently of the panel map.
  void all_ram_on();
  void show_com(uint8_t com);
  // seg is zero-based (0..16); com follows the panel labels (1..4).
  void set_segment(uint8_t seg, uint8_t com, bool on);
  bool flush();
  void prepare_content();
  void set_temperature(float value);
  void set_humidity(float value);
  void set_icon(panel::Icon icon, bool on);

 protected:
  bool initialize_();
  bool write_data_(const uint8_t *data, size_t len);

  std::array<uint8_t, 16> buffer_{};
  pcf8562_writer_t writer_{};
  uint8_t bias_{0x00};
  uint8_t hardware_subaddress_{0};
  bool initialized_{false};
  bool dirty_{true};
  struct RetainedFrame {
    uint32_t version{0};
    std::array<uint8_t, 16> data{};
  };
  ESPPreferenceObject frame_preference_;
  uint32_t rtc_storage_key_{0};
  bool restore_from_rtc_{false};
  bool restored_frame_{false};
  uint32_t frame_version_() const { return 0x50434600U | this->bias_ | (this->hardware_subaddress_ << 4); }
};

}  // namespace esphome::pcf8562
