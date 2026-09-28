#pragma once

#include "app/theme_palette.hpp"

#include <cstdint>

namespace calendar {

// Appearance is intentionally independent from calendar and Wi-Fi credentials.
// There is no ambient-light input on this board; brightness is always manual.
struct AppearancePreferences {
    ThemeId theme_id = kDefaultThemeId;
    bool dark_theme = false;
    std::uint8_t brightness_percent = 75;
};

constexpr std::uint8_t kMinimumBrightnessPercent = 10;
constexpr std::uint8_t kMaximumBrightnessPercent = 100;
constexpr std::uint8_t kDefaultBrightnessPercent = 75;

std::uint8_t clamp_brightness_percent(std::uint8_t value);
AppearancePreferences load_appearance_preferences();
bool save_appearance_preferences(const AppearancePreferences& preferences);

}  // namespace calendar
