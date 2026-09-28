#pragma once

#include "app/calendar_provider.hpp"

#include <cstdint>

namespace calendar {

// Call once, immediately after generated ui_init(), from the Arduino/LVGL loop
// context. All remaining functions must run on that same context.
void initialize_calendar_ui();

// Called from the normal Arduino/LVGL loop after the connectivity service.
// It imports a completed feed and refreshes the safe, on-screen connection
// status; it never performs network I/O itself.
void tick_calendar_ui();

// Invoked by generated EEZ native actions. Day offsets are stable positions in
// the currently visible Sunday-through-Saturday strip.
void select_calendar_day(std::uint8_t day_offset);
void show_calendar_main();
void show_calendar_primary_page();
void show_calendar_forecast();
void show_calendar_previous_primary_page();
void show_calendar_agenda();
void show_calendar_settings();
void show_previous_calendar_period();
void show_next_calendar_period();
void show_calendar_today();
void show_calendar_week_view();
void show_calendar_month_view();
void begin_calendar_network_setup();
// Opens the display-side Wi-Fi form. The SSID and password are only handed to
// the board service from tick_calendar_ui(), never from an LVGL callback.
void open_calendar_display_wifi_setup();
void sync_calendar_now();

// Appearance is manual-only. These actions only touch LVGL from its owning
// context; persistence is deferred to tick_calendar_ui().
void show_calendar_brightness_settings();
void show_calendar_settings_from_brightness();
void change_calendar_theme_style();
void toggle_calendar_dark_theme();
void preview_calendar_brightness();
void commit_calendar_brightness();

void show_calendar_weather();
void show_calendar_settings_from_weather();
void refresh_calendar_weather();
void open_calendar_weather_location_editor();
void show_calendar_firmware_update();
void show_calendar_settings_from_firmware();
void arm_calendar_firmware_update();
void cancel_calendar_firmware_update();
void reboot_calendar_firmware_update();

// QA-only controls for the Milestone 1 mock provider. They must be called from
// the same Arduino/LVGL loop context as the normal controller entry points.
// They neither perform I/O nor alter the Ready state used at ordinary boot.
void set_calendar_provider_state_for_debug(ProviderState state);
void refresh_without_selected_event_for_debug();

}  // namespace calendar
