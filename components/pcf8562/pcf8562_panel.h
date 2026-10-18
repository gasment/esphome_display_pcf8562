#pragma once

#include <array>
#include <cstdint>

namespace esphome::pcf8562::panel {

using Frame = std::array<uint8_t, 16>;
struct Address {
  uint8_t seg;
  uint8_t com;
};

// Digits 1..8, segments A..G. Source: segment_map.md (user-confirmed table).
inline constexpr Address DIGITS[8][7] = {
    {{0, 1}, {0, 2}, {0, 3}, {0, 4}, {1, 4}, {1, 2}, {1, 3}},
    {{2, 1}, {2, 2}, {2, 3}, {2, 4}, {3, 3}, {3, 1}, {3, 2}},
    {{4, 1}, {4, 2}, {4, 3}, {4, 4}, {5, 4}, {5, 2}, {5, 3}},
    {{6, 1}, {6, 2}, {6, 3}, {6, 4}, {7, 4}, {7, 2}, {7, 3}},
    {{9, 4}, {9, 3}, {9, 2}, {9, 1}, {8, 2}, {8, 4}, {8, 3}},
    {{11, 4}, {11, 3}, {11, 2}, {11, 1}, {10, 2}, {10, 4}, {10, 3}},
    {{13, 4}, {13, 3}, {13, 2}, {13, 1}, {12, 2}, {12, 4}, {12, 3}},
    {{15, 4}, {15, 3}, {15, 2}, {15, 1}, {14, 2}, {14, 4}, {14, 3}},
};

enum class Icon : uint8_t {
  PERCENT, HUMIDITY_DOT, TEMPERATURE, HUMIDITY, BLUETOOTH, WIFI,
  TEMPERATURE_DOT, CELSIUS, BATTERY_OUTLINE, BATTERY_RIGHT, BATTERY_MIDDLE, BATTERY_LEFT,
};

// Icons S1..S12.
inline constexpr Address ICONS[12] = {
    {1, 1}, {3, 4}, {5, 1}, {7, 1}, {8, 1}, {10, 1},
    {12, 1}, {14, 1}, {16, 1}, {16, 4}, {16, 2}, {16, 3},
};

// Bit0=A, bit1=B, ... bit6=G.
inline constexpr uint8_t NUMBERS[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
inline constexpr uint8_t MINUS = 0x40;

constexpr void set(Frame &frame, Address address, bool on) {
  const uint8_t mask = 1U << (7 - ((address.seg % 2) * 4 + address.com - 1));
  auto &value = frame[address.seg / 2];
  value = on ? value | mask : value & ~mask;
}

constexpr void set_icon(Frame &frame, Icon icon, bool on) {
  set(frame, ICONS[static_cast<uint8_t>(icon)], on);
}

constexpr void set_digit(Frame &frame, uint8_t digit, uint8_t segments) {
  for (uint8_t i = 0; i < 7; i++)
    set(frame, DIGITS[digit - 1][i], (segments & (1U << i)) != 0);
}

// Positions in visual left-to-right order, independent of the panel's numbering.
constexpr void write_row(Frame &frame, bool temperature, const std::array<uint8_t, 4> &segments, bool dot) {
  for (uint8_t i = 0; i < 4; i++)
    set_digit(frame, temperature ? 5 + i : 4 - i, segments[i]);
  set_icon(frame, temperature ? Icon::TEMPERATURE_DOT : Icon::HUMIDITY_DOT, dot);
}

constexpr void invalid(Frame &frame, bool temperature) {
  write_row(frame, temperature, {MINUS, MINUS, MINUS, MINUS}, false);
}

constexpr void set_temperature(Frame &frame, float value) {
  // Bounds also reject infinities before any float-to-integer conversion.
  if (value != value || value <= -99.5f || value >= 100.0f) {
    invalid(frame, true);
    return;
  }
  if (value < 0) {
    const unsigned magnitude = static_cast<unsigned>(-value + 0.5f);
    std::array<uint8_t, 4> digits{};
    digits[3] = NUMBERS[magnitude % 10];
    if (magnitude >= 10) {
      digits[1] = MINUS;
      digits[2] = NUMBERS[magnitude / 10];
    } else if (magnitude != 0) {
      digits[2] = MINUS;
    }
    write_row(frame, true, digits, false);
    return;
  }
  // Float multiplication is intentional: round at the sensor's float precision.
  const unsigned scaled = static_cast<unsigned>(value * 100.0f + 0.5f);
  if (scaled >= 10000) {
    invalid(frame, true);
    return;
  }
  write_row(frame, true,
            {scaled >= 1000 ? NUMBERS[scaled / 1000] : uint8_t{0}, NUMBERS[(scaled / 100) % 10],
             NUMBERS[(scaled / 10) % 10], NUMBERS[scaled % 10]}, true);
}

constexpr void set_humidity(Frame &frame, float value) {
  if (value != value || value < 0 || value > 100.0f) {
    invalid(frame, false);
    return;
  }
  const unsigned scaled = static_cast<unsigned>(value * 100.0f + 0.5f);
  if (scaled >= 10000) {
    write_row(frame, false, {0, NUMBERS[1], NUMBERS[0], NUMBERS[0]}, false);
    return;
  }
  write_row(frame, false,
            {scaled >= 1000 ? NUMBERS[scaled / 1000] : uint8_t{0}, NUMBERS[(scaled / 100) % 10],
             NUMBERS[(scaled / 10) % 10], NUMBERS[scaled % 10]}, true);
}

}  // namespace esphome::pcf8562::panel
