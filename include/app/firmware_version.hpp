#pragma once

namespace calendar {

#if defined(APP_RGB_STAGED_STARTUP) && defined(APP_LVGL_DRAW_BUFFER_ROWS)
inline constexpr char kFirmwareVersion[] = "0.6.36-gpio17-ldr-inverted-buffer16";
#elif defined(APP_RGB_STAGED_STARTUP)
inline constexpr char kFirmwareVersion[] = "0.6.36-gpio17-ldr-inverted";
#elif defined(APP_RGB_DIAGNOSTICS)
inline constexpr char kFirmwareVersion[] = "0.6.19-rgb-diagnostics";
#elif defined(APP_OWN_ST7262_PANEL)
inline constexpr char kFirmwareVersion[] = "0.6.17-weather-image-icons";
#else
inline constexpr char kFirmwareVersion[] = "0.6.11-rgb-single-flush-owner";
#endif

}  // namespace calendar
