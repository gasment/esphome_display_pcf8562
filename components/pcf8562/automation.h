#pragma once

#include "esphome/core/automation.h"
#include "pcf8562.h"

namespace esphome::pcf8562 {

template<typename... Ts> class ContentSetAction : public Action<Ts...> {
 public:
  explicit ContentSetAction(PCF8562Display *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, temperature)
  TEMPLATABLE_VALUE(float, humidity)
  TEMPLATABLE_VALUE(bool, wifi)
  TEMPLATABLE_VALUE(bool, bluetooth)
  TEMPLATABLE_VALUE(bool, battery_outline)
  TEMPLATABLE_VALUE(bool, battery_left)
  TEMPLATABLE_VALUE(bool, battery_middle)
  TEMPLATABLE_VALUE(bool, battery_right)
  TEMPLATABLE_VALUE(bool, temperature_icon)
  TEMPLATABLE_VALUE(bool, humidity_icon)
  TEMPLATABLE_VALUE(bool, celsius)
  TEMPLATABLE_VALUE(bool, percent)

  void play(const Ts &...x) override {
    this->parent_->prepare_content();
    if (this->temperature_.has_value())
      this->parent_->set_temperature(this->temperature_.value(x...));
    if (this->humidity_.has_value())
      this->parent_->set_humidity(this->humidity_.value(x...));
    if (this->wifi_.has_value())
      this->parent_->set_icon(panel::Icon::WIFI, this->wifi_.value(x...));
    if (this->bluetooth_.has_value())
      this->parent_->set_icon(panel::Icon::BLUETOOTH, this->bluetooth_.value(x...));
    if (this->battery_outline_.has_value())
      this->parent_->set_icon(panel::Icon::BATTERY_OUTLINE, this->battery_outline_.value(x...));
    if (this->battery_left_.has_value())
      this->parent_->set_icon(panel::Icon::BATTERY_LEFT, this->battery_left_.value(x...));
    if (this->battery_middle_.has_value())
      this->parent_->set_icon(panel::Icon::BATTERY_MIDDLE, this->battery_middle_.value(x...));
    if (this->battery_right_.has_value())
      this->parent_->set_icon(panel::Icon::BATTERY_RIGHT, this->battery_right_.value(x...));
    if (this->temperature_icon_.has_value())
      this->parent_->set_icon(panel::Icon::TEMPERATURE, this->temperature_icon_.value(x...));
    if (this->humidity_icon_.has_value())
      this->parent_->set_icon(panel::Icon::HUMIDITY, this->humidity_icon_.value(x...));
    if (this->celsius_.has_value())
      this->parent_->set_icon(panel::Icon::CELSIUS, this->celsius_.value(x...));
    if (this->percent_.has_value())
      this->parent_->set_icon(panel::Icon::PERCENT, this->percent_.value(x...));
    this->parent_->flush();
  }

 protected:
  PCF8562Display *parent_;
};

}  // namespace esphome::pcf8562

