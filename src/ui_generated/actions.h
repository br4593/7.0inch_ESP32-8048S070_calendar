#ifndef EEZ_LVGL_UI_ACTIONS_H
#define EEZ_LVGL_UI_ACTIONS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_select_day_0(lv_event_t * e);
extern void action_select_day_1(lv_event_t * e);
extern void action_select_day_2(lv_event_t * e);
extern void action_select_day_3(lv_event_t * e);
extern void action_select_day_4(lv_event_t * e);
extern void action_select_day_5(lv_event_t * e);
extern void action_select_day_6(lv_event_t * e);
extern void action_show_agenda(lv_event_t * e);
extern void action_show_settings(lv_event_t * e);
extern void action_show_agenda_from_settings(lv_event_t * e);
extern void action_start_network_setup(lv_event_t * e);
extern void action_sync_calendar_now(lv_event_t * e);
extern void action_open_display_wifi_setup(lv_event_t * e);
extern void action_previous_period(lv_event_t * e);
extern void action_next_period(lv_event_t * e);
extern void action_go_today(lv_event_t * e);
extern void action_show_month_view(lv_event_t * e);
extern void action_show_week_view(lv_event_t * e);
extern void action_show_brightness_settings(lv_event_t * e);
extern void action_show_settings_from_brightness(lv_event_t * e);
extern void action_toggle_dark_theme(lv_event_t * e);
extern void action_change_theme_style(lv_event_t * e);
extern void action_preview_brightness(lv_event_t * e);
extern void action_commit_brightness(lv_event_t * e);
extern void action_show_weather(lv_event_t * e);
extern void action_show_settings_from_weather(lv_event_t * e);
extern void action_refresh_weather(lv_event_t * e);
extern void action_show_weather_location_editor(lv_event_t * e);
extern void action_show_firmware_update(lv_event_t * e);
extern void action_show_settings_from_firmware(lv_event_t * e);
extern void action_arm_firmware_update(lv_event_t * e);
extern void action_cancel_firmware_update(lv_event_t * e);
extern void action_reboot_firmware_update(lv_event_t * e);
extern void action_show_main(lv_event_t * e);
extern void action_show_calendar(lv_event_t * e);
extern void action_show_forecast(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif /* EEZ_LVGL_UI_ACTIONS_H */