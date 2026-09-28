#include "app/ui_controller.hpp"

#include <lvgl.h>

extern "C" void action_select_day_0(lv_event_t* event) { (void)event; calendar::select_calendar_day(0); }
extern "C" void action_select_day_1(lv_event_t* event) { (void)event; calendar::select_calendar_day(1); }
extern "C" void action_select_day_2(lv_event_t* event) { (void)event; calendar::select_calendar_day(2); }
extern "C" void action_select_day_3(lv_event_t* event) { (void)event; calendar::select_calendar_day(3); }
extern "C" void action_select_day_4(lv_event_t* event) { (void)event; calendar::select_calendar_day(4); }
extern "C" void action_select_day_5(lv_event_t* event) { (void)event; calendar::select_calendar_day(5); }
extern "C" void action_select_day_6(lv_event_t* event) { (void)event; calendar::select_calendar_day(6); }
extern "C" void action_show_agenda(lv_event_t* event) { (void)event; calendar::show_calendar_agenda(); }
extern "C" void action_show_settings(lv_event_t* event) { (void)event; calendar::show_calendar_settings(); }
extern "C" void action_show_agenda_from_settings(lv_event_t* event) { (void)event; calendar::show_calendar_previous_primary_page(); }
extern "C" void action_start_network_setup(lv_event_t* event) { (void)event; calendar::begin_calendar_network_setup(); }
extern "C" void action_open_display_wifi_setup(lv_event_t* event) { (void)event; calendar::open_calendar_display_wifi_setup(); }
extern "C" void action_sync_calendar_now(lv_event_t* event) { (void)event; calendar::sync_calendar_now(); }
extern "C" void action_show_brightness_settings(lv_event_t* event) { (void)event; calendar::show_calendar_brightness_settings(); }
extern "C" void action_show_settings_from_brightness(lv_event_t* event) { (void)event; calendar::show_calendar_settings_from_brightness(); }
extern "C" void action_change_theme_style(lv_event_t* event) { (void)event; calendar::change_calendar_theme_style(); }
extern "C" void action_toggle_dark_theme(lv_event_t* event) { (void)event; calendar::toggle_calendar_dark_theme(); }
extern "C" void action_preview_brightness(lv_event_t* event) { (void)event; calendar::preview_calendar_brightness(); }
extern "C" void action_commit_brightness(lv_event_t* event) { (void)event; calendar::commit_calendar_brightness(); }
extern "C" void action_previous_period(lv_event_t* event) { (void)event; calendar::show_previous_calendar_period(); }
extern "C" void action_next_period(lv_event_t* event) { (void)event; calendar::show_next_calendar_period(); }
extern "C" void action_go_today(lv_event_t* event) { (void)event; calendar::show_calendar_today(); }
extern "C" void action_show_week_view(lv_event_t* event) { (void)event; calendar::show_calendar_week_view(); }
extern "C" void action_show_month_view(lv_event_t* event) { (void)event; calendar::show_calendar_month_view(); }
extern "C" void action_show_weather(lv_event_t* event) { (void)event; calendar::show_calendar_weather(); }
extern "C" void action_show_settings_from_weather(lv_event_t* event) { (void)event; calendar::show_calendar_settings_from_weather(); }
extern "C" void action_refresh_weather(lv_event_t* event) { (void)event; calendar::refresh_calendar_weather(); }
extern "C" void action_show_weather_location_editor(lv_event_t* event) { (void)event; calendar::open_calendar_weather_location_editor(); }
extern "C" void action_show_firmware_update(lv_event_t* event) { (void)event; calendar::show_calendar_firmware_update(); }
extern "C" void action_show_settings_from_firmware(lv_event_t* event) { (void)event; calendar::show_calendar_settings_from_firmware(); }
extern "C" void action_arm_firmware_update(lv_event_t* event) { (void)event; calendar::arm_calendar_firmware_update(); }
extern "C" void action_cancel_firmware_update(lv_event_t* event) { (void)event; calendar::cancel_calendar_firmware_update(); }
extern "C" void action_reboot_firmware_update(lv_event_t* event) { (void)event; calendar::reboot_calendar_firmware_update(); }
extern "C" void action_show_main(lv_event_t* event) { (void)event; calendar::show_calendar_main(); }
extern "C" void action_show_calendar(lv_event_t* event) { (void)event; calendar::show_calendar_primary_page(); }
extern "C" void action_show_forecast(lv_event_t* event) { (void)event; calendar::show_calendar_forecast(); }
