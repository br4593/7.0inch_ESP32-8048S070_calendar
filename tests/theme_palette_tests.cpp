#include "app/theme_palette.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace {

int failures = 0;
const char *contrast_theme = nullptr;
bool contrast_dark = false;
bool contrast_rgb565 = false;
#define CHECK(...)                                                             \
  do {                                                                         \
    if (!(__VA_ARGS__)) {                                                      \
      ++failures;                                                              \
      std::cerr << __FUNCTION__ << ": " #__VA_ARGS__ " failed at "             \
                << __LINE__;                                                   \
      if (contrast_theme != nullptr) {                                         \
        std::cerr << " [" << contrast_theme                                    \
                  << (contrast_dark ? " dark" : " light")                      \
                  << (contrast_rgb565 ? ", RGB565" : ", RGB888") << ']';       \
      }                                                                        \
      std::cerr << '\n';                                                       \
    }                                                                          \
  } while (false)

double linear_channel(std::uint8_t value) {
  const double channel = static_cast<double>(value) / 255.0;
  return channel <= 0.04045 ? channel / 12.92
                            : std::pow((channel + 0.055) / 1.055, 2.4);
}

double luminance(std::uint32_t color) {
  return 0.2126 * linear_channel(static_cast<std::uint8_t>(color >> 16U)) +
         0.7152 * linear_channel(static_cast<std::uint8_t>(color >> 8U)) +
         0.0722 * linear_channel(static_cast<std::uint8_t>(color));
}

double contrast_ratio(std::uint32_t first, std::uint32_t second) {
  const double first_luminance = luminance(first);
  const double second_luminance = luminance(second);
  const double lighter = std::max(first_luminance, second_luminance);
  const double darker = std::min(first_luminance, second_luminance);
  return (lighter + 0.05) / (darker + 0.05);
}

std::uint32_t rgb565_round_trip(std::uint32_t color) {
  const std::uint8_t red5 = static_cast<std::uint8_t>((color >> 19U) & 0x1FU);
  const std::uint8_t green6 = static_cast<std::uint8_t>((color >> 10U) & 0x3FU);
  const std::uint8_t blue5 = static_cast<std::uint8_t>((color >> 3U) & 0x1FU);
  const std::uint8_t red8 =
      static_cast<std::uint8_t>((red5 << 3U) | (red5 >> 2U));
  const std::uint8_t green8 =
      static_cast<std::uint8_t>((green6 << 2U) | (green6 >> 4U));
  const std::uint8_t blue8 =
      static_cast<std::uint8_t>((blue5 << 3U) | (blue5 >> 2U));
  return (static_cast<std::uint32_t>(red8) << 16U) |
         (static_cast<std::uint32_t>(green8) << 8U) |
         static_cast<std::uint32_t>(blue8);
}

void check_contrast_contract(calendar::ThemeId theme, bool dark,
                             bool quantize_to_rgb565) {
  constexpr double kMinimumContrast = 4.5;
  constexpr double kMinimumNonTextContrast = 3.0;
  contrast_theme = calendar::theme_name(theme);
  contrast_dark = dark;
  contrast_rgb565 = quantize_to_rgb565;
  const calendar::ThemePalette &palette = calendar::palette_for(theme, dark);
  const calendar::ThemeStyleRecipe &recipe =
      calendar::theme_style_recipe(theme);
  const auto color = [quantize_to_rgb565](std::uint32_t value) {
    return quantize_to_rgb565 ? rgb565_round_trip(value) : value;
  };
  const auto contrast = [&color](std::uint32_t first, std::uint32_t second) {
    return contrast_ratio(color(first), color(second));
  };
  CHECK(contrast(palette.text_primary, palette.surface) >= kMinimumContrast);
  CHECK(contrast(palette.text_secondary, palette.surface) >= kMinimumContrast);
  CHECK(contrast(palette.text_muted, palette.surface) >= kMinimumContrast);
  CHECK(contrast(palette.text_primary, palette.surface_subtle) >=
        kMinimumContrast);
  CHECK(contrast(palette.text_secondary, palette.surface_subtle) >=
        kMinimumContrast);
  CHECK(contrast(palette.text_muted, palette.surface_subtle) >=
        kMinimumContrast);
  CHECK(contrast(palette.text_primary, palette.action_soft) >=
        kMinimumContrast);
  CHECK(contrast(palette.text_secondary, palette.action_soft) >=
        kMinimumContrast);
  CHECK(contrast(palette.text_muted, palette.action_soft) >= kMinimumContrast);
  CHECK(contrast(palette.text_secondary, palette.input_inset) >=
        kMinimumContrast);
  CHECK(contrast(palette.on_action, palette.action) >= kMinimumContrast);
  CHECK(contrast(palette.on_action, palette.action_pressed) >=
        kMinimumContrast);
  CHECK(contrast(palette.on_selection, palette.selection) >= kMinimumContrast);
  CHECK(contrast(palette.today_fill, palette.today_text) >= kMinimumContrast);
  CHECK(contrast(palette.header, palette.on_header) >= kMinimumContrast);
  CHECK(contrast(palette.header, palette.status_normal) >= kMinimumContrast);
  CHECK(contrast(palette.header, palette.status_attention) >= kMinimumContrast);
  CHECK(contrast(palette.header, palette.status_error) >= kMinimumContrast);
  CHECK(contrast(palette.focus, palette.input_inset) >=
        kMinimumNonTextContrast);
  CHECK(contrast(palette.today_ring, palette.today_fill) >=
        kMinimumNonTextContrast);

  // Primary navigation can appear on any page surface. Soft selected tabs use
  // primary text on the soft fill and retain the accent underline.
  CHECK(contrast(palette.action, palette.canvas) >= kMinimumContrast);
  CHECK(contrast(palette.action, palette.surface) >= kMinimumContrast);
  if (recipe.navigation_grammar() ==
      calendar::NavigationGrammar::SoftSelectedTab) {
    CHECK(contrast(palette.text_primary, palette.action_soft) >=
          kMinimumContrast);
    CHECK(contrast(palette.action, palette.action_soft) >=
          kMinimumNonTextContrast);
  } else {
    CHECK(contrast(palette.action, palette.action_soft) >= kMinimumContrast);
  }

  // Selected Pressed keeps the selected fill. Today and the persistent
  // selected outline use on_selection, so selection never relies on a low
  // contrast fill against the adjacent cell alone.
  CHECK(contrast(palette.on_selection, palette.selection) >= kMinimumContrast);
  CHECK(std::max(contrast(palette.focus, palette.selection),
                 contrast(palette.on_selection, palette.selection)) >=
        kMinimumNonTextContrast);
  contrast_theme = nullptr;
}

void test_selection_and_exact_tokens() {
  const calendar::ThemePalette &light = calendar::light_palette();
  const calendar::ThemePalette &dark = calendar::dark_palette();
  CHECK(&calendar::palette_for(false) == &light);
  CHECK(&calendar::palette_for(true) == &dark);
  CHECK(light.canvas == 0xF4F6F7U && light.surface == 0xFFFFFFU &&
        light.surface_subtle == 0xF7F9FAU && light.surface_muted == 0xE9EFF1U &&
        light.input_inset == 0xEEF2F3U && light.header == 0x142A33U &&
        light.on_header == 0xFFFFFFU && light.text_primary == 0x122B34U &&
        light.text_secondary == 0x38535DU && light.text_muted == 0x556D75U &&
        light.border == 0x7A9098U && light.divider == 0xC4D0D4U &&
        light.action == 0x0D7383U && light.on_action == 0xFFFFFFU &&
        light.action_pressed == 0x0E5A66U && light.selection == 0x147A8AU &&
        light.on_selection == 0xFFFFFFU && light.action_soft == 0xDCECEFU &&
        light.focus == 0x0A7184U && light.today_fill == 0xFFF0D5U &&
        light.today_text == 0x704400U && light.today_ring == 0xA56600U &&
        light.status_normal == 0xBFE7ECU &&
        light.status_attention == 0xFFD486U && light.status_error == 0xFFB7AEU);
  CHECK(dark.canvas == 0x0E161BU && dark.surface == 0x141E24U &&
        dark.surface_subtle == 0x19262DU && dark.surface_muted == 0x111B21U &&
        dark.input_inset == 0x0F181DU && dark.header == 0x080F13U &&
        dark.on_header == 0xFFFFFFU && dark.text_primary == 0xF1F6F8U &&
        dark.text_secondary == 0xD3E0E4U && dark.text_muted == 0xA4B7BEU &&
        dark.border == 0x5A717AU && dark.divider == 0x2A3B43U &&
        dark.action == 0x5BC2D5U && dark.on_action == 0x071419U &&
        dark.action_pressed == 0x3196AAU && dark.selection == 0x285C69U &&
        dark.on_selection == 0xFFFFFFU && dark.action_soft == 0x213B43U &&
        dark.focus == 0x73D3E3U && dark.today_fill == 0x493A20U &&
        dark.today_text == 0xFFD58CU && dark.today_ring == 0xDEA34EU &&
        dark.status_normal == 0xBDEAF0U && dark.status_attention == 0xFFD58CU &&
        dark.status_error == 0xFFB7AEU);
}

void test_stable_ids_names_and_metrics() {
  using calendar::ThemeId;
  CHECK(static_cast<std::uint8_t>(ThemeId::ModernQuiet) == 0U);
  CHECK(static_cast<std::uint8_t>(ThemeId::SwissGrid) == 1U);
  CHECK(static_cast<std::uint8_t>(ThemeId::BauhausPrimary) == 2U);
  CHECK(static_cast<std::uint8_t>(ThemeId::NordicQuiet) == 3U);
  CHECK(static_cast<std::uint8_t>(ThemeId::SilverBlue) == 4U);
  CHECK(static_cast<std::uint8_t>(ThemeId::Sage) == 5U);
  CHECK(static_cast<std::uint8_t>(ThemeId::Lavender) == 6U);
  CHECK(static_cast<std::uint8_t>(ThemeId::WarmSand) == 7U);
  CHECK(calendar::kThemeCount == 8U);
  CHECK(calendar::kDefaultThemeId == ThemeId::SilverBlue);

  constexpr std::array<ThemeId, calendar::kThemeCount> themes{
      ThemeId::ModernQuiet, ThemeId::SwissGrid,  ThemeId::BauhausPrimary,
      ThemeId::NordicQuiet, ThemeId::SilverBlue, ThemeId::Sage,
      ThemeId::Lavender,    ThemeId::WarmSand};
  constexpr std::array<const char *, calendar::kThemeCount> names{
      "Modern Quiet", "Swiss Grid", "Bauhaus Primary", "Nordic Quiet",
      "Silver Blue",  "Sage",       "Lavender",        "Warm Sand"};
  for (std::size_t index = 0; index < themes.size(); ++index) {
    CHECK(calendar::is_valid_theme_id(themes[index]));
    CHECK(calendar::theme_id_from_raw(
              static_cast<std::uint8_t>(themes[index])) == themes[index]);
    CHECK(std::strcmp(calendar::theme_name(themes[index]), names[index]) == 0);
    const calendar::ThemeStyleRecipe &recipe =
        calendar::theme_style_recipe(themes[index]);
    CHECK(&calendar::theme_metrics(themes[index]) == &recipe);
    CHECK(recipe.card_radius <= 12U);
    CHECK(recipe.control_radius <= 12U);
    CHECK(recipe.input_radius <= 12U);
    CHECK(recipe.structural_border_width <= 2U);
    CHECK(recipe.divider_width >= 1U && recipe.divider_width <= 2U);
    CHECK(recipe.focus_width >= 3U && recipe.focus_width <= 4U);
    CHECK(recipe.content_padding >= 8U && recipe.content_padding <= 16U);
    CHECK(recipe.row_gap <= 8U);
    CHECK(recipe.nav_indicator_width >= 48U &&
          recipe.nav_indicator_width <= 80U);
    CHECK(recipe.nav_indicator_height >= 3U &&
          recipe.nav_indicator_height <= 4U);
    CHECK(recipe.event_marker_size >= 10U && recipe.event_marker_size <= 14U);
    CHECK(recipe.event_marker_radius <= recipe.event_marker_size / 2U);
    CHECK(recipe.ascii_title_letter_space >= 0 &&
          recipe.ascii_title_letter_space <= 1);
    CHECK(static_cast<std::uint8_t>(recipe.surface_grammar()) <=
          static_cast<std::uint8_t>(calendar::SurfaceGrammar::OpenSurfaces));
    CHECK(static_cast<std::uint8_t>(recipe.navigation_grammar()) <=
          static_cast<std::uint8_t>(
              calendar::NavigationGrammar::SoftSelectedTab));
    CHECK(static_cast<std::uint8_t>(recipe.forecast_well_grammar()) <=
          static_cast<std::uint8_t>(calendar::ForecastWellGrammar::Open));
    CHECK(&calendar::palette_for(themes[index], false) !=
          &calendar::palette_for(themes[index], true));
  }

  CHECK(!calendar::is_valid_theme_id(
      static_cast<ThemeId>(calendar::kThemeCount)));
  CHECK(!calendar::is_valid_theme_id(static_cast<ThemeId>(255U)));
  CHECK(calendar::theme_id_from_raw(calendar::kThemeCount) ==
        ThemeId::SilverBlue);
  CHECK(calendar::theme_id_from_raw(255U) == ThemeId::SilverBlue);
  const ThemeId invalid = static_cast<ThemeId>(255U);
  CHECK(std::strcmp(calendar::theme_name(invalid), "Silver Blue") == 0);
  CHECK(&calendar::theme_metrics(invalid) ==
        &calendar::theme_metrics(ThemeId::SilverBlue));
  CHECK(&calendar::theme_style_recipe(invalid) ==
        &calendar::theme_style_recipe(ThemeId::SilverBlue));
  CHECK(&calendar::palette_for(invalid, false) ==
        &calendar::palette_for(ThemeId::SilverBlue, false));
}

void check_exact_recipe(const calendar::ThemeStyleRecipe &actual,
                        const calendar::ThemeStyleRecipe &expected) {
  CHECK(actual.card_radius == expected.card_radius);
  CHECK(actual.control_radius == expected.control_radius);
  CHECK(actual.input_radius == expected.input_radius);
  CHECK(actual.structural_border_width == expected.structural_border_width);
  CHECK(actual.divider_width == expected.divider_width);
  CHECK(actual.focus_width == expected.focus_width);
  CHECK(actual.content_padding == expected.content_padding);
  CHECK(actual.row_gap == expected.row_gap);
  CHECK(actual.nav_indicator_width == expected.nav_indicator_width);
  CHECK(actual.nav_indicator_height == expected.nav_indicator_height);
  CHECK(actual.event_marker_size == expected.event_marker_size);
  CHECK(actual.event_marker_radius == expected.event_marker_radius);
  CHECK(actual.ascii_title_letter_space == expected.ascii_title_letter_space);
  CHECK(actual.grammar_bits == expected.grammar_bits);
}

void test_exact_style_recipes() {
  using calendar::ThemeId;
  CHECK(sizeof(calendar::ThemeStyleRecipe) == 14U);
  CHECK(sizeof(calendar::ThemeStyleRecipe) <= 16U);

  constexpr calendar::ThemeStyleRecipe modern{8, 8,  8, 1,  1, 4, 12,
                                              6, 64, 3, 12, 6, 0, 0x00};
  constexpr calendar::ThemeStyleRecipe swiss{2, 2,  2, 1,  1, 3, 8,
                                             0, 80, 3, 10, 0, 1, 0x15};
  constexpr calendar::ThemeStyleRecipe bauhaus{0, 0,  0, 2,  2, 3, 12,
                                               6, 64, 4, 14, 0, 1, 0x2A};
  constexpr calendar::ThemeStyleRecipe nordic{8, 8,  8, 0,  1, 3, 16,
                                              8, 48, 4, 10, 5, 0, 0x3F};
  constexpr calendar::ThemeStyleRecipe balanced{12, 8,  8, 1,  1, 3, 16,
                                                6,  48, 3, 10, 5, 0, 0x0C};

  check_exact_recipe(calendar::theme_style_recipe(ThemeId::ModernQuiet),
                     modern);
  check_exact_recipe(calendar::theme_style_recipe(ThemeId::SwissGrid), swiss);
  check_exact_recipe(calendar::theme_style_recipe(ThemeId::BauhausPrimary),
                     bauhaus);
  check_exact_recipe(calendar::theme_style_recipe(ThemeId::NordicQuiet),
                     nordic);
  check_exact_recipe(calendar::theme_style_recipe(ThemeId::SilverBlue),
                     balanced);
  check_exact_recipe(calendar::theme_style_recipe(ThemeId::Sage), balanced);
  check_exact_recipe(calendar::theme_style_recipe(ThemeId::Lavender), balanced);
  check_exact_recipe(calendar::theme_style_recipe(ThemeId::WarmSand), balanced);

  CHECK(modern.surface_grammar() == calendar::SurfaceGrammar::LayeredCards);
  CHECK(modern.navigation_grammar() ==
        calendar::NavigationGrammar::AccentUnderline);
  CHECK(modern.forecast_well_grammar() ==
        calendar::ForecastWellGrammar::CurrentOnly);
  CHECK(swiss.surface_grammar() == calendar::SurfaceGrammar::RuledGrid);
  CHECK(swiss.navigation_grammar() ==
        calendar::NavigationGrammar::RuledUnderline);
  CHECK(swiss.forecast_well_grammar() == calendar::ForecastWellGrammar::None);
  CHECK(bauhaus.surface_grammar() == calendar::SurfaceGrammar::StrongBlocks);
  CHECK(bauhaus.navigation_grammar() ==
        calendar::NavigationGrammar::FilledTabAndUnderline);
  CHECK(bauhaus.forecast_well_grammar() ==
        calendar::ForecastWellGrammar::GeometricCurrent);
  CHECK(nordic.surface_grammar() == calendar::SurfaceGrammar::OpenSurfaces);
  CHECK(nordic.navigation_grammar() ==
        calendar::NavigationGrammar::SoftSelectedTab);
  CHECK(nordic.forecast_well_grammar() == calendar::ForecastWellGrammar::Open);
  CHECK(balanced.surface_grammar() == calendar::SurfaceGrammar::LayeredCards);
  CHECK(balanced.navigation_grammar() ==
        calendar::NavigationGrammar::SoftSelectedTab);
  CHECK(balanced.forecast_well_grammar() ==
        calendar::ForecastWellGrammar::CurrentOnly);
}

void check_exact_palette(const calendar::ThemePalette &actual,
                         const calendar::ThemePalette &expected) {
  CHECK(actual.canvas == expected.canvas);
  CHECK(actual.surface == expected.surface);
  CHECK(actual.surface_subtle == expected.surface_subtle);
  CHECK(actual.surface_muted == expected.surface_muted);
  CHECK(actual.input_inset == expected.input_inset);
  CHECK(actual.header == expected.header);
  CHECK(actual.on_header == expected.on_header);
  CHECK(actual.text_primary == expected.text_primary);
  CHECK(actual.text_secondary == expected.text_secondary);
  CHECK(actual.text_muted == expected.text_muted);
  CHECK(actual.border == expected.border);
  CHECK(actual.divider == expected.divider);
  CHECK(actual.action == expected.action);
  CHECK(actual.on_action == expected.on_action);
  CHECK(actual.action_pressed == expected.action_pressed);
  CHECK(actual.selection == expected.selection);
  CHECK(actual.on_selection == expected.on_selection);
  CHECK(actual.action_soft == expected.action_soft);
  CHECK(actual.focus == expected.focus);
  CHECK(actual.today_fill == expected.today_fill);
  CHECK(actual.today_text == expected.today_text);
  CHECK(actual.today_ring == expected.today_ring);
  CHECK(actual.status_normal == expected.status_normal);
  CHECK(actual.status_attention == expected.status_attention);
  CHECK(actual.status_error == expected.status_error);
}

void test_approved_balanced_palette_tokens() {
  using calendar::ThemeId;
  constexpr calendar::ThemePalette common_light{
      0xF3F4F6, 0xFFFFFF, 0xF8F9FB, 0xE9ECF0, 0xEDF0F4, 0xF3F4F6, 0x1D2430,
      0x1D2430, 0x46505F, 0x5B6472, 0x7C8592, 0xCDD2DB, 0,        0xFFFFFF,
      0,        0,        0xFFFFFF, 0,        0,        0xFFF0CC, 0x694600,
      0x906200, 0x356645, 0x7A4F00, 0xB02632};
  constexpr calendar::ThemePalette common_dark{
      0x111318, 0x1B1E25, 0x232731, 0x16191F, 0x151820, 0x111318, 0xF1F3F8,
      0xF1F3F8, 0xD0D5E0, 0xADB5C3, 0x7D8799, 0x3A4250, 0,        0x10141D,
      0,        0,        0xFFFFFF, 0,        0,        0x463819, 0xFFDC88,
      0xDBAE52, 0x9DD8B1, 0xFFCF85, 0xFFB4BD};

  const auto with_accent =
      [](calendar::ThemePalette palette, std::uint32_t action,
         std::uint32_t action_pressed, std::uint32_t selection,
         std::uint32_t action_soft, std::uint32_t focus) {
        palette.action = action;
        palette.action_pressed = action_pressed;
        palette.selection = selection;
        palette.action_soft = action_soft;
        palette.focus = focus;
        return palette;
      };

  const std::array<calendar::ThemePalette, 8> expected{
      with_accent(common_light, 0x225FC6, 0x184992, 0x225FC6, 0xE7EEFB,
                  0x225FC6),
      with_accent(common_dark, 0x9EC1FF, 0x6F9FE7, 0x305585, 0x293951,
                  0x9EC1FF),
      with_accent(common_light, 0x356645, 0x244B30, 0x356645, 0xE5F0E8,
                  0x356645),
      with_accent(common_dark, 0x97D4AC, 0x69B686, 0x2A6040, 0x263C2E,
                  0x97D4AC),
      with_accent(common_light, 0x7043A4, 0x542E81, 0x7043A4, 0xEFE8F8,
                  0x7043A4),
      with_accent(common_dark, 0xD1AFF5, 0xB78BDD, 0x66458C, 0x3C304C,
                  0xD1AFF5),
      with_accent(common_light, 0x84551E, 0x633D12, 0x84551E, 0xF6EBDC,
                  0x84551E),
      with_accent(common_dark, 0xE7BE82, 0xC59959, 0x65481F, 0x403526,
                  0xE7BE82),
  };
  constexpr std::array<ThemeId, 4> themes{ThemeId::SilverBlue, ThemeId::Sage,
                                          ThemeId::Lavender, ThemeId::WarmSand};

  for (std::size_t index = 0; index < themes.size(); ++index) {
    check_exact_palette(calendar::palette_for(themes[index], false),
                        expected[index * 2U]);
    check_exact_palette(calendar::palette_for(themes[index], true),
                        expected[index * 2U + 1U]);
  }
}

void test_theme_identity_tokens() {
  using calendar::ThemeId;
  const calendar::ThemePalette &swiss_light =
      calendar::palette_for(ThemeId::SwissGrid, false);
  const calendar::ThemePalette &swiss_dark =
      calendar::palette_for(ThemeId::SwissGrid, true);
  CHECK(swiss_light.header == 0x111111U && swiss_light.action == 0xB72025U &&
        swiss_light.selection == 0x111111U &&
        swiss_light.today_fill == 0xFFE6A3U);
  CHECK(swiss_dark.header == 0x000000U && swiss_dark.action == 0xFF5B5FU &&
        swiss_dark.selection == 0x333333U &&
        swiss_dark.today_text == 0xFFD97AU);

  const calendar::ThemePalette &bauhaus_light =
      calendar::palette_for(ThemeId::BauhausPrimary, false);
  const calendar::ThemePalette &bauhaus_dark =
      calendar::palette_for(ThemeId::BauhausPrimary, true);
  CHECK(bauhaus_light.header == 0x151515U &&
        bauhaus_light.action == 0x0047ABU &&
        bauhaus_light.selection == 0xC62828U &&
        bauhaus_light.today_fill == 0xF5C400U);
  CHECK(bauhaus_dark.action == 0x66A3FFU &&
        bauhaus_dark.selection == 0xA93633U &&
        bauhaus_dark.today_text == 0xFFE072U);

  const calendar::ThemePalette &nordic_light =
      calendar::palette_for(ThemeId::NordicQuiet, false);
  const calendar::ThemePalette &nordic_dark =
      calendar::palette_for(ThemeId::NordicQuiet, true);
  CHECK(nordic_light.header == 0x23332DU && nordic_light.action == 0x286B57U &&
        nordic_light.today_fill == 0xF6E8C8U);
  CHECK(nordic_dark.header == 0x09100DU && nordic_dark.action == 0x69C6A8U &&
        nordic_dark.selection == 0x285B4AU);
}

void test_accessible_contrast() {
  using calendar::ThemeId;
  constexpr std::array<ThemeId, calendar::kThemeCount> themes{
      ThemeId::ModernQuiet, ThemeId::SwissGrid,  ThemeId::BauhausPrimary,
      ThemeId::NordicQuiet, ThemeId::SilverBlue, ThemeId::Sage,
      ThemeId::Lavender,    ThemeId::WarmSand};
  for (ThemeId theme : themes) {
    for (bool dark : {false, true}) {
      check_contrast_contract(theme, dark, false);
      check_contrast_contract(theme, dark, true);
    }
  }
}

} // namespace

int main() {
  test_selection_and_exact_tokens();
  test_stable_ids_names_and_metrics();
  test_exact_style_recipes();
  test_approved_balanced_palette_tokens();
  test_theme_identity_tokens();
  test_accessible_contrast();
  if (failures != 0) {
    std::cerr << failures << " theme palette test(s) failed\n";
    return 1;
  }
  std::cout << "theme palette tests passed\n";
  return 0;
}
