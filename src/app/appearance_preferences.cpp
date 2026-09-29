#include "app/appearance_preferences.hpp"

#include <Preferences.h>

namespace calendar {
namespace {

constexpr char kPreferencesNamespace[] = "calendar-ui";
constexpr char kThemeIdKey[] = "theme_v1";
constexpr char kDarkThemeKey[] = "dark";
constexpr char kBrightnessKey[] = "brightness";
constexpr char kAutomaticBrightnessKey[] = "auto_brightness";

}  // namespace

std::uint8_t clamp_brightness_percent(std::uint8_t value) {
    if (value < kMinimumBrightnessPercent) return kMinimumBrightnessPercent;
    if (value > kMaximumBrightnessPercent) return kMaximumBrightnessPercent;
    return value;
}

AppearancePreferences load_appearance_preferences() {
    AppearancePreferences result;
    Preferences preferences;
    if (!preferences.begin(kPreferencesNamespace, true)) return result;

    // Existing installations have only the legacy `dark` key. A missing or
    // invalid family therefore selects Silver Blue while retaining that saved
    // light/dark choice.
    const std::uint8_t stored_theme = preferences.getUChar(
        kThemeIdKey, static_cast<std::uint8_t>(kDefaultThemeId));
    result.theme_id = theme_id_from_raw(stored_theme);
    result.dark_theme = preferences.getBool(kDarkThemeKey, result.dark_theme);
    result.brightness_percent = clamp_brightness_percent(
        preferences.getUChar(kBrightnessKey, kDefaultBrightnessPercent));
    result.automatic_brightness = preferences.getBool(kAutomaticBrightnessKey, false);
    preferences.end();
    return result;
}

bool save_appearance_preferences(const AppearancePreferences& appearance) {
    Preferences preferences;
    if (!preferences.begin(kPreferencesNamespace, false)) return false;
    const ThemeId theme_id = theme_id_from_raw(
        static_cast<std::uint8_t>(appearance.theme_id));
    const std::uint8_t brightness = clamp_brightness_percent(appearance.brightness_percent);

    preferences.putUChar(kThemeIdKey, static_cast<std::uint8_t>(theme_id));
    // Continue writing the legacy key so rollback firmware preserves the
    // selected mode even though it does not understand theme families.
    preferences.putBool(kDarkThemeKey, appearance.dark_theme);
    preferences.putUChar(kBrightnessKey, brightness);
    preferences.putBool(kAutomaticBrightnessKey, appearance.automatic_brightness);
    const bool saved = preferences.isKey(kThemeIdKey) &&
                       preferences.isKey(kDarkThemeKey) && preferences.isKey(kBrightnessKey) &&
                       preferences.isKey(kAutomaticBrightnessKey) &&
                       preferences.getUChar(kThemeIdKey, 0xFFU) ==
                           static_cast<std::uint8_t>(theme_id) &&
                       preferences.getBool(kDarkThemeKey, !appearance.dark_theme) == appearance.dark_theme &&
                       preferences.getUChar(kBrightnessKey, 0) == brightness &&
                       preferences.getBool(kAutomaticBrightnessKey,
                                           !appearance.automatic_brightness) ==
                           appearance.automatic_brightness;
    preferences.end();
    return saved;
}

}  // namespace calendar
