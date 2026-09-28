#include "app/theme_palette.hpp"

namespace calendar {
namespace {

struct ThemeRecord {
  const char *name;
  ThemeStyleRecipe recipe;
  ThemePalette light;
  ThemePalette dark;
};

constexpr std::uint8_t grammar_bits(SurfaceGrammar surface,
                                    NavigationGrammar navigation,
                                    ForecastWellGrammar forecast_wells) {
  return static_cast<std::uint8_t>(surface) |
         static_cast<std::uint8_t>(static_cast<std::uint8_t>(navigation)
                                   << 2U) |
         static_cast<std::uint8_t>(static_cast<std::uint8_t>(forecast_wells)
                                   << 4U);
}

constexpr ThemePalette kModernQuietLight{
    0xF4F6F7, 0xFFFFFF, 0xF7F9FA, 0xE9EFF1, 0xEEF2F3, 0x142A33, 0xFFFFFF,
    0x122B34, 0x38535D, 0x556D75, 0x7A9098, 0xC4D0D4, 0x0D7383, 0xFFFFFF,
    0x0E5A66, 0x147A8A, 0xFFFFFF, 0xDCECEF, 0x0A7184, 0xFFF0D5, 0x704400,
    0xA56600, 0xBFE7EC, 0xFFD486, 0xFFB7AE,
};

constexpr ThemePalette kModernQuietDark{
    0x0E161B, 0x141E24, 0x19262D, 0x111B21, 0x0F181D, 0x080F13, 0xFFFFFF,
    0xF1F6F8, 0xD3E0E4, 0xA4B7BE, 0x5A717A, 0x2A3B43, 0x5BC2D5, 0x071419,
    0x3196AA, 0x285C69, 0xFFFFFF, 0x213B43, 0x73D3E3, 0x493A20, 0xFFD58C,
    0xDEA34E, 0xBDEAF0, 0xFFD58C, 0xFFB7AE,
};

constexpr ThemePalette kSwissGridLight{
    0xF5F5F2, 0xFFFFFF, 0xF0F0EC, 0xE5E5E0, 0xF3F3EF, 0x111111, 0xFFFFFF,
    0x111111, 0x383838, 0x5C5C5C, 0x777770, 0xC8C8C0, 0xB72025, 0xFFFFFF,
    0x87171B, 0x111111, 0xFFFFFF, 0xF6DDDE, 0xB72025, 0xFFE6A3, 0x4C3500,
    0x9A6200, 0xCDEAE0, 0xFFE19C, 0xF4B8B6,
};

constexpr ThemePalette kSwissGridDark{
    0x101010, 0x181818, 0x202020, 0x151515, 0x131313, 0x000000, 0xFFFFFF,
    0xF5F5F2, 0xD6D6D0, 0xA8A8A0, 0x707068, 0x333330, 0xFF5B5F, 0x1A0001,
    0xE54C52, 0x333333, 0xFFFFFF, 0x3A2324, 0xFF7478, 0x3E3210, 0xFFD97A,
    0xE3A900, 0xCDEAE0, 0xFFE19C, 0xF4B8B6,
};

constexpr ThemePalette kBauhausPrimaryLight{
    0xF4EEDC, 0xFFFAF0, 0xF6F0E3, 0xE5DEC9, 0xF0E8D5, 0x151515, 0xFFFFFF,
    0x171717, 0x403D37, 0x625E55, 0x7E786D, 0xCCC4B3, 0x0047AB, 0xFFFFFF,
    0x00347E, 0xC62828, 0xFFFFFF, 0xDDE7F5, 0x0047AB, 0xF5C400, 0x2A2100,
    0x7A5F00, 0xC7E2F2, 0xF5C400, 0xF5B8B0,
};

constexpr ThemePalette kBauhausPrimaryDark{
    0x12110E, 0x1D1B16, 0x25221B, 0x171510, 0x16140F, 0x050505, 0xFFFFFF,
    0xF7F3E8, 0xD9D3C5, 0xAAA497, 0x746E62, 0x38342B, 0x66A3FF, 0x07142A,
    0x548FDC, 0xA93633, 0xFFFFFF, 0x253248, 0x85B8FF, 0x4A3C00, 0xFFE072,
    0xD8AD00, 0xC7E2F2, 0xF5C400, 0xF5B8B0,
};

constexpr ThemePalette kNordicQuietLight{
    0xF2F4F1, 0xFEFFFC, 0xF6F8F4, 0xE5EAE4, 0xEDF1EC, 0x23332D, 0xFFFFFF,
    0x17231F, 0x3D514A, 0x586C64, 0x778A82, 0xCAD4CE, 0x286B57, 0xFFFFFF,
    0x1C4F40, 0x286B57, 0xFFFFFF, 0xDCEBE5, 0x1D6551, 0xF6E8C8, 0x5A3D00,
    0x9B6E1B, 0xD0E8DE, 0xF4D99A, 0xF1B9AE,
};

constexpr ThemePalette kNordicQuietDark{
    0x101713, 0x16211C, 0x1D2A24, 0x111B16, 0x111A16, 0x09100D, 0xFFFFFF,
    0xF3F7F4, 0xD5E1DB, 0x9FB3AA, 0x60776D, 0x2A3B33, 0x69C6A8, 0x08140F,
    0x47A589, 0x285B4A, 0xFFFFFF, 0x213A31, 0x7AD7B8, 0x47391F, 0xFFD58C,
    0xD8A34F, 0xD0E8DE, 0xF4D99A, 0xF1B9AE,
};

constexpr ThemePalette kSilverBlueLight{
    0xF3F4F6, 0xFFFFFF, 0xF8F9FB, 0xE9ECF0, 0xEDF0F4, 0xF3F4F6, 0x1D2430,
    0x1D2430, 0x46505F, 0x5B6472, 0x7C8592, 0xCDD2DB, 0x225FC6, 0xFFFFFF,
    0x184992, 0x225FC6, 0xFFFFFF, 0xE7EEFB, 0x225FC6, 0xFFF0CC, 0x694600,
    0x906200, 0x356645, 0x7A4F00, 0xB02632,
};

constexpr ThemePalette kSilverBlueDark{
    0x111318, 0x1B1E25, 0x232731, 0x16191F, 0x151820, 0x111318, 0xF1F3F8,
    0xF1F3F8, 0xD0D5E0, 0xADB5C3, 0x7D8799, 0x3A4250, 0x9EC1FF, 0x10141D,
    0x6F9FE7, 0x305585, 0xFFFFFF, 0x293951, 0x9EC1FF, 0x463819, 0xFFDC88,
    0xDBAE52, 0x9DD8B1, 0xFFCF85, 0xFFB4BD,
};

constexpr ThemePalette kSageLight{
    0xF3F4F6, 0xFFFFFF, 0xF8F9FB, 0xE9ECF0, 0xEDF0F4, 0xF3F4F6, 0x1D2430,
    0x1D2430, 0x46505F, 0x5B6472, 0x7C8592, 0xCDD2DB, 0x356645, 0xFFFFFF,
    0x244B30, 0x356645, 0xFFFFFF, 0xE5F0E8, 0x356645, 0xFFF0CC, 0x694600,
    0x906200, 0x356645, 0x7A4F00, 0xB02632,
};

constexpr ThemePalette kSageDark{
    0x111318, 0x1B1E25, 0x232731, 0x16191F, 0x151820, 0x111318, 0xF1F3F8,
    0xF1F3F8, 0xD0D5E0, 0xADB5C3, 0x7D8799, 0x3A4250, 0x97D4AC, 0x10141D,
    0x69B686, 0x2A6040, 0xFFFFFF, 0x263C2E, 0x97D4AC, 0x463819, 0xFFDC88,
    0xDBAE52, 0x9DD8B1, 0xFFCF85, 0xFFB4BD,
};

constexpr ThemePalette kLavenderLight{
    0xF3F4F6, 0xFFFFFF, 0xF8F9FB, 0xE9ECF0, 0xEDF0F4, 0xF3F4F6, 0x1D2430,
    0x1D2430, 0x46505F, 0x5B6472, 0x7C8592, 0xCDD2DB, 0x7043A4, 0xFFFFFF,
    0x542E81, 0x7043A4, 0xFFFFFF, 0xEFE8F8, 0x7043A4, 0xFFF0CC, 0x694600,
    0x906200, 0x356645, 0x7A4F00, 0xB02632,
};

constexpr ThemePalette kLavenderDark{
    0x111318, 0x1B1E25, 0x232731, 0x16191F, 0x151820, 0x111318, 0xF1F3F8,
    0xF1F3F8, 0xD0D5E0, 0xADB5C3, 0x7D8799, 0x3A4250, 0xD1AFF5, 0x10141D,
    0xB78BDD, 0x66458C, 0xFFFFFF, 0x3C304C, 0xD1AFF5, 0x463819, 0xFFDC88,
    0xDBAE52, 0x9DD8B1, 0xFFCF85, 0xFFB4BD,
};

constexpr ThemePalette kWarmSandLight{
    0xF3F4F6, 0xFFFFFF, 0xF8F9FB, 0xE9ECF0, 0xEDF0F4, 0xF3F4F6, 0x1D2430,
    0x1D2430, 0x46505F, 0x5B6472, 0x7C8592, 0xCDD2DB, 0x84551E, 0xFFFFFF,
    0x633D12, 0x84551E, 0xFFFFFF, 0xF6EBDC, 0x84551E, 0xFFF0CC, 0x694600,
    0x906200, 0x356645, 0x7A4F00, 0xB02632,
};

constexpr ThemePalette kWarmSandDark{
    0x111318, 0x1B1E25, 0x232731, 0x16191F, 0x151820, 0x111318, 0xF1F3F8,
    0xF1F3F8, 0xD0D5E0, 0xADB5C3, 0x7D8799, 0x3A4250, 0xE7BE82, 0x10141D,
    0xC59959, 0x65481F, 0xFFFFFF, 0x403526, 0xE7BE82, 0x463819, 0xFFDC88,
    0xDBAE52, 0x9DD8B1, 0xFFCF85, 0xFFB4BD,
};

constexpr ThemeRecord kThemes[]{
    {"Modern Quiet",
     {8, 8, 8, 1, 1, 4, 12, 6, 64, 3, 12, 6, 0,
      grammar_bits(SurfaceGrammar::LayeredCards,
                   NavigationGrammar::AccentUnderline,
                   ForecastWellGrammar::CurrentOnly)},
     kModernQuietLight,
     kModernQuietDark},
    {"Swiss Grid",
     {2, 2, 2, 1, 1, 3, 8, 0, 80, 3, 10, 0, 1,
      grammar_bits(SurfaceGrammar::RuledGrid, NavigationGrammar::RuledUnderline,
                   ForecastWellGrammar::None)},
     kSwissGridLight,
     kSwissGridDark},
    {"Bauhaus Primary",
     {0, 0, 0, 2, 2, 3, 12, 6, 64, 4, 14, 0, 1,
      grammar_bits(SurfaceGrammar::StrongBlocks,
                   NavigationGrammar::FilledTabAndUnderline,
                   ForecastWellGrammar::GeometricCurrent)},
     kBauhausPrimaryLight,
     kBauhausPrimaryDark},
    {"Nordic Quiet",
     {8, 8, 8, 0, 1, 3, 16, 8, 48, 4, 10, 5, 0,
      grammar_bits(SurfaceGrammar::OpenSurfaces,
                   NavigationGrammar::SoftSelectedTab,
                   ForecastWellGrammar::Open)},
     kNordicQuietLight,
     kNordicQuietDark},
    {"Silver Blue",
     {12, 8, 8, 1, 1, 3, 16, 6, 48, 3, 10, 5, 0,
      grammar_bits(SurfaceGrammar::LayeredCards,
                   NavigationGrammar::SoftSelectedTab,
                   ForecastWellGrammar::CurrentOnly)},
     kSilverBlueLight,
     kSilverBlueDark},
    {"Sage",
     {12, 8, 8, 1, 1, 3, 16, 6, 48, 3, 10, 5, 0,
      grammar_bits(SurfaceGrammar::LayeredCards,
                   NavigationGrammar::SoftSelectedTab,
                   ForecastWellGrammar::CurrentOnly)},
     kSageLight,
     kSageDark},
    {"Lavender",
     {12, 8, 8, 1, 1, 3, 16, 6, 48, 3, 10, 5, 0,
      grammar_bits(SurfaceGrammar::LayeredCards,
                   NavigationGrammar::SoftSelectedTab,
                   ForecastWellGrammar::CurrentOnly)},
     kLavenderLight,
     kLavenderDark},
    {"Warm Sand",
     {12, 8, 8, 1, 1, 3, 16, 6, 48, 3, 10, 5, 0,
      grammar_bits(SurfaceGrammar::LayeredCards,
                   NavigationGrammar::SoftSelectedTab,
                   ForecastWellGrammar::CurrentOnly)},
     kWarmSandLight,
     kWarmSandDark},
};

static_assert(sizeof(kThemes) / sizeof(kThemes[0]) == kThemeCount,
              "ThemeId and theme table must stay in sync");

std::uint8_t theme_index(ThemeId theme) {
  return is_valid_theme_id(theme) ? static_cast<std::uint8_t>(theme)
                                  : static_cast<std::uint8_t>(kDefaultThemeId);
}

} // namespace

ThemeId theme_id_from_raw(std::uint8_t raw_theme_id) {
  const ThemeId theme = static_cast<ThemeId>(raw_theme_id);
  return is_valid_theme_id(theme) ? theme : kDefaultThemeId;
}

const char *theme_name(ThemeId theme) {
  return kThemes[theme_index(theme)].name;
}

const ThemeStyleRecipe &theme_style_recipe(ThemeId theme) {
  return kThemes[theme_index(theme)].recipe;
}

const ThemeMetrics &theme_metrics(ThemeId theme) {
  return theme_style_recipe(theme);
}

const ThemePalette &palette_for(ThemeId theme, bool dark_theme) {
  const ThemeRecord &record = kThemes[theme_index(theme)];
  return dark_theme ? record.dark : record.light;
}

const ThemePalette &light_palette() {
  return palette_for(ThemeId::ModernQuiet, false);
}

const ThemePalette &dark_palette() {
  return palette_for(ThemeId::ModernQuiet, true);
}

const ThemePalette &palette_for(bool dark_theme) {
  return palette_for(ThemeId::ModernQuiet, dark_theme);
}

} // namespace calendar
