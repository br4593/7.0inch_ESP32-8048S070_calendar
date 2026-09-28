/**
 * LVGL 9.2.2 configuration for the ESP32-8048S070C target.
 *
 * Keep the clock custom setting disabled: the Arduino loop advances LVGL with
 * lv_tick_inc(), so there is exactly one tick source.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

#define LV_USE_OS LV_OS_NONE

#define LV_TICK_CUSTOM 0

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_32 1

// Palette state pairs apply immediately, including button-label inheritance.
// Interpolating the default theme's pressed colors can cross low contrast.
#define LV_THEME_DEFAULT_TRANSITION_TIME 0

#define LV_USE_BIDI 1
#define LV_BIDI_BASE_DIR_DEF LV_BASE_DIR_AUTO

#endif  // LV_CONF_H
