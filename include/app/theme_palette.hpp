#pragma once

#include <cstdint>

namespace calendar {

// Values are persisted in NVS. Append new themes before Count and never
// renumber an existing entry.
enum class ThemeId : std::uint8_t {
  ModernQuiet = 0,
  SwissGrid = 1,
  BauhausPrimary = 2,
  NordicQuiet = 3,
  SilverBlue = 4,
  Sage = 5,
  Lavender = 6,
  WarmSand = 7,
  Count = 8,
};

inline constexpr ThemeId kDefaultThemeId = ThemeId::SilverBlue;
constexpr std::uint8_t kThemeCount = static_cast<std::uint8_t>(ThemeId::Count);

constexpr bool is_valid_theme_id(ThemeId theme) {
  return static_cast<std::uint8_t>(theme) < kThemeCount;
}

// Semantic colors shared by every theme family. Consumers select colors by
// role so each light/dark appearance remains complete and consistent.
struct ThemePalette {
  std::uint32_t canvas;
  std::uint32_t surface;
  std::uint32_t surface_subtle;
  std::uint32_t surface_muted;
  std::uint32_t input_inset;
  std::uint32_t header;
  std::uint32_t on_header;
  std::uint32_t text_primary;
  std::uint32_t text_secondary;
  std::uint32_t text_muted;
  std::uint32_t border;
  std::uint32_t divider;
  std::uint32_t action;
  std::uint32_t on_action;
  std::uint32_t action_pressed;
  std::uint32_t selection;
  std::uint32_t on_selection;
  std::uint32_t action_soft;
  std::uint32_t focus;
  std::uint32_t today_fill;
  std::uint32_t today_text;
  std::uint32_t today_ring;
  std::uint32_t status_normal;
  std::uint32_t status_attention;
  std::uint32_t status_error;
};

enum class SurfaceGrammar : std::uint8_t {
  LayeredCards = 0,
  RuledGrid = 1,
  StrongBlocks = 2,
  OpenSurfaces = 3,
};

enum class NavigationGrammar : std::uint8_t {
  AccentUnderline = 0,
  RuledUnderline = 1,
  FilledTabAndUnderline = 2,
  SoftSelectedTab = 3,
};

enum class ForecastWellGrammar : std::uint8_t {
  CurrentOnly = 0,
  None = 1,
  GeometricCurrent = 2,
  Open = 3,
};

// Compact component grammar. Layout and touch-target bounds remain shared by
// every theme; these values only tune styles inside those fixed bounds.
struct ThemeStyleRecipe {
  std::uint8_t card_radius;
  std::uint8_t control_radius;
  std::uint8_t input_radius;
  std::uint8_t structural_border_width;
  std::uint8_t divider_width;
  std::uint8_t focus_width;
  std::uint8_t content_padding;
  std::uint8_t row_gap;
  std::uint8_t nav_indicator_width;
  std::uint8_t nav_indicator_height;
  std::uint8_t event_marker_size;
  std::uint8_t event_marker_radius;
  std::int8_t ascii_title_letter_space;
  std::uint8_t grammar_bits;

  constexpr SurfaceGrammar surface_grammar() const {
    return static_cast<SurfaceGrammar>(grammar_bits & 0x03U);
  }

  constexpr NavigationGrammar navigation_grammar() const {
    return static_cast<NavigationGrammar>((grammar_bits >> 2U) & 0x03U);
  }

  constexpr ForecastWellGrammar forecast_well_grammar() const {
    return static_cast<ForecastWellGrammar>((grammar_bits >> 4U) & 0x03U);
  }
};

static_assert(sizeof(ThemeStyleRecipe) <= 16U,
              "Theme recipes must remain a compact flash-resident table");

// Compatibility alias for code written against the initial radius-only API.
using ThemeMetrics = ThemeStyleRecipe;

ThemeId theme_id_from_raw(std::uint8_t raw_theme_id);
const char *theme_name(ThemeId theme);
const ThemeStyleRecipe &theme_style_recipe(ThemeId theme);
const ThemeMetrics &theme_metrics(ThemeId theme);
const ThemePalette &palette_for(ThemeId theme, bool dark_theme);

// Compatibility accessors retain the existing Modern Quiet behavior.
const ThemePalette &light_palette();
const ThemePalette &dark_palette();
const ThemePalette &palette_for(bool dark_theme);

} // namespace calendar
