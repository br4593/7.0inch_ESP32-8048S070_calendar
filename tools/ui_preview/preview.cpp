#include "Arduino.h"
#include "app/ui_controller.hpp"
#include "fixtures.hpp"
#include <lvgl.h>
extern "C" {
#include "ui_generated/screens.h"
#include "ui_generated/ui.h"
}
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr int kWidth = 800, kHeight = 480;
constexpr double kTextContrast = 4.5, kIconContrast = 3.0;
std::array<std::uint16_t, kWidth * kHeight> pixels{};
std::array<std::uint16_t, kWidth * 40> draw_buffer{};
std::array<void *, 4> supplementary_pools{};
std::ofstream checks;
std::ofstream performance;
unsigned failures = 0;
unsigned static_button_samples = 0;
int max_static_center2_x = 0;
int max_static_center2_y = 0;
int max_static_pressed_center_change = 0;

struct RenderStats {
  std::uint64_t flushes = 0;
  std::uint64_t refreshes = 0;
  std::uint64_t pixels = 0;
};
RenderStats render_stats;
void pump(int rounds);

void flush(lv_display_t *display, const lv_area_t *area, std::uint8_t *data) {
  ++render_stats.flushes;
  render_stats.pixels += static_cast<std::uint64_t>(area->x2 - area->x1 + 1) *
                         static_cast<std::uint64_t>(area->y2 - area->y1 + 1);
  if (lv_display_flush_is_last(display))
    ++render_stats.refreshes;
  auto *source = reinterpret_cast<std::uint16_t *>(data);
  for (int y = area->y1; y <= area->y2; ++y)
    for (int x = area->x1; x <= area->x2; ++x)
      pixels[y * kWidth + x] = *source++;
  lv_display_flush_ready(display);
}
void reset_render_stats() { render_stats = {}; }
RenderStats measure_pump(int rounds) {
  lv_refr_now(nullptr);
  reset_render_stats();
  pump(rounds);
  return render_stats;
}
std::string render_stats_text(const RenderStats &stats) {
  std::ostringstream out;
  out << "flushes=" << stats.flushes << " refreshes=" << stats.refreshes
      << " pixels=" << stats.pixels;
  return out.str();
}
void pump(int rounds = 12) {
  for (int i = 0; i < rounds; ++i) {
    preview_millis += 20;
    lv_tick_inc(20);
    calendar::tick_calendar_ui();
    ui_tick();
    lv_timer_handler();
  }
}
void capture(const std::string &path) {
  lv_obj_update_layout(lv_screen_active());
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(nullptr);
  std::ofstream output(path, std::ios::binary);
  output << "P6\n800 480\n255\n";
  for (auto color : pixels) {
    const auto r = (color >> 11) & 31, g = (color >> 5) & 63, b = color & 31;
    const char rgb[]{static_cast<char>((r << 3) | (r >> 2)),
                     static_cast<char>((g << 2) | (g >> 4)),
                     static_cast<char>((b << 3) | (b >> 2))};
    output.write(rgb, 3);
  }
}
void check(bool condition, std::string_view name,
           std::string_view detail = {}) {
  checks << (condition ? "PASS " : "FAIL ") << name;
  if (!detail.empty())
    checks << ": " << detail;
  checks << '\n';
  if (!condition)
    ++failures;
}

void record_performance(std::string_view name, const RenderStats &stats) {
  performance << name << ' ' << render_stats_text(stats) << '\n';
}

bool settings_values_centered() {
  const std::array<std::pair<lv_obj_t *, lv_obj_t *>, 3> pairs{
      {{objects.settings_wifi_caption_label,
        objects.settings_wifi_status_label},
       {objects.settings_time_caption_label,
        objects.settings_time_status_label},
       {objects.settings_feed_caption_label,
        objects.settings_feed_status_label}}};
  for (const auto &pair : pairs) {
    lv_area_t caption{}, value{};
    lv_obj_update_layout(pair.second);
    lv_obj_get_coords(pair.first, &caption);
    lv_obj_get_coords(pair.second, &value);
    const int height = lv_obj_get_height(pair.second);
    if (height > 48 ||
        std::abs((value.y1 + value.y2) - (caption.y1 + caption.y2)) > 1)
      return false;
  }
  return true;
}

unsigned swatch_style_change_count = 0;
void count_swatch_style_changes(lv_event_t *) { ++swatch_style_change_count; }

void verify_redraw_performance() {
  constexpr int kIdleRounds = 100; // 2 seconds at the preview's 20 ms tick.

  calendar::show_calendar_main();
  pump(40);
  const RenderStats today_idle = measure_pump(kIdleRounds);
  record_performance("today_idle_2s", today_idle);

  calendar::show_calendar_settings();
  pump(40);
  const RenderStats settings_idle = measure_pump(kIdleRounds);
  record_performance("settings_idle_2s", settings_idle);
  check(settings_idle.pixels == 0, "idle Settings has no dirty pixels",
        render_stats_text(settings_idle));

  const board::ConnectivityStatus connected = preview::connection;
  preview::connection.setup_ap_active = true;
  preview::connection.wifi_connected = false;
  std::strcpy(preview::connection.setup_ssid, "Calendar Setup");
  std::strcpy(preview::connection.setup_password, "48273190");
  std::strcpy(preview::connection.setup_address, "192.168.4.1");
  std::strcpy(preview::connection.wifi_detail,
              "Waiting for a phone to finish setup");
  const RenderStats settings_two_line = measure_pump(20);
  record_performance("settings_change_two_line", settings_two_line);
  check(settings_two_line.pixels > 0 &&
            std::strchr(lv_label_get_text(objects.settings_wifi_status_label),
                        '\n') != nullptr,
        "Settings two-line status change redraws once",
        render_stats_text(settings_two_line));
  check(settings_values_centered(), "Settings two-line values remain centered");
  const RenderStats settings_two_line_idle = measure_pump(kIdleRounds);
  record_performance("settings_two_line_idle_2s", settings_two_line_idle);
  check(settings_two_line_idle.pixels == 0,
        "unchanged two-line Settings has no dirty pixels",
        render_stats_text(settings_two_line_idle));

  preview::connection = connected;
  const RenderStats settings_one_line = measure_pump(20);
  record_performance("settings_change_one_line", settings_one_line);
  check(settings_one_line.pixels > 0, "Settings one-line status change redraws",
        render_stats_text(settings_one_line));
  check(settings_values_centered(), "Settings one-line values remain centered");

  const std::string connected_feed_text =
      lv_label_get_text(objects.settings_feed_status_label);
  preview::connection.fetch_state = board::CalendarFetchState::Failed;
  std::snprintf(preview::connection.summary,
                sizeof(preview::connection.summary),
                "%s", "Overflow sentinel WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW");
  std::snprintf(
      preview::connection.fetch_detail,
      sizeof(preview::connection.fetch_detail), "%s",
      "Overflow sentinel WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW WWW");
  const std::string overflow_source =
      std::to_string(preview::connection.configured_feed_count) +
      " private calendars saved - " + preview::connection.fetch_detail;
  const RenderStats settings_overflow = measure_pump(20);
  record_performance("settings_change_overflow", settings_overflow);
  const char *overflow_display =
      lv_label_get_text(objects.settings_feed_status_label);
  std::ostringstream overflow_detail;
  overflow_detail << render_stats_text(settings_overflow)
                  << " mode="
                  << static_cast<int>(lv_label_get_long_mode(
                         objects.settings_feed_status_label))
                  << " height="
                  << lv_obj_get_height(objects.settings_feed_status_label)
                  << " source_bytes=" << overflow_source.size()
                  << " display_bytes="
                  << (overflow_display ? std::strlen(overflow_display) : 0)
                  << " mutated="
                  << (overflow_display != nullptr &&
                      overflow_source != overflow_display);
  check(settings_overflow.pixels > 0 &&
            lv_label_get_long_mode(objects.settings_feed_status_label) ==
                LV_LABEL_LONG_DOT &&
            lv_obj_get_height(objects.settings_feed_status_label) == 48 &&
            overflow_display != nullptr &&
            overflow_source != overflow_display,
        "Settings overflow is ellipsized after full source is cached",
        overflow_detail.str());
  check(settings_values_centered(), "Settings overflow value remains centered");
  const RenderStats settings_overflow_idle = measure_pump(kIdleRounds);
  record_performance("settings_overflow_idle_2s", settings_overflow_idle);
  check(settings_overflow_idle.pixels == 0,
        "unchanged ellipsized Settings has no dirty pixels",
        render_stats_text(settings_overflow_idle));

  preview::connection = connected;
  const RenderStats settings_restored = measure_pump(20);
  record_performance("settings_restore_after_overflow", settings_restored);
  const char *restored_feed =
      lv_label_get_text(objects.settings_feed_status_label);
  check(settings_restored.pixels > 0 && restored_feed != nullptr &&
            connected_feed_text == restored_feed &&
            std::strstr(restored_feed, "Overflow sentinel") == nullptr &&
            lv_label_get_long_mode(objects.settings_feed_status_label) ==
                LV_LABEL_LONG_WRAP,
        "Settings value restores after overflow without stale text",
        render_stats_text(settings_restored));
  check(settings_values_centered(),
        "Settings restored value remains centered after overflow");

  preview::ota_status = {};
  preview::ota_status.state = board::OtaState::Disabled;
  std::strcpy(preview::ota_status.detail, "Firmware updates are disabled");
  calendar::show_calendar_firmware_update();
  pump(40);
  const RenderStats firmware_idle = measure_pump(kIdleRounds);
  record_performance("firmware_disabled_idle_2s", firmware_idle);
  check(firmware_idle.pixels == 0, "idle Firmware has no dirty pixels",
        render_stats_text(firmware_idle));

  preview::ota_status.state = board::OtaState::Armed;
  std::strcpy(preview::ota_status.upload_url, "http://192.168.1.42/update");
  std::strcpy(preview::ota_status.one_time_code, "482731");
  preview::ota_status.seconds_remaining = 300;
  const RenderStats armed = measure_pump(20);
  record_performance("firmware_change_armed", armed);
  check(armed.pixels > 0 &&
            std::strstr(lv_label_get_text(objects.firmware_instructions_label),
                        "300 seconds") != nullptr,
        "Firmware Armed countdown renders", render_stats_text(armed));
  preview::ota_status.seconds_remaining = 299;
  const RenderStats countdown = measure_pump(20);
  record_performance("firmware_countdown_tick", countdown);
  check(countdown.pixels > 0 &&
            std::strstr(lv_label_get_text(objects.firmware_instructions_label),
                        "299 seconds") != nullptr,
        "Firmware countdown decrement renders", render_stats_text(countdown));

  preview::ota_status = {};
  preview::ota_status.state = board::OtaState::Receiving;
  preview::ota_status.received_bytes = 256 * 1024;
  preview::ota_status.total_bytes = 1024 * 1024;
  const RenderStats receiving = measure_pump(20);
  record_performance("firmware_change_receiving", receiving);
  check(receiving.pixels > 0 &&
            lv_bar_get_value(objects.firmware_progress_bar) == 25 &&
            std::strstr(lv_label_get_text(objects.firmware_instructions_label),
                        "256 KiB") != nullptr,
        "Firmware Receiving bytes and progress render",
        render_stats_text(receiving));
  preview::ota_status.received_bytes = 512 * 1024;
  const RenderStats receiving_progress = measure_pump(20);
  record_performance("firmware_receiving_progress", receiving_progress);
  check(receiving_progress.pixels > 0 &&
            lv_bar_get_value(objects.firmware_progress_bar) == 50,
        "Firmware Receiving progress update renders",
        render_stats_text(receiving_progress));

  preview::ota_status = {};
  preview::ota_status.state = board::OtaState::ReadyToReboot;
  std::strcpy(preview::ota_status.detail, "Verified image is ready to install");
  const RenderStats ready = measure_pump(20);
  record_performance("firmware_change_ready", ready);
  check(ready.pixels > 0 &&
            !lv_obj_has_state(objects.firmware_reboot_button,
                              LV_STATE_DISABLED),
        "Firmware Ready-to-reboot primary action renders",
        render_stats_text(ready));
  const RenderStats ready_idle = measure_pump(kIdleRounds);
  record_performance("firmware_ready_idle_2s", ready_idle);
  check(ready_idle.pixels == 0,
        "unchanged Ready Firmware has no dirty pixels",
        render_stats_text(ready_idle));

  calendar::show_calendar_brightness_settings();
  pump(20);
  for (auto *swatch : {objects.theme_preview_canvas,
                       objects.theme_preview_surface,
                       objects.theme_preview_accent})
    lv_obj_add_event_cb(swatch, count_swatch_style_changes,
                        LV_EVENT_STYLE_CHANGED, nullptr);
  swatch_style_change_count = 0;
  preview::reset_runtime_counters();
  const unsigned selected_theme =
      lv_dropdown_get_selected(objects.theme_style_dropdown);
  const bool selected_dark =
      lv_obj_has_state(objects.dark_theme_switch, LV_STATE_CHECKED);
  int next_brightness = lv_slider_get_value(objects.brightness_slider) + 7;
  if (next_brightness > 100)
    next_brightness -= 14;
  lv_slider_set_value(objects.brightness_slider, next_brightness, LV_ANIM_OFF);
  lv_refr_now(nullptr);
  reset_render_stats();
  calendar::preview_calendar_brightness();
  pump(4);
  const RenderStats brightness_drag = render_stats;
  record_performance("brightness_single_drag_step", brightness_drag);
  check(preview::backlight_update_count == 1 &&
            preview::last_backlight_percent == next_brightness,
        "brightness drag updates backlight once");
  check(preview::appearance_save_count == 0,
        "brightness drag does not save preferences");
  check(swatch_style_change_count == 0 &&
            lv_dropdown_get_selected(objects.theme_style_dropdown) ==
                selected_theme &&
            lv_obj_has_state(objects.dark_theme_switch, LV_STATE_CHECKED) ==
                selected_dark,
        "brightness drag leaves theme controls and swatches untouched",
        "swatch_style_changes=" +
            std::to_string(swatch_style_change_count));
  std::ostringstream brightness_text;
  brightness_text << next_brightness << '%';
  check(std::strcmp(lv_label_get_text(objects.brightness_value_label),
                    brightness_text.str().c_str()) == 0,
        "brightness drag updates percentage label");
  calendar::commit_calendar_brightness();
  pump(20);
  check(preview::appearance_save_count == 1,
        "brightness release saves preferences once",
        std::to_string(preview::appearance_save_count));
}
unsigned count_objects(lv_obj_t *object) {
  unsigned n = 1;
  for (unsigned i = 0; i < lv_obj_get_child_count(object); ++i)
    n += count_objects(lv_obj_get_child(object, static_cast<int32_t>(i)));
  return n;
}
unsigned total_objects() {
  unsigned n = 0;
  for (auto *screen :
       {objects.main_screen, objects.agenda_screen, objects.month_screen,
        objects.weather_screen, objects.settings_screen,
        objects.event_details_screen, objects.brightness_settings_screen,
        objects.firmware_update_screen})
    n += count_objects(screen);
  return n;
}
void click(lv_obj_t *object) {
  lv_obj_send_event(object, LV_EVENT_CLICKED, nullptr);
  pump();
}
lv_obj_t *visible_child(lv_obj_t *parent, unsigned ordinal = 0) {
  if (!parent)
    return nullptr;
  for (unsigned i = 0; i < lv_obj_get_child_count(parent); ++i) {
    auto *child = lv_obj_get_child(parent, static_cast<int32_t>(i));
    if (!lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN) && ordinal-- == 0)
      return child;
  }
  return nullptr;
}
lv_obj_t *find_descendant_by_type(lv_obj_t *parent,
                                  const lv_obj_class_t *type) {
  if (!parent)
    return nullptr;
  if (lv_obj_check_type(parent, type))
    return parent;
  for (unsigned i = 0; i < lv_obj_get_child_count(parent); ++i)
    if (auto *found = find_descendant_by_type(
            lv_obj_get_child(parent, static_cast<int32_t>(i)), type))
      return found;
  return nullptr;
}
bool descendant_label_contains(lv_obj_t *parent, const char *needle) {
  if (!parent)
    return false;
  if (lv_obj_check_type(parent, &lv_label_class)) {
    const char *value = lv_label_get_text(parent);
    if (value && std::strstr(value, needle))
      return true;
  }
  for (unsigned i = 0; i < lv_obj_get_child_count(parent); ++i)
    if (descendant_label_contains(
            lv_obj_get_child(parent, static_cast<int32_t>(i)), needle))
      return true;
  return false;
}
lv_obj_t *visible_row_containing(lv_obj_t *parent, const char *needle) {
  if (!parent)
    return nullptr;
  for (unsigned i = 0; i < lv_obj_get_child_count(parent); ++i) {
    auto *child = lv_obj_get_child(parent, static_cast<int32_t>(i));
    if (!lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN) &&
        descendant_label_contains(child, needle))
      return child;
  }
  return nullptr;
}

void collect_descendants(lv_obj_t *parent, const lv_obj_class_t *type,
                         std::vector<lv_obj_t *> &output,
                         bool include_parent = false) {
  if (!parent)
    return;
  if (include_parent && lv_obj_check_type(parent, type))
    output.push_back(parent);
  for (unsigned i = 0; i < lv_obj_get_child_count(parent); ++i) {
    auto *child = lv_obj_get_child(parent, static_cast<int32_t>(i));
    if (lv_obj_check_type(child, type))
      output.push_back(child);
    collect_descendants(child, type, output, false);
  }
}
lv_obj_t *find_label(lv_obj_t *parent, const char *text, bool exact = true) {
  if (!parent)
    return nullptr;
  if (lv_obj_check_type(parent, &lv_label_class)) {
    const char *value = lv_label_get_text(parent);
    if (value && ((exact && std::strcmp(value, text) == 0) ||
                  (!exact && std::strstr(value, text))))
      return parent;
  }
  for (unsigned i = 0; i < lv_obj_get_child_count(parent); ++i)
    if (auto *found = find_label(
            lv_obj_get_child(parent, static_cast<int32_t>(i)), text, exact))
      return found;
  return nullptr;
}
lv_obj_t *ancestor_of_type(lv_obj_t *object, const lv_obj_class_t *type) {
  for (auto *candidate = object; candidate;
       candidate = lv_obj_get_parent(candidate))
    if (lv_obj_check_type(candidate, type))
      return candidate;
  return nullptr;
}
lv_obj_t *direct_single_label(lv_obj_t *button) {
  if (!button)
    return nullptr;
  lv_obj_t *result = nullptr;
  unsigned labels = 0;
  for (unsigned i = 0; i < lv_obj_get_child_count(button); ++i) {
    auto *child = lv_obj_get_child(button, static_cast<int32_t>(i));
    if (lv_obj_check_type(child, &lv_label_class)) {
      result = child;
      ++labels;
    }
  }
  return labels == 1 ? result : nullptr;
}
lv_area_t area_of(lv_obj_t *object) {
  lv_area_t area{};
  if (object) {
    lv_obj_update_layout(object);
    lv_obj_get_coords(object, &area);
  }
  return area;
}
std::string area_text(lv_obj_t *object) {
  const auto a = area_of(object);
  std::ostringstream out;
  out << a.x1 << ',' << a.y1 << ' ' << (a.x2 - a.x1 + 1) << 'x'
      << (a.y2 - a.y1 + 1);
  return out.str();
}
int center2_x(lv_obj_t *object) {
  const auto a = area_of(object);
  return a.x1 + a.x2;
}
int center2_y(lv_obj_t *object) {
  const auto a = area_of(object);
  return a.y1 + a.y2;
}
int blank_gap_y(lv_obj_t *upper, lv_obj_t *lower) {
  const auto a = area_of(upper), b = area_of(lower);
  return b.y1 - a.y2 - 1;
}
lv_obj_t *visible_overlay(lv_obj_t *screen) {
  if (!screen)
    return nullptr;
  for (int i = static_cast<int>(lv_obj_get_child_count(screen)) - 1; i >= 0;
       --i) {
    auto *child = lv_obj_get_child(screen, i);
    if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN))
      continue;
    const auto a = area_of(child);
    if (a.x1 == 0 && a.y1 == 64 && a.x2 == 799 && a.y2 == 479)
      return child;
  }
  return nullptr;
}

lv_obj_t *label_at_local_position(lv_obj_t *parent, int x, int y) {
  if (!parent)
    return nullptr;
  const auto parent_area = area_of(parent);
  std::vector<lv_obj_t *> labels;
  collect_descendants(parent, &lv_label_class, labels);
  for (auto *label : labels) {
    const auto a = area_of(label);
    if (a.x1 - parent_area.x1 == x && a.y1 - parent_area.y1 == y)
      return label;
  }
  return nullptr;
}

std::uint32_t rgb888(lv_color_t color) {
  return lv_color_to_u32(color) & 0xFFFFFFU;
}
std::uint32_t rgb565_expanded(lv_color_t color) {
  const std::uint32_t source = rgb888(color);
  const std::uint8_t r5 = static_cast<std::uint8_t>((source >> 19U) & 0x1FU);
  const std::uint8_t g6 = static_cast<std::uint8_t>((source >> 10U) & 0x3FU);
  const std::uint8_t b5 = static_cast<std::uint8_t>((source >> 3U) & 0x1FU);
  const std::uint8_t r8 = static_cast<std::uint8_t>((r5 << 3U) | (r5 >> 2U));
  const std::uint8_t g8 = static_cast<std::uint8_t>((g6 << 2U) | (g6 >> 4U));
  const std::uint8_t b8 = static_cast<std::uint8_t>((b5 << 3U) | (b5 >> 2U));
  return (static_cast<std::uint32_t>(r8) << 16U) |
         (static_cast<std::uint32_t>(g8) << 8U) | b8;
}
double linear_channel(std::uint8_t value) {
  const double c = static_cast<double>(value) / 255.0;
  return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}
double luminance(std::uint32_t c) {
  return .2126 * linear_channel(static_cast<std::uint8_t>(c >> 16U)) +
         .7152 * linear_channel(static_cast<std::uint8_t>(c >> 8U)) +
         .0722 * linear_channel(static_cast<std::uint8_t>(c));
}
double contrast(std::uint32_t a, std::uint32_t b) {
  const double x = luminance(a), y = luminance(b);
  return (std::max(x, y) + .05) / (std::min(x, y) + .05);
}
std::string contrast_detail(std::uint32_t fg, std::uint32_t bg, double ratio) {
  std::ostringstream out;
  out << '#' << std::hex << std::setw(6) << std::setfill('0') << fg << " on #"
      << std::setw(6) << bg << std::dec << std::fixed << std::setprecision(2)
      << " = " << ratio << ":1";
  return out.str();
}

void select_appearance(unsigned theme, bool dark) {
  calendar::show_calendar_brightness_settings();
  pump(3);
  lv_dropdown_set_selected(objects.theme_style_dropdown, theme);
  calendar::change_calendar_theme_style();
  if (dark)
    lv_obj_add_state(objects.dark_theme_switch, LV_STATE_CHECKED);
  else
    lv_obj_remove_state(objects.dark_theme_switch, LV_STATE_CHECKED);
  calendar::toggle_calendar_dark_theme();
  pump(4);
}

double object_text_contrast(lv_obj_t *label, lv_obj_t *background) {
  const auto fg = rgb565_expanded(
      lv_obj_get_style_text_color_filtered(label, LV_PART_MAIN));
  const auto bg = rgb565_expanded(
      lv_obj_get_style_bg_color_filtered(background, LV_PART_MAIN));
  return contrast(fg, bg);
}

void verify_default_header_contrast_all_appearances(unsigned restore_theme,
                                                    bool restore_dark) {
  const auto saved_connection = preview::connection;
  preview::connection = {};
  unsigned combinations = 0;
  double overall_minimum = 100.0;
  std::string overall_minimum_case;
  for (unsigned theme = 0; theme < calendar::kThemeCount; ++theme) {
    for (bool dark : {false, true}) {
      select_appearance(theme, dark);
      struct HeaderSample {
        const char *name;
        void (*show)();
        lv_obj_t *header;
        std::array<lv_obj_t *, 3> labels;
      };
      const std::array<HeaderSample, 4> samples{
          {{"Today",
            calendar::show_calendar_main,
            objects.main_header,
            {objects.main_sync_state_label, objects.main_wifi_icon,
             objects.main_wifi_label}},
           {"Week",
            calendar::show_calendar_primary_page,
            objects.agenda_header,
            {objects.sync_state_label, objects.agenda_wifi_icon,
             objects.agenda_wifi_label}},
           {"Month",
            calendar::show_calendar_month_view,
            objects.month_header,
            {objects.month_sync_state_label, objects.month_wifi_icon,
             objects.month_wifi_label}},
           {"Forecast",
            calendar::show_calendar_forecast,
            objects.weather_header,
            {objects.forecast_sync_state_label, objects.forecast_wifi_icon,
             objects.forecast_wifi_label}}}};
      for (const auto &sample : samples) {
        sample.show();
        pump(3);
        double minimum = 100.0;
        std::uint32_t worst_fg = 0, worst_bg = 0;
        for (auto *label : sample.labels) {
          const auto fg = rgb565_expanded(
              lv_obj_get_style_text_color_filtered(label, LV_PART_MAIN));
          const auto bg = rgb565_expanded(
              lv_obj_get_style_bg_color_filtered(sample.header, LV_PART_MAIN));
          const double ratio = contrast(fg, bg);
          if (ratio < minimum) {
            minimum = ratio;
            worst_fg = fg;
            worst_bg = bg;
          }
        }
        std::ostringstream name;
        name << "default Neutral header contrast theme " << theme << ' '
             << (dark ? "dark " : "light ") << sample.name;
        check(minimum >= kTextContrast, name.str(),
              contrast_detail(worst_fg, worst_bg, minimum));
        if (minimum < overall_minimum) {
          overall_minimum = minimum;
          overall_minimum_case = name.str();
        }
      }
      ++combinations;
    }
  }
  check(combinations == calendar::kThemeCount * 2U,
        "default Neutral contrast covers all appearances",
        std::to_string(combinations));
  std::ostringstream minimum_detail;
  minimum_detail << std::fixed << std::setprecision(2) << overall_minimum
                 << ":1 at " << overall_minimum_case;
  check(overall_minimum >= kTextContrast,
        "default Neutral minimum final RGB565 contrast", minimum_detail.str());
  preview::connection = saved_connection;
  select_appearance(restore_theme, restore_dark);
  pump(20);
}

bool inspect_static_single_label_button(lv_obj_t *button, const char *name,
                                        unsigned theme, bool dark,
                                        unsigned &border_width_mask) {
  if (!button) {
    check(false, std::string("static button exists: ") + name);
    return false;
  }
  auto *label = direct_single_label(button);
  bool ok = label != nullptr;
  if (!label) {
    check(false, std::string("static single-label composition: ") + name);
    return false;
  }
  lv_obj_update_layout(button);
  const int dx = center2_x(label) - center2_x(button),
            dy = center2_y(label) - center2_y(button);
  const auto button_area = area_of(button), label_area = area_of(label);
  const bool contained =
      label_area.x1 >= button_area.x1 && label_area.x2 <= button_area.x2 &&
      label_area.y1 >= button_area.y1 && label_area.y2 <= button_area.y2;
  const bool content_width =
      lv_obj_get_style_width(label, LV_PART_MAIN) == LV_SIZE_CONTENT;
  const bool content_height =
      lv_obj_get_style_height(label, LV_PART_MAIN) == LV_SIZE_CONTENT;
  const bool font =
      lv_obj_get_style_text_font(label, LV_PART_MAIN) == &lv_font_montserrat_16;
  const int border = lv_obj_get_style_border_width(button, LV_PART_MAIN);
  if (border >= 0 && border <= 2)
    border_width_mask |= 1U << static_cast<unsigned>(border);
  lv_obj_add_state(button, LV_STATE_PRESSED);
  pump(1);
  const int pressed_dx = center2_x(label) - center2_x(button),
            pressed_dy = center2_y(label) - center2_y(button);
  lv_obj_remove_state(button, LV_STATE_PRESSED);
  pump(1);
  ok = ok && std::abs(dx) <= 1 && std::abs(dy) <= 1 && pressed_dx == dx &&
       pressed_dy == dy && contained && content_width && content_height && font;
  ++static_button_samples;
  max_static_center2_x = std::max(max_static_center2_x, std::abs(dx));
  max_static_center2_y = std::max(max_static_center2_y, std::abs(dy));
  max_static_pressed_center_change =
      std::max(max_static_pressed_center_change,
               std::max(std::abs(pressed_dx - dx), std::abs(pressed_dy - dy)));
  if (!ok) {
    std::ostringstream detail;
    detail << "theme=" << theme << ' ' << (dark ? "dark" : "light")
           << " border=" << border << " center2=" << dx << ',' << dy
           << " pressed=" << pressed_dx << ',' << pressed_dy
           << " content=" << content_width << ',' << content_height
           << " font16=" << font << " contained=" << contained
           << " button=" << area_text(button) << " label=" << area_text(label);
    check(false, std::string("static button geometry: ") + name, detail.str());
  }
  return ok;
}

void verify_static_button_geometry_all_appearances(unsigned restore_theme,
                                                   bool restore_dark) {
  const std::array<std::pair<lv_obj_t *, const char *>, 38> buttons{
      {{objects.main_settings_button, "Today Settings"},
       {objects.main_nav_main_button, "Today nav Today"},
       {objects.main_nav_calendar_button, "Today nav Calendar"},
       {objects.main_nav_forecast_button, "Today nav Forecast"},
       {objects.settings_button, "Week Settings"},
       {objects.agenda_previous_button, "Week Previous"},
       {objects.agenda_next_button, "Week Next"},
       {objects.agenda_today_button, "Week Today"},
       {objects.agenda_week_button, "Week mode Week"},
       {objects.agenda_month_button, "Week mode Month"},
       {objects.agenda_nav_main_button, "Week nav Today"},
       {objects.agenda_nav_calendar_button, "Week nav Calendar"},
       {objects.agenda_nav_forecast_button, "Week nav Forecast"},
       {objects.details_back_button, "Details Back"},
       {objects.settings_back_button, "Settings Back"},
       {objects.settings_enter_wifi_button, "Settings Wi-Fi"},
       {objects.settings_start_setup_button, "Settings setup"},
       {objects.settings_sync_now_button, "Settings sync"},
       {objects.settings_brightness_button, "Settings Appearance"},
       {objects.settings_weather_button, "Settings Weather"},
       {objects.settings_firmware_button, "Settings Firmware"},
       {objects.month_settings_button, "Month Settings"},
       {objects.month_previous_button, "Month Previous"},
       {objects.month_next_button, "Month Next"},
       {objects.month_today_button, "Month Today"},
       {objects.month_week_button, "Month mode Week"},
       {objects.month_month_button, "Month mode Month"},
       {objects.weather_back_button, "Forecast Back"},
       {objects.weather_set_location_button, "Forecast Set location"},
       {objects.weather_refresh_button, "Forecast Refresh"},
       {objects.forecast_nav_main_button, "Forecast nav Today"},
       {objects.forecast_nav_calendar_button, "Forecast nav Calendar"},
       {objects.forecast_nav_forecast_button, "Forecast nav Forecast"},
       {objects.brightness_back_button, "Appearance Back"},
       {objects.firmware_back_button, "Firmware Back"},
       {objects.firmware_enable_button, "Firmware Enable"},
       {objects.firmware_cancel_button, "Firmware Cancel"},
       {objects.firmware_reboot_button, "Firmware Reboot"}}};
  unsigned mask = 0, combinations = 0;
  for (unsigned theme = 0; theme < calendar::kThemeCount; ++theme) {
    for (bool dark : {false, true}) {
      select_appearance(theme, dark);
      bool all_ok = true;
      for (const auto &item : buttons)
        all_ok = inspect_static_single_label_button(item.first, item.second,
                                                    theme, dark, mask) &&
                 all_ok;
      std::ostringstream name;
      name << "static button label centers/fonts theme " << theme << ' '
           << (dark ? "dark" : "light");
      check(all_ok, name.str());
      calendar::show_calendar_forecast();
      pump(3);
      const auto container = area_of(objects.forecast_cards_container);
      const auto first = area_of(objects.forecast_day_label_0);
      const auto last = area_of(objects.forecast_day_label_6);
      const int left_margin = first.x1 - container.x1;
      const int right_margin = container.x2 - last.x2;
      std::ostringstream margin_name, margin_detail;
      margin_name << "Forecast equal outer margins theme " << theme << ' '
                  << (dark ? "dark" : "light");
      margin_detail << "left=" << left_margin << " right=" << right_margin
                    << " container="
                    << area_text(objects.forecast_cards_container);
      check(left_margin == right_margin && left_margin == 9, margin_name.str(),
            margin_detail.str());
      ++combinations;
    }
  }
  check((mask & 0x7U) == 0x7U,
        "static button checks exercised border widths 0/1/2",
        std::to_string(mask));
  check(combinations == calendar::kThemeCount * 2U,
        "static button checks cover all appearances",
        std::to_string(combinations));
  std::ostringstream center_detail;
  center_detail << "samples=" << static_button_samples
                << " max-center2=" << max_static_center2_x << ','
                << max_static_center2_y << " (half-pixels), max-pressed-change="
                << max_static_pressed_center_change;
  check(max_static_center2_x <= 1 && max_static_center2_y <= 1 &&
            max_static_pressed_center_change == 0,
        "static button center aggregate", center_detail.str());
  select_appearance(restore_theme, restore_dark);
}

void verify_event_text_direction_and_agenda_viewport() {
  auto verify_rows = [](lv_obj_t *container, const char *screen_name) {
    unsigned checked = 0;
    for (unsigned i = 0; i < lv_obj_get_child_count(container); ++i) {
      auto *row = lv_obj_get_child(container, static_cast<int32_t>(i));
      if (lv_obj_has_flag(row, LV_OBJ_FLAG_HIDDEN))
        continue;
      std::vector<lv_obj_t *> labels;
      collect_descendants(row, &lv_label_class, labels);
      if (labels.size() < 2)
        continue;
      for (unsigned field = 0; field < 2; ++field) {
        std::ostringstream name;
        name << screen_name << " row " << checked << ' '
             << (field == 0 ? "title" : "source") << " LEFT/AUTO";
        const auto align =
            lv_obj_get_style_text_align(labels[field], LV_PART_MAIN);
        const auto direction =
            lv_obj_get_style_base_dir(labels[field], LV_PART_MAIN);
        std::ostringstream detail;
        detail << "align=" << static_cast<int>(align)
               << " base_dir=" << static_cast<int>(direction) << " text='"
               << lv_label_get_text(labels[field]) << '\'';
        check(align == LV_TEXT_ALIGN_LEFT && direction == LV_BASE_DIR_AUTO,
              name.str(), detail.str());
      }
      ++checked;
    }
    check(checked >= 2, std::string(screen_name) + " dynamic rows exercised",
          std::to_string(checked));
  };
  calendar::show_calendar_main();
  pump();
  verify_rows(objects.main_events_list, "Today");
  calendar::show_calendar_week_view();
  pump(20);
  lv_obj_scroll_to_y(objects.agenda_rows_container, 0, LV_ANIM_OFF);
  pump(2);
  verify_rows(objects.agenda_rows_container, "Week");
  lv_obj_update_layout(objects.agenda_rows_container);
  check(lv_obj_get_height(objects.agenda_rows_container) == 186,
        "Week event viewport height",
        std::to_string(lv_obj_get_height(objects.agenda_rows_container)) +
            " px");
  std::vector<lv_obj_t *> visible_rows;
  for (unsigned i = 0;
       i < lv_obj_get_child_count(objects.agenda_rows_container); ++i) {
    auto *row = lv_obj_get_child(objects.agenda_rows_container,
                                 static_cast<int32_t>(i));
    if (!lv_obj_has_flag(row, LV_OBJ_FLAG_HIDDEN))
      visible_rows.push_back(row);
  }
  bool three_fit = visible_rows.size() >= 3;
  int gap01 = -1, gap12 = -1;
  const int pad_top =
      lv_obj_get_style_pad_top(objects.agenda_rows_container, LV_PART_MAIN);
  const int pad_bottom =
      lv_obj_get_style_pad_bottom(objects.agenda_rows_container, LV_PART_MAIN);
  if (three_fit) {
    const auto viewport = area_of(objects.agenda_rows_container);
    const auto first = area_of(visible_rows[0]),
               second = area_of(visible_rows[1]),
               third = area_of(visible_rows[2]);
    gap01 = second.y1 - first.y2 - 1;
    gap12 = third.y1 - second.y2 - 1;
    three_fit = first.y1 >= viewport.y1 && third.y2 <= viewport.y2 &&
                gap01 == 6 && gap12 == 6;
  }
  std::ostringstream viewport_detail;
  viewport_detail << "rows=" << visible_rows.size() << " pad-y=" << pad_top
                  << ',' << pad_bottom << " raster-gaps=" << gap01 << ','
                  << gap12;
  if (visible_rows.size() >= 3)
    viewport_detail << " viewport=" << area_text(objects.agenda_rows_container)
                    << " third=" << area_text(visible_rows[2]);
  check(three_fit && pad_top == 5 && pad_bottom == 5,
        "Week viewport fully contains three rows with 5 px vertical padding",
        viewport_detail.str());

  auto *row = visible_row_containing(objects.agenda_rows_container,
                                     "deliberately long");
  if (row) {
    click(row);
    for (auto *label :
         {objects.details_title_label, objects.details_location_label}) {
      const char *field =
          label == objects.details_title_label ? "title" : "location";
      check(lv_obj_get_style_text_align(label, LV_PART_MAIN) ==
                    LV_TEXT_ALIGN_AUTO &&
                lv_obj_get_style_base_dir(label, LV_PART_MAIN) ==
                    LV_BASE_DIR_AUTO,
            std::string("Details ") + field + " AUTO direction");
    }
  } else
    check(false, "Details direction fixture row exists");
}

void verify_month_and_forecast_columns() {
  calendar::show_calendar_primary_page();
  pump();
  calendar::show_calendar_month_view();
  pump(20);
  auto *table =
      find_descendant_by_type(objects.month_grid_container, &lv_table_class);
  check(table, "Month geometry table exists");
  if (table) {
    const std::array<lv_obj_t *, 7> headings{
        objects.month_weekday_label_0, objects.month_weekday_label_1,
        objects.month_weekday_label_2, objects.month_weekday_label_3,
        objects.month_weekday_label_4, objects.month_weekday_label_5,
        objects.month_weekday_label_6};
    const auto table_area = area_of(table);
    int cursor = table_area.x1;
    bool aligned = true;
    std::ostringstream detail;
    for (unsigned column = 0; column < 7; ++column) {
      const int width = lv_table_get_column_width(table, column);
      const int table_center2 = 2 * cursor + width - 1;
      const int delta = center2_x(headings[column]) - table_center2;
      detail << (column ? "," : "") << width << "/d" << delta;
      aligned = aligned && std::abs(delta) <= 1;
      cursor += width;
    }
    check(aligned, "Month weekday headings follow rendered table columns",
          detail.str());
  }

  calendar::show_calendar_forecast();
  pump();
  const std::array<lv_obj_t *, 7> days{
      objects.forecast_day_label_0, objects.forecast_day_label_1,
      objects.forecast_day_label_2, objects.forecast_day_label_3,
      objects.forecast_day_label_4, objects.forecast_day_label_5,
      objects.forecast_day_label_6};
  const std::array<lv_obj_t *, 7> icons{
      objects.forecast_icon_container_0, objects.forecast_icon_container_1,
      objects.forecast_icon_container_2, objects.forecast_icon_container_3,
      objects.forecast_icon_container_4, objects.forecast_icon_container_5,
      objects.forecast_icon_container_6};
  const std::array<lv_obj_t *, 7> temps{
      objects.forecast_temp_label_0, objects.forecast_temp_label_1,
      objects.forecast_temp_label_2, objects.forecast_temp_label_3,
      objects.forecast_temp_label_4, objects.forecast_temp_label_5,
      objects.forecast_temp_label_6};
  const std::array<lv_obj_t *, 7> conditions{
      objects.forecast_condition_label_0, objects.forecast_condition_label_1,
      objects.forecast_condition_label_2, objects.forecast_condition_label_3,
      objects.forecast_condition_label_4, objects.forecast_condition_label_5,
      objects.forecast_condition_label_6};
  bool aligned = true, equal_steps = true;
  int expected_step = 0;
  std::ostringstream detail;
  for (unsigned i = 0; i < 7; ++i) {
    const int center = center2_x(days[i]);
    const int icon_delta = center2_x(icons[i]) - center;
    const int temp_delta = center2_x(temps[i]) - center,
              condition_delta = center2_x(conditions[i]) - center;
    aligned = aligned && std::abs(icon_delta) <= 1 &&
              std::abs(temp_delta) <= 1 && std::abs(condition_delta) <= 1;
    if (i) {
      const int step = center2_x(days[i]) - center2_x(days[i - 1]);
      if (i == 1)
        expected_step = step;
      else
        equal_steps = equal_steps && step == expected_step;
    }
    detail << (i ? "," : "") << "c" << center << "/" << icon_delta << '/'
           << temp_delta << '/' << condition_delta;
  }
  check(aligned && equal_steps,
        "Forecast seven columns share centers and equal pitch", detail.str());
}

lv_obj_t *button_with_text(lv_obj_t *parent, const char *text) {
  auto *label = find_label(parent, text, true);
  return label ? ancestor_of_type(label, &lv_button_class) : nullptr;
}

void verify_lazy_forms_and_capture(const std::string &directory) {
  calendar::show_calendar_settings();
  pump();
  calendar::open_calendar_display_wifi_setup();
  pump(20);
  auto *wifi_form = visible_overlay(objects.settings_screen);
  check(wifi_form, "lazy Wi-Fi form is visible");
  if (wifi_form) {
    std::vector<lv_obj_t *> textareas, dropdowns, keyboards;
    collect_descendants(wifi_form, &lv_textarea_class, textareas);
    collect_descendants(wifi_form, &lv_dropdown_class, dropdowns);
    collect_descendants(wifi_form, &lv_keyboard_class, keyboards);
    check(textareas.size() == 2, "Wi-Fi form has SSID/password fields",
          std::to_string(textareas.size()));
    check(dropdowns.size() == 1 &&
              !lv_obj_has_flag(dropdowns[0], LV_OBJ_FLAG_HIDDEN),
          "Wi-Fi scan picker is reachable with fixture",
          std::to_string(dropdowns.size()));
    check(keyboards.size() == 1, "Wi-Fi form has one keyboard",
          std::to_string(keyboards.size()));
    auto *feedback = label_at_local_position(wifi_form, 20, 136);
    auto *connect = button_with_text(wifi_form, "Connect");
    if (feedback) {
      const auto form = area_of(wifi_form), a = area_of(feedback);
      check(a.y1 - form.y1 == 136 && lv_obj_get_height(feedback) == 24,
            "Wi-Fi feedback geometry", area_text(feedback));
    } else
      check(false, "Wi-Fi feedback label found");
    if (connect) {
      const auto form = area_of(wifi_form), a = area_of(connect);
      check(a.y1 - form.y1 == 168 && lv_obj_get_height(connect) == 44,
            "Wi-Fi action row geometry", area_text(connect));
    } else
      check(false, "Wi-Fi Connect button found");
    if (connect && !keyboards.empty()) {
      const int gap = blank_gap_y(connect, keyboards[0]);
      const int row_gap = lv_obj_get_style_pad_row(keyboards[0], LV_PART_MAIN);
      const int effective_row =
          (lv_obj_get_content_height(keyboards[0]) - 3 * row_gap) / 4;
      std::ostringstream detail;
      detail << "keyboard=" << area_text(keyboards[0]) << " gap=" << gap
             << " effective-row=" << effective_row;
      check(lv_obj_get_height(keyboards[0]) == 188 && gap >= 8 &&
                effective_row >= 44,
            "Wi-Fi keyboard preserves action gap and four 44 px rows",
            detail.str());
    }
    capture(directory + "/wifi-form.ppm");
  }

  calendar::show_calendar_weather();
  pump();
  calendar::open_calendar_weather_location_editor();
  pump(20);
  auto *weather_form = visible_overlay(objects.weather_screen);
  check(weather_form, "lazy Weather form is visible");
  if (weather_form) {
    std::vector<lv_obj_t *> textareas, dropdowns, keyboards;
    collect_descendants(weather_form, &lv_textarea_class, textareas);
    collect_descendants(weather_form, &lv_dropdown_class, dropdowns);
    collect_descendants(weather_form, &lv_keyboard_class, keyboards);
    std::sort(textareas.begin(), textareas.end(), [](lv_obj_t *a, lv_obj_t *b) {
      return area_of(a).x1 < area_of(b).x1;
    });
    check(textareas.size() == 3, "Weather form has three text fields",
          std::to_string(textareas.size()));
    check(dropdowns.size() == 1, "Weather form has one Units dropdown",
          std::to_string(dropdowns.size()));
    const std::array<const char *, 4> caption_names{"Location", "Latitude",
                                                    "Longitude", "Units"};
    const std::array<int, 4> expected_x{20, 218, 363, 508};
    bool captions_ok = true;
    const auto form_area = area_of(weather_form);
    for (unsigned i = 0; i < caption_names.size(); ++i) {
      auto *caption = find_label(weather_form, caption_names[i], true);
      lv_obj_t *field = nullptr;
      if (i < 3 && i < textareas.size())
        field = textareas[i];
      else if (i == 3 && !dropdowns.empty())
        field = dropdowns[0];
      const bool ok = caption && field &&
                      area_of(caption).x1 - form_area.x1 == expected_x[i] &&
                      area_of(caption).y2 < area_of(field).y1 &&
                      area_of(field).y1 - form_area.y1 == 60;
      captions_ok = captions_ok && ok;
      if (!ok) {
        std::ostringstream name;
        name << "Weather persistent caption " << caption_names[i];
        check(false, name.str(), caption ? area_text(caption) : "missing");
      }
    }
    check(captions_ok, "Weather fields retain four persistent captions");
    if (!dropdowns.empty()) {
      const char *options = lv_dropdown_get_options(dropdowns[0]);
      check(options && std::strcmp(options, "Metric\nImperial") == 0,
            "Weather Units options are concise", options ? options : "null");
      const auto *font = lv_obj_get_style_text_font(dropdowns[0], LV_PART_MAIN);
      const int letter =
          lv_obj_get_style_text_letter_space(dropdowns[0], LV_PART_MAIN);
      const int symbol = lv_text_get_width(
          LV_SYMBOL_DOWN, std::strlen(LV_SYMBOL_DOWN), font, letter);
      bool fit = true;
      std::ostringstream detail;
      for (const char *option : {"Metric", "Imperial"}) {
        const int width =
            lv_text_get_width(option, std::strlen(option), font, letter);
        const int required =
            width + symbol +
            lv_obj_get_style_pad_column(dropdowns[0], LV_PART_MAIN);
        fit = fit && required <= lv_obj_get_content_width(dropdowns[0]);
        detail << option << '=' << required << '/'
               << lv_obj_get_content_width(dropdowns[0]) << ' ';
      }
      check(fit, "Weather Units text and arrow fit", detail.str());
    }
    auto *cancel = button_with_text(weather_form, "Cancel");
    auto *save = button_with_text(weather_form, "Save");
    auto *feedback = label_at_local_position(weather_form, 20, 116);
    if (cancel) {
      const auto a = area_of(cancel);
      check(a.x1 - form_area.x1 == 674 && a.y1 - form_area.y1 == 60,
            "Weather Cancel geometry", area_text(cancel));
    } else
      check(false, "Weather Cancel button found");
    if (save) {
      const auto a = area_of(save);
      check(a.y1 - form_area.y1 == 114, "Weather Save geometry",
            area_text(save));
    } else
      check(false, "Weather Save button found");
    if (feedback) {
      const auto a = area_of(feedback);
      check(a.y1 - form_area.y1 == 116 && lv_obj_get_height(feedback) <= 32,
            "Weather feedback geometry", area_text(feedback));
    } else
      check(false, "Weather feedback label found");
    if (save && !keyboards.empty()) {
      const int gap = blank_gap_y(save, keyboards[0]);
      std::ostringstream detail;
      detail << "keyboard=" << area_text(keyboards[0]) << " gap=" << gap;
      check(lv_obj_get_height(keyboards[0]) == 236 && gap >= 14,
            "Weather keyboard clears Save row", detail.str());
    }
    capture(directory + "/weather-form.ppm");
  }
}

void verify_firmware_states_and_capture(const std::string &directory) {
  struct StateCase {
    board::OtaState state;
    const char *name;
    lv_obj_t *enabled;
  };
  const std::array<StateCase, 3> cases{
      {{board::OtaState::Disabled, "disabled", objects.firmware_enable_button},
       {board::OtaState::Armed, "armed", objects.firmware_cancel_button},
       {board::OtaState::ReadyToReboot, "ready",
        objects.firmware_reboot_button}}};
  for (const auto &state_case : cases) {
    preview::ota_status = {};
    preview::ota_status.state = state_case.state;
    if (state_case.state == board::OtaState::Disabled)
      std::strcpy(preview::ota_status.detail, "Firmware updates are disabled");
    else if (state_case.state == board::OtaState::Armed) {
      std::strcpy(preview::ota_status.upload_url, "http://192.168.1.42/update");
      std::strcpy(preview::ota_status.one_time_code, "482731");
      preview::ota_status.seconds_remaining = 300;
    } else
      std::strcpy(preview::ota_status.detail,
                  "Verified image is ready to install");
    calendar::show_calendar_firmware_update();
    pump(10);
    const std::array<lv_obj_t *, 3> actions{objects.firmware_enable_button,
                                            objects.firmware_cancel_button,
                                            objects.firmware_reboot_button};
    unsigned enabled_count = 0;
    for (auto *action : actions)
      if (!lv_obj_has_state(action, LV_STATE_DISABLED))
        ++enabled_count;
    const bool correct =
        enabled_count == 1 &&
        !lv_obj_has_state(state_case.enabled, LV_STATE_DISABLED);
    std::ostringstream detail;
    detail << "enabled=" << enabled_count << " expected="
           << lv_label_get_text(direct_single_label(state_case.enabled));
    check(correct,
          std::string("Firmware valid primary action ") + state_case.name,
          detail.str());
    const auto &palette = calendar::palette_for(preview::appearance.theme_id,
                                                preview::appearance.dark_theme);
    const auto actual = rgb888(
        lv_obj_get_style_bg_color_filtered(state_case.enabled, LV_PART_MAIN));
    const auto expected = rgb888(lv_color_hex(palette.action));
    check(actual == expected,
          std::string("Firmware primary action paint ") + state_case.name,
          contrast_detail(actual, expected, actual == expected ? 1.0 : 0.0));
    capture(directory + "/firmware-" + state_case.name + ".ppm");
  }
}

void verify_settings_caption_alignment() {
  const std::array<std::pair<lv_obj_t *, lv_obj_t *>, 3> pairs{
      {{objects.settings_wifi_caption_label,
        objects.settings_wifi_status_label},
       {objects.settings_time_caption_label,
        objects.settings_time_status_label},
       {objects.settings_feed_caption_label,
        objects.settings_feed_status_label}}};
  auto verify = [&](const char *state) {
    calendar::show_calendar_settings();
    pump(20);
    bool all = true;
    std::ostringstream detail;
    for (unsigned i = 0; i < pairs.size(); ++i) {
      auto *caption = pairs[i].first;
      auto *value = pairs[i].second;
      const int delta = center2_y(value) - center2_y(caption);
      const int height = lv_obj_get_height(value);
      const bool fits = std::abs(delta) <= 1 && height <= 48 &&
                        area_of(caption).x2 < area_of(value).x1;
      all = all && fits;
      detail << (i ? "," : "") << "d" << delta << "/h" << height;
    }
    check(all, std::string("Settings caption/value vertical centers ") + state,
          detail.str());
  };
  verify("connected multiline");
  const auto saved = preview::connection;
  preview::connection = {};
  preview::connection.service_initialized = true;
  calendar::show_calendar_settings();
  pump(60);
  verify("single-line missing setup");
  preview::connection = saved;
  calendar::show_calendar_settings();
  pump(60);
}

void check_pressed_descendants(lv_obj_t *button, std::string_view name) {
  if (!button)
    return;
  lv_obj_add_state(button, LV_STATE_PRESSED);
  pump(2);
  const auto bg =
      rgb565_expanded(lv_obj_get_style_bg_color_filtered(button, LV_PART_MAIN));
  std::vector<lv_obj_t *> pending;
  for (unsigned i = 0; i < lv_obj_get_child_count(button); ++i)
    pending.push_back(lv_obj_get_child(button, static_cast<int32_t>(i)));
  unsigned label_index = 0;
  bool found = false;
  while (!pending.empty()) {
    auto *child = pending.back();
    pending.pop_back();
    for (unsigned i = 0; i < lv_obj_get_child_count(child); ++i)
      pending.push_back(lv_obj_get_child(child, static_cast<int32_t>(i)));
    if (!lv_obj_check_type(child, &lv_label_class) ||
        lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN))
      continue;
    found = true;
    const auto fg = rgb565_expanded(
        lv_obj_get_style_text_color_filtered(child, LV_PART_MAIN));
    const double ratio = contrast(fg, bg);
    const char *text = lv_label_get_text(child);
    const bool icon =
        text && (std::strcmp(text, "●") == 0 || std::strcmp(text, "○") == 0);
    std::ostringstream label;
    label << "pressed " << name << " child " << label_index++ << " ["
          << (text ? text : "") << ']';
    check(ratio >= (icon ? kIconContrast : kTextContrast), label.str(),
          contrast_detail(fg, bg, ratio));
  }
  check(found,
        std::string("pressed ") + std::string(name) + " has label child");
  lv_obj_remove_state(button, LV_STATE_PRESSED);
  pump(2);
}
void check_button_height(lv_obj_t *button, std::string_view name) {
  if (!button || lv_obj_has_flag(button, LV_OBJ_FLAG_HIDDEN))
    return;
  lv_obj_update_layout(button);
  const auto h = lv_obj_get_height(button);
  check(h >= 44, std::string(name) + " touch height",
        std::to_string(h) + " px");
}
void check_wrapped_label(lv_obj_t *label, std::string_view name) {
  if (!label || lv_obj_has_flag(label, LV_OBJ_FLAG_HIDDEN))
    return;
  lv_point_t size{};
  const char *text = lv_label_get_text(label);
  const auto *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
  lv_text_get_size(&size, text ? text : "", font,
                   lv_obj_get_style_text_letter_space(label, LV_PART_MAIN),
                   lv_obj_get_style_text_line_space(label, LV_PART_MAIN),
                   lv_obj_get_content_width(label), LV_TEXT_FLAG_NONE);
  std::ostringstream detail;
  detail << "text " << size.x << 'x' << size.y << " in "
         << lv_obj_get_content_width(label) << 'x'
         << lv_obj_get_content_height(label);
  check(size.y <= lv_obj_get_content_height(label), name, detail.str());
}
void check_single_line_label(lv_obj_t *label, std::string_view name) {
  if (!label || lv_obj_has_flag(label, LV_OBJ_FLAG_HIDDEN))
    return;
  lv_point_t size{};
  const char *text = lv_label_get_text(label);
  const auto *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
  lv_text_get_size(&size, text ? text : "", font,
                   lv_obj_get_style_text_letter_space(label, LV_PART_MAIN),
                   lv_obj_get_style_text_line_space(label, LV_PART_MAIN),
                   LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  std::ostringstream detail;
  detail << "text " << size.x << " px in " << lv_obj_get_content_width(label)
         << " px";
  check(size.x <= lv_obj_get_content_width(label), name, detail.str());
}

void capture_primary_screens(const std::string &directory) {
  calendar::show_calendar_main();
  pump();
  capture(directory + "/today.ppm");
  calendar::show_calendar_primary_page();
  pump(20);
  capture(directory + "/week.ppm");
  if (auto *row = visible_row_containing(objects.agenda_rows_container,
                                         "deliberately long")) {
    click(row);
    capture(directory + "/details.ppm");
    click(objects.details_back_button);
  } else
    check(false, "primary capture Details fixture row exists");
  calendar::show_calendar_month_view();
  pump(20);
  capture(directory + "/month.ppm");
  calendar::show_calendar_forecast();
  pump();
  capture(directory + "/forecast.ppm");
  calendar::show_calendar_settings();
  pump();
  capture(directory + "/settings.ppm");
  calendar::show_calendar_brightness_settings();
  pump();
  capture(directory + "/appearance.ppm");
  check(objects.automatic_brightness_switch != nullptr &&
            lv_obj_has_flag(objects.automatic_brightness_switch, LV_OBJ_FLAG_CHECKABLE),
        "GPIO17 automatic brightness switch is present and checkable");
  check(std::strstr(lv_label_get_text(objects.brightness_sensor_hint_label),
                    "no ambient-light sensor") == nullptr,
        "Appearance no longer displays the manual-only sensor claim");
  lv_area_t auto_area{}, hint_area{}, slider_area{};
  lv_obj_get_coords(objects.automatic_brightness_switch, &auto_area);
  lv_obj_get_coords(objects.brightness_sensor_hint_label, &hint_area);
  lv_obj_get_coords(objects.brightness_slider, &slider_area);
  check(slider_area.y2 < auto_area.y1 && auto_area.y2 < hint_area.y1,
        "GPIO17 switch has clear space below slider and above hint");
  const auto manual_brightness = preview::appearance.brightness_percent;
  const auto toggle_auto = [](bool enabled) {
    if (enabled) lv_obj_add_state(objects.automatic_brightness_switch, LV_STATE_CHECKED);
    else lv_obj_remove_state(objects.automatic_brightness_switch, LV_STATE_CHECKED);
    lv_obj_send_event(objects.automatic_brightness_switch, LV_EVENT_VALUE_CHANGED, nullptr);
    pump(4);
  };
  preview::ambient_light_raw = 3500;
  toggle_auto(true);
  check(preview::last_backlight_percent == 10 &&
            preview::appearance.automatic_brightness &&
            preview::appearance.brightness_percent == manual_brightness &&
            lv_obj_has_state(objects.brightness_slider, LV_STATE_DISABLED),
        "GPIO17 high dark sample dims to 10 percent and retains manual preference");
  capture(directory + "/appearance-auto-dark.ppm");
  toggle_auto(false);
  check(preview::last_backlight_percent == manual_brightness &&
            !preview::appearance.automatic_brightness &&
            !lv_obj_has_state(objects.brightness_slider, LV_STATE_DISABLED),
        "Disabling GPIO17 automatic brightness restores manual setting");
  preview::ambient_light_raw = 300;
  toggle_auto(true);
  check(preview::last_backlight_percent == 100 &&
            lv_slider_get_value(objects.brightness_slider) == 100,
        "GPIO17 low bright sample sets backlight and slider to 100 percent");
  capture(directory + "/appearance-auto-bright.ppm");
  calendar::show_calendar_main();
  calendar::show_calendar_brightness_settings();
  pump(4);
  check(std::strcmp(lv_label_get_text(objects.brightness_value_label), "100% Auto") == 0,
        "Reopening Appearance preserves the automatic brightness label");
  toggle_auto(false);
  toggle_auto(true);
  check(preview::last_backlight_percent == 100,
        "Reenabling Auto reapplies unchanged ADC brightness after manual override");
  preview::reset_runtime_counters();
  preview::ambient_light_raw = 3500;
  pump(50);
  check(preview::last_backlight_percent > 10 && preview::last_backlight_percent < 100 &&
            preview::appearance_save_count == 0,
        "GPIO17 filtering smooths a light change without writing preferences");
  toggle_auto(false);
  preview::ambient_light_raw = 300;
  calendar::show_calendar_firmware_update();
  pump();
  capture(directory + "/firmware.ppm");
}
void capture_pressed_states(const std::string &directory) {
  calendar::show_calendar_main();
  pump();
  lv_obj_add_state(objects.main_wifi_button, LV_STATE_PRESSED);
  capture(directory + "/pressed-main-wifi.ppm");
  lv_obj_remove_state(objects.main_wifi_button, LV_STATE_PRESSED);
  pump(2);
  lv_obj_add_state(objects.main_settings_button, LV_STATE_PRESSED);
  capture(directory + "/pressed-main-settings.ppm");
  lv_obj_remove_state(objects.main_settings_button, LV_STATE_PRESSED);
  pump(2);
  auto *row = visible_child(objects.main_events_list);
  if (row) {
    lv_obj_add_state(row, LV_STATE_PRESSED);
    capture(directory + "/pressed-main-row.ppm");
    lv_obj_remove_state(row, LV_STATE_PRESSED);
    pump(2);
  }
  lv_obj_add_state(objects.main_weather_card, LV_STATE_PRESSED);
  capture(directory + "/pressed-main-weather.ppm");
  lv_obj_remove_state(objects.main_weather_card, LV_STATE_PRESSED);
  pump(2);
  lv_obj_add_state(objects.main_nav_calendar_button, LV_STATE_PRESSED);
  capture(directory + "/pressed-main-navigation.ppm");
  lv_obj_remove_state(objects.main_nav_calendar_button, LV_STATE_PRESSED);
  pump(2);
}
void verify_pressed_contrast() {
  calendar::show_calendar_main();
  pump();
  for (const auto &item : std::array<std::pair<lv_obj_t *, const char *>, 5>{
           {{objects.main_wifi_button, "main Wi-Fi"},
            {objects.main_settings_button, "main Settings"},
            {objects.main_nav_main_button, "main Today nav"},
            {objects.main_nav_calendar_button, "main Calendar nav"},
            {objects.main_nav_forecast_button, "main Forecast nav"}}})
    check_pressed_descendants(item.first, item.second);
  if (auto *row = visible_child(objects.main_events_list))
    check_pressed_descendants(row, "main event row");
  check_pressed_descendants(objects.main_weather_card, "main weather card");
  calendar::show_calendar_primary_page();
  pump();
  for (const auto &item : std::array<std::pair<lv_obj_t *, const char *>, 10>{
           {{objects.agenda_wifi_button, "week Wi-Fi"},
            {objects.settings_button, "week Settings"},
            {objects.agenda_previous_button, "week Previous"},
            {objects.agenda_next_button, "week Next"},
            {objects.agenda_today_button, "week Today"},
            {objects.agenda_week_button, "week mode"},
            {objects.agenda_month_button, "month mode"},
            {objects.agenda_nav_main_button, "week Today nav"},
            {objects.agenda_nav_calendar_button, "week Calendar nav"},
            {objects.agenda_nav_forecast_button, "week Forecast nav"}}})
    check_pressed_descendants(item.first, item.second);
  if (auto *row = visible_child(objects.agenda_rows_container))
    check_pressed_descendants(row, "week event row");
  calendar::show_calendar_month_view();
  pump();
  check_pressed_descendants(objects.month_wifi_button, "month Wi-Fi");
  check_pressed_descendants(objects.month_settings_button, "month Settings");
  calendar::show_calendar_forecast();
  pump();
  for (const auto &item : std::array<std::pair<lv_obj_t *, const char *>, 7>{
           {{objects.weather_back_button, "forecast Back"},
            {objects.weather_refresh_button, "forecast Refresh"},
            {objects.weather_set_location_button, "forecast location"},
            {objects.forecast_wifi_button, "forecast Wi-Fi"},
            {objects.forecast_nav_main_button, "forecast Today nav"},
            {objects.forecast_nav_calendar_button, "forecast Calendar nav"},
            {objects.forecast_nav_forecast_button, "forecast Forecast nav"}}})
    check_pressed_descendants(item.first, item.second);
  calendar::show_calendar_settings();
  pump();
  check_pressed_descendants(objects.settings_back_button, "settings Back");
  for (const auto &item : std::array<std::pair<lv_obj_t *, const char *>, 6>{
           {{objects.settings_enter_wifi_button, "settings Wi-Fi"},
            {objects.settings_start_setup_button, "settings setup"},
            {objects.settings_sync_now_button, "settings sync"},
            {objects.settings_brightness_button, "settings appearance"},
            {objects.settings_weather_button, "settings weather"},
            {objects.settings_firmware_button, "settings firmware"}}})
    check_pressed_descendants(item.first, item.second);
  calendar::show_calendar_brightness_settings();
  pump();
  check_pressed_descendants(objects.brightness_back_button, "appearance Back");
}
void verify_geometry() {
  calendar::show_calendar_main();
  pump();
  lv_obj_update_layout(objects.main_screen);
  lv_area_t events{}, weather{};
  lv_obj_get_coords(objects.main_events_container, &events);
  lv_obj_get_coords(objects.main_weather_card, &weather);
  check(events.y1 == weather.y1, "main card top alignment");
  check(events.y2 == weather.y2, "main card bottom alignment");
  check(descendant_label_contains(objects.main_events_list, "Work"),
        "Work source identity visible");
  check(descendant_label_contains(objects.main_events_list, "Family"),
        "Family source identity visible");
  check_wrapped_label(objects.main_weather_feels_value_label,
                      "main feels value fit");
  check_wrapped_label(objects.main_weather_humidity_value_label,
                      "main humidity value fit");
  check_wrapped_label(objects.main_weather_wind_value_label,
                      "main wind value fit");
  for (const auto &item : std::array<std::pair<lv_obj_t *, const char *>, 7>{
           {{objects.main_wifi_button, "main Wi-Fi"},
            {objects.main_settings_button, "main Settings"},
            {objects.main_weather_card, "main weather"},
            {objects.main_nav_main_button, "main Today nav"},
            {objects.main_nav_calendar_button, "main Calendar nav"},
            {objects.main_nav_forecast_button, "main Forecast nav"},
            {visible_child(objects.main_events_list), "main event row"}}})
    check_button_height(item.first, item.second);
  calendar::show_calendar_primary_page();
  pump();
  calendar::show_calendar_month_view();
  pump();
  auto *table =
      find_descendant_by_type(objects.month_grid_container, &lv_table_class);
  lv_mem_monitor_t month_memory{};
  lv_mem_monitor(&month_memory);
  std::ostringstream month_detail;
  month_detail << "children="
               << lv_obj_get_child_count(objects.month_grid_container)
               << ", state='" << lv_label_get_text(objects.month_state_label)
               << "', free=" << month_memory.free_size
               << ", biggest=" << month_memory.free_biggest_size;
  check(table, "month table exists", month_detail.str());
  if (table) {
    check(lv_table_get_row_count(table) == 6, "month has six rows");
    check(lv_obj_get_height(table) >= 264, "month rows reserve 44 px",
          std::to_string(lv_obj_get_height(table)) + " px table");
  }
  calendar::show_calendar_forecast();
  pump();
  check(!lv_obj_has_flag(objects.forecast_wifi_button, LV_OBJ_FLAG_HIDDEN),
        "forecast Wi-Fi control visible");
  for (auto *label :
       {objects.forecast_day_label_0, objects.forecast_day_label_1,
        objects.forecast_day_label_2, objects.forecast_day_label_3,
        objects.forecast_day_label_4, objects.forecast_day_label_5,
        objects.forecast_day_label_6})
    check_single_line_label(label, "forecast day label fit");
  for (auto *label :
       {objects.forecast_condition_label_0, objects.forecast_condition_label_1,
        objects.forecast_condition_label_2, objects.forecast_condition_label_3,
        objects.forecast_condition_label_4, objects.forecast_condition_label_5,
        objects.forecast_condition_label_6})
    check_wrapped_label(label, "forecast condition fit");
  calendar::show_calendar_brightness_settings();
  pump();
  lv_area_t back{}, heading{};
  lv_obj_get_coords(objects.brightness_back_button, &back);
  lv_obj_get_coords(objects.appearance_heading_label, &heading);
  check(back.x2 < heading.x1 || heading.x2 < back.x1,
        "appearance Back/title do not overlap",
        std::to_string(back.x2) + " vs " + std::to_string(heading.x1));
}
void verify_details_and_recovery(const std::string &directory) {
  calendar::show_calendar_week_view();
  pump(30);
  auto *calendar_row = visible_row_containing(objects.agenda_rows_container,
                                              "deliberately long");
  check(calendar_row, "calendar long event row exists");
  if (calendar_row) {
    click(calendar_row);
    check(lv_screen_active() == objects.event_details_screen,
          "calendar row opens Details");
    capture(directory + "/details-calendar-long.ppm");
    check_wrapped_label(objects.details_title_label, "details long title fit");
    check_wrapped_label(objects.details_location_label,
                        "details long location fit");
    click(objects.details_back_button);
    check(lv_screen_active() == objects.agenda_screen,
          "Details Back returns to Week");

    auto *missing_row =
        visible_row_containing(objects.agenda_rows_container, "birthday");
    if (!missing_row)
      missing_row =
          visible_row_containing(objects.agenda_rows_container, "Birthday");
    check(missing_row, "missing-location fixture row exists");
    if (missing_row) {
      click(missing_row);
      check(lv_obj_has_flag(objects.details_location_label, LV_OBJ_FLAG_HIDDEN),
            "missing location is omitted in Details");
      const char *title = lv_label_get_text(objects.details_title_label);
      if (title && std::strstr(title, "יום הולדת")) {
        check(lv_obj_get_style_base_dir(objects.details_title_label,
                                        LV_PART_MAIN) == LV_BASE_DIR_AUTO,
              "mixed Hebrew/English Details title keeps AUTO direction", title);
      }
      capture(directory + "/details-missing-location.ppm");
      click(objects.details_back_button);
    }

    lv_obj_scroll_to_y(objects.agenda_rows_container, 8, LV_ANIM_OFF);
    pump(2);
    const int32_t saved_scroll =
        lv_obj_get_scroll_y(objects.agenda_rows_container);
    check(saved_scroll > 0,
          "Calendar recovery fixture has a saved scroll position",
          std::to_string(saved_scroll) + " px");
    calendar_row = visible_row_containing(objects.agenda_rows_container,
                                          "deliberately long");
    if (calendar_row) {
      click(calendar_row);
      preview::queue_calendar(
          "X-ESP32-CALENDAR-ID:work\r\nX-ESP32-CALENDAR-NAME:Work\r\nX-ESP32-"
          "CALENDAR-COLOR:2D7FF9\r\n"
          "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nUID:"
          "standup\r\nDTSTART:20260928T073000Z\r\nDTEND:"
          "20260928T080000Z\r\nSUMMARY:Team stand-up\r\nLOCATION:Main "
          "conference "
          "room\r\nEND:VEVENT\r\nBEGIN:VEVENT\r\nUID:design-review\r\nDTSTART:"
          "20260928T083000Z\r\nDTEND:20260928T090000Z\r\nSUMMARY:Design "
          "review\r\nEND:VEVENT\r\nBEGIN:VEVENT\r\nUID:project-"
          "checkin\r\nDTSTART:20260928T113000Z\r\nDTEND:"
          "20260928T120000Z\r\nSUMMARY:Project "
          "check-in\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n"
          "X-ESP32-CALENDAR-ID:family\r\nX-ESP32-CALENDAR-NAME:Family\r\nX-"
          "ESP32-CALENDAR-COLOR:35A854\r\n"
          "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nUID:"
          "birthday\r\nDTSTART;VALUE=DATE:20260928\r\nDTEND;VALUE=DATE:"
          "20260929\r\nSUMMARY:Maya's "
          "birthday\r\nEND:VEVENT\r\nBEGIN:VEVENT\r\nUID:"
          "appointment\r\nDTSTART:20260928T130000Z\r\nDTEND:"
          "20260928T133000Z\r\nSUMMARY:Dentist "
          "appointment\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n");
      pump(80);
      check(lv_screen_active() == objects.agenda_screen,
            "deleted Calendar event import recovers to Week origin");
      const char *footer = lv_label_get_text(objects.footer_label);
      check(footer && std::strstr(footer, "no longer available"),
            "deleted Calendar event notice is visible",
            footer ? footer : "null");
      const int32_t restored_scroll =
          lv_obj_get_scroll_y(objects.agenda_rows_container);
      check(restored_scroll == saved_scroll,
            "deleted Calendar event restores Week scroll",
            std::to_string(saved_scroll) + " -> " +
                std::to_string(restored_scroll) + " px");
      capture(directory + "/details-deleted-calendar-recovery.ppm");
      lv_obj_scroll_to_y(objects.agenda_rows_container, 40, LV_ANIM_OFF);
      pump(2);
      const int32_t pre_clamp_scroll =
          lv_obj_get_scroll_y(objects.agenda_rows_container);
      preview::queue_calendar(
          "X-ESP32-CALENDAR-ID:work\r\nX-ESP32-CALENDAR-NAME:Work\r\nX-ESP32-"
          "CALENDAR-COLOR:2D7FF9\r\n"
          "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nUID:"
          "standup\r\nDTSTART:20260928T073000Z\r\nDTEND:"
          "20260928T080000Z\r\nSUMMARY:Team "
          "stand-up\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n"
          "X-ESP32-CALENDAR-ID:family\r\nX-ESP32-CALENDAR-NAME:Family\r\nX-"
          "ESP32-CALENDAR-COLOR:35A854\r\n"
          "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nUID:"
          "birthday\r\nDTSTART;VALUE=DATE:20260928\r\nDTEND;VALUE=DATE:"
          "20260929\r\nSUMMARY:Maya's "
          "birthday\r\nEND:VEVENT\r\nBEGIN:VEVENT\r\nUID:"
          "appointment\r\nDTSTART:20260928T130000Z\r\nDTEND:"
          "20260928T133000Z\r\nSUMMARY:Dentist "
          "appointment\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n");
      pump(80);
      const int32_t clamped_scroll =
          lv_obj_get_scroll_y(objects.agenda_rows_container);
      check(pre_clamp_scroll > 0 && clamped_scroll == 0,
            "Week scroll clamps when refreshed content shrinks to three "
            "fitting rows",
            std::to_string(pre_clamp_scroll) + " -> " +
                std::to_string(clamped_scroll) + " px");
    }
  }
  calendar::show_calendar_main();
  pump();
  auto *main_row = visible_child(objects.main_events_list);
  check(main_row, "main event row exists for deletion recovery");
  if (main_row) {
    click(main_row);
    check(lv_screen_active() == objects.event_details_screen,
          "main row opens Details");
    calendar::refresh_without_selected_event_for_debug();
    pump();
    check(lv_screen_active() == objects.main_screen,
          "deleted Main event recovers to Main origin");
    const char *notice = lv_label_get_text(objects.main_events_hint_label);
    check(notice && std::strstr(notice, "no longer available"),
          "deleted event notice is visible", notice ? notice : "null");
    capture(directory + "/details-deleted-main-recovery.ppm");
  }
}
void capture_status_states(const std::string &directory) {
  calendar::show_calendar_main();
  preview::connection = {};
  pump(20);
  check(std::strcmp(lv_label_get_text(objects.main_wifi_label),
                    "Starting...") == 0,
        "starting connection state");
  capture(directory + "/status-starting.ppm");
  preview::connection.service_initialized = true;
  preview::connection.setup_ap_active = true;
  std::strcpy(preview::connection.setup_ssid, "Calendar Setup");
  std::strcpy(preview::connection.setup_password, "fixture-only");
  std::strcpy(preview::connection.setup_address, "192.168.4.1");
  pump(20);
  check(std::strcmp(lv_label_get_text(objects.main_wifi_label),
                    "Setup Wi-Fi") == 0,
        "setup AP connection state");
  capture(directory + "/status-setup.ppm");
  preview::set_fixture(false, false);
  pump(40);
  preview::connection.fetch_state = board::CalendarFetchState::Failed;
  std::strcpy(preview::connection.fetch_detail, "HTTPS read timeout");
  pump(20);
  check(std::strcmp(lv_label_get_text(objects.main_wifi_label),
                    "Wi-Fi connected") == 0,
        "connected state independent of sync failure");
  check(std::strstr(lv_label_get_text(objects.main_sync_state_label),
                    "Sync failed - saved") != nullptr,
        "sync failure overrides prior success",
        lv_label_get_text(objects.main_sync_state_label));
  capture(directory + "/status-connected-sync-failed.ppm");
}
void capture_negative_weather(const std::string &directory) {
  const auto saved = preview::weather;
  preview::weather.current.temperature_tenths_c = -300;
  preview::weather.current.feels_like_tenths_c = -325;
  for (std::size_t i = 0; i < preview::weather.forecast_count; ++i) {
    preview::weather.forecast[i].minimum_tenths_c =
        -350 + static_cast<int>(i) * 5;
    preview::weather.forecast[i].maximum_tenths_c =
        -300 + static_cast<int>(i) * 5;
  }
  ++preview::weather_status.generation;
  calendar::show_calendar_main();
  pump(20);
  check_single_line_label(objects.main_weather_temperature_label,
                          "negative Main temperature fit");
  capture(directory + "/weather-negative-main.ppm");
  calendar::show_calendar_forecast();
  pump(20);
  check_single_line_label(objects.weather_temperature_label,
                          "negative Forecast temperature fit");
  for (auto *label :
       {objects.forecast_temp_label_0, objects.forecast_temp_label_1,
        objects.forecast_temp_label_2, objects.forecast_temp_label_3,
        objects.forecast_temp_label_4, objects.forecast_temp_label_5,
        objects.forecast_temp_label_6})
    check_single_line_label(label, "negative daily range fit");
  capture(directory + "/weather-negative-forecast.ppm");
  preview::weather = saved;
  ++preview::weather_status.generation;
  pump(20);
}
void stress_navigation_and_themes() {
  calendar::show_calendar_settings();
  pump();
  calendar::show_calendar_brightness_settings();
  pump();
  for (unsigned warm = 0; warm < calendar::kThemeCount; ++warm) {
    lv_dropdown_set_selected(objects.theme_style_dropdown, warm);
    calendar::change_calendar_theme_style();
    calendar::show_calendar_main();
    calendar::show_calendar_primary_page();
    calendar::show_calendar_month_view();
    calendar::show_calendar_settings();
    calendar::show_calendar_brightness_settings();
    pump(3);
  }
  pump(20);
  lv_mem_monitor_t before{};
  lv_mem_monitor(&before);
  const unsigned objects_before = total_objects();
  for (unsigned i = 0; i < 80; ++i) {
    lv_dropdown_set_selected(objects.theme_style_dropdown,
                             i % calendar::kThemeCount);
    calendar::change_calendar_theme_style();
    if (i & 1U)
      lv_obj_add_state(objects.dark_theme_switch, LV_STATE_CHECKED);
    else
      lv_obj_remove_state(objects.dark_theme_switch, LV_STATE_CHECKED);
    calendar::toggle_calendar_dark_theme();
    calendar::show_calendar_main();
    calendar::show_calendar_primary_page();
    calendar::show_calendar_month_view();
    calendar::show_calendar_forecast();
    calendar::show_calendar_settings();
    calendar::show_calendar_brightness_settings();
    pump(3);
  }
  pump(20);
  lv_mem_monitor_t after{};
  lv_mem_monitor(&after);
  const unsigned objects_after = total_objects();
  check(objects_after == objects_before, "navigation object count stable",
        std::to_string(objects_before) + " -> " +
            std::to_string(objects_after));
  const std::size_t used_before = before.total_size - before.free_size,
                    used_after = after.total_size - after.free_size;
  check(used_after <= used_before + 2048U, "navigation LVGL memory stabilizes",
        std::to_string(used_before) + " -> " + std::to_string(used_after) +
            " bytes");
}
} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: calendar_ui_preview OUTPUT_DIR [theme-id] "
                 "[light|dark] [mixed|imperial]\n";
    return 2;
  }
  const std::string directory = argv[1];
  std::filesystem::create_directories(directory);
  checks.open(directory + "/checks.txt");
  performance.open(directory + "/performance.txt");
  setenv("TZ", "IST-2IDT,M3.4.4/26,M10.5.0", 1);
  tzset();
  std::tm local{};
  local.tm_year = 126;
  local.tm_mon = 8;
  local.tm_mday = 28;
  local.tm_hour = 9;
  local.tm_min = 41;
  local.tm_isdst = -1;
  preview::now = std::mktime(&local);
  const int requested_theme = argc > 2
                                  ? std::atoi(argv[2])
                                  : static_cast<int>(calendar::kDefaultThemeId);
  const bool requested_dark = argc > 3 && std::strcmp(argv[3], "dark") == 0;
  preview::appearance.theme_id =
      calendar::theme_id_from_raw(static_cast<std::uint8_t>(requested_theme));
  preview::appearance.dark_theme = requested_dark;
  preview::set_fixture(argc > 4 && std::strcmp(argv[4], "mixed") == 0,
                       argc > 4 && std::strcmp(argv[4], "imperial") == 0);
  lv_init();
  for (auto &pool : supplementary_pools) {
    pool = std::malloc(64 * 1024);
    if (!pool || !lv_mem_add_pool(pool, 64 * 1024)) {
      std::cerr << "Preview LVGL pool allocation failed\n";
      return 1;
    }
  }
  auto *display = lv_display_create(kWidth, kHeight);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display, draw_buffer.data(), nullptr,
                         sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);
  ui_init();
  calendar::initialize_calendar_ui();
  pump(40);
  verify_redraw_performance();
  capture_primary_screens(directory);
  verify_lazy_forms_and_capture(directory);
  verify_firmware_states_and_capture(directory);
  capture_pressed_states(directory);
  verify_pressed_contrast();
  verify_geometry();
  verify_settings_caption_alignment();
  verify_event_text_direction_and_agenda_viewport();
  verify_month_and_forecast_columns();
  verify_default_header_contrast_all_appearances(
      static_cast<unsigned>(requested_theme), requested_dark);
  verify_static_button_geometry_all_appearances(
      static_cast<unsigned>(requested_theme), requested_dark);
  verify_details_and_recovery(directory);
  capture_negative_weather(directory);
  capture_status_states(directory);
  stress_navigation_and_themes();
  lv_mem_monitor_t memory{};
  lv_mem_monitor(&memory);
  std::ofstream evidence(directory + "/runtime.txt");
  evidence << "LVGL " << LVGL_VERSION_MAJOR << '.' << LVGL_VERSION_MINOR << '.'
           << LVGL_VERSION_PATCH
           << "; RGB565; real EEZ/controller; desktop service/clock/storage "
              "fixtures; no hardware\ntheme_id="
           << requested_theme
           << "\nmode=" << (requested_dark ? "dark" : "light")
           << "\nobjects=" << total_objects()
           << "\npool_bytes=" << memory.total_size
           << "\nused_bytes=" << (memory.total_size - memory.free_size)
           << "\npeak_used_bytes=" << memory.max_used
           << "\nfailures=" << failures << '\n';
  checks << "SUMMARY failures=" << failures << '\n';
  std::cout << "LVGL " << LVGL_VERSION_MAJOR << '.' << LVGL_VERSION_MINOR << '.'
            << LVGL_VERSION_PATCH << " QA rendered; failures=" << failures
            << '\n';
  return failures ? 1 : 0;
}
