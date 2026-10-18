#include "pcf8562.h"

#include <algorithm>
#include <limits>
#ifdef USE_ESP32
#include <esp_system.h>
#endif

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::pcf8562 {

static const char *const TAG = "pcf8562";

void PCF8562Display::setup() {
  // Datasheet: allow at least 1 ms after power-on before I2C transfers.
  delay(1);
  if (this->restore_from_rtc_) {
    this->frame_preference_ = global_preferences->make_preference<RetainedFrame>(this->rtc_storage_key_, false);
#ifdef USE_ESP32
    RetainedFrame saved{};
    if (esp_reset_reason() == ESP_RST_DEEPSLEEP && this->frame_preference_.load(&saved) &&
        saved.version == this->frame_version_()) {
      this->buffer_ = saved.data;
      this->restored_frame_ = true;
      ESP_LOGD(TAG, "Restored previous LCD frame from RTC; preserving display during wakeup");
    }
#endif
  }
  if (!this->restored_frame_ && !this->writer_.has_value()) {
    this->set_temperature(std::numeric_limits<float>::quiet_NaN());
    this->set_humidity(std::numeric_limits<float>::quiet_NaN());
    this->set_icon(panel::Icon::TEMPERATURE, true);
    this->set_icon(panel::Icon::HUMIDITY, true);
    this->set_icon(panel::Icon::CELSIUS, true);
    this->set_icon(panel::Icon::PERCENT, true);
  }
  this->update();
}

void PCF8562Display::prepare_content() {
  auto frame = this->buffer_;
  frame[8] &= 0xF0;
  std::fill(frame.begin() + 9, frame.end(), 0);
  this->dirty_ |= frame != this->buffer_;
  this->buffer_ = frame;
}

void PCF8562Display::set_temperature(float value) {
  const auto previous = this->buffer_;
  panel::set_temperature(this->buffer_, value);
  this->dirty_ |= previous != this->buffer_;
}

void PCF8562Display::set_humidity(float value) {
  const auto previous = this->buffer_;
  panel::set_humidity(this->buffer_, value);
  this->dirty_ |= previous != this->buffer_;
}

void PCF8562Display::set_icon(panel::Icon icon, bool on) {
  const auto previous = this->buffer_;
  panel::set_icon(this->buffer_, icon, on);
  this->dirty_ |= previous != this->buffer_;
}

void PCF8562Display::update() {
  if (this->writer_.has_value()) {
    this->writer_(*this);
  }
  this->flush();
}

void PCF8562Display::dump_config() {
  ESP_LOGCONFIG(TAG, "PCF8562 Segment LCD:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Panel: 17 SEG x 4 COM, 1:4 multiplex");
  ESP_LOGCONFIG(TAG, "  Bias: %s", this->bias_ == 0 ? "1/3" : "1/2");
  ESP_LOGCONFIG(TAG, "  Hardware Subaddress (A2:A0): %u", this->hardware_subaddress_);
  ESP_LOGCONFIG(TAG, "  Restore frame from RTC: %s", YESNO(this->restore_from_rtc_));
  LOG_UPDATE_INTERVAL(this);
}

void PCF8562Display::clear() {
  for (auto &value : this->buffer_) {
    if (value != 0) {
      value = 0;
      this->dirty_ = true;
    }
  }
}

void PCF8562Display::all_on() {
  // First eight bytes cover SEG0..15; SEG16 occupies the high nibble of byte 8.
  // Unconnected SEG17..31 stay off.
  std::array<uint8_t, 16> frame{};
  std::fill_n(frame.begin(), 8, 0xFF);
  frame[8] = 0xF0;
  if (this->buffer_ != frame) {
    this->buffer_ = frame;
    this->dirty_ = true;
  }
}

void PCF8562Display::all_ram_on() {
  for (auto &value : this->buffer_) {
    if (value != 0xFF) {
      value = 0xFF;
      this->dirty_ = true;
    }
  }
}

void PCF8562Display::show_com(uint8_t com) {
  if (com < 1 || com > 4) {
    ESP_LOGW(TAG, "Invalid COM%u (expected COM1..4)", com);
    return;
  }
  // Each byte contains the same COM bit for two consecutive SEG outputs.
  // COM1: 88, COM2: 44, COM3: 22, COM4: 11 (hex).
  const uint8_t pattern = 0x88U >> (com - 1);
  for (auto &value : this->buffer_) {
    if (value != pattern) {
      value = pattern;
      this->dirty_ = true;
    }
  }
}

void PCF8562Display::set_segment(uint8_t seg, uint8_t com, bool on) {
  if (seg > 16 || com < 1 || com > 4) {
    ESP_LOGW(TAG, "Invalid segment: SEG%u COM%u (expected SEG0..16, COM1..4)", seg, com);
    return;
  }
  const uint8_t bit = 7 - ((seg % 2) * 4 + com - 1);
  const uint8_t mask = 1U << bit;
  auto &value = this->buffer_[seg / 2];
  const uint8_t next = on ? (value | mask) : (value & ~mask);
  if (value != next) {
    value = next;
    this->dirty_ = true;
  }
}

bool PCF8562Display::write_data_(const uint8_t *data, size_t len) {
  const auto error = this->write(data, len);
  if (error != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C write failed (error %d); check address, wiring and power", static_cast<int>(error));
    this->status_set_warning();
    return false;
  }
  return true;
}

bool PCF8562Display::initialize_() {
  // Commands, not register addresses. C=1 continues commands, C=0 ends commands.
  // A retained frame is already valid: keep E=1 to avoid blanking on wakeup.
  // Cold starts still disable the output until undefined power-on RAM is replaced.
  const uint8_t commands[] = {
      static_cast<uint8_t>((this->restored_frame_ ? 0xC8 : 0xC0) | this->bias_),
      0xF0,
      static_cast<uint8_t>(0x60 | this->hardware_subaddress_),
  };
  ESP_LOGD(TAG, "Init commands: %s", format_hex_pretty(commands, sizeof(commands)).c_str());
  return this->write_data_(commands, sizeof(commands));
}

bool PCF8562Display::flush() {
  if (this->initialized_ && !this->dirty_) {
    return true;
  }
  if (!this->initialized_ && !this->initialize_()) {
    return false;
  }

  // Re-select device and RAM pointer on every transfer, including retries.
  // Loading all 32 addresses also clears unused outputs and power-on RAM contents.
  uint8_t packet[18];
  packet[0] = 0xE0 | this->hardware_subaddress_;  // Device-select, C=1.
  packet[1] = 0x00;                            // Load-data-pointer=0, C=0.
  std::copy(this->buffer_.begin(), this->buffer_.end(), packet + 2);
  ESP_LOGV(TAG, "Frame TX (device select, RAM pointer, 16 RAM bytes): %s",
           format_hex_pretty(packet, sizeof(packet)).c_str());
  if (!this->write_data_(packet, sizeof(packet))) {
    return false;
  }

  if (!this->initialized_) {
    // Enable only after a complete frame has replaced the undefined power-on RAM.
    const uint8_t enable = 0x48 | this->bias_;  // Mode-set, E=1, C=0.
    ESP_LOGD(TAG, "Display enable command: 0x%02X", enable);
    if (!this->write_data_(&enable, 1)) {
      return false;
    }
    this->initialized_ = true;
    ESP_LOGI(TAG, "LCD initialized and frame written; verify A2:A0 if segments stay blank");
  }
  this->dirty_ = false;
  if (this->restore_from_rtc_) {
    const RetainedFrame saved{this->frame_version_(), this->buffer_};
    if (!this->frame_preference_.save(&saved))
      ESP_LOGW(TAG, "Failed to retain LCD frame in RTC; check shared RTC storage capacity");
  }
  this->status_clear_warning();
  return true;
}

}  // namespace esphome::pcf8562
