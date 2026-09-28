#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN_SCREEN = 1,
    SCREEN_ID_AGENDA_SCREEN = 2,
    SCREEN_ID_EVENT_DETAILS_SCREEN = 3,
    SCREEN_ID_SETTINGS_SCREEN = 4,
    SCREEN_ID_MONTH_SCREEN = 5,
    SCREEN_ID_BRIGHTNESS_SETTINGS_SCREEN = 6,
    SCREEN_ID_WEATHER_SCREEN = 7,
    SCREEN_ID_FIRMWARE_UPDATE_SCREEN = 8,
    _SCREEN_ID_LAST = 8
};

typedef struct _objects_t {
    lv_obj_t *main_screen;
    lv_obj_t *agenda_screen;
    lv_obj_t *event_details_screen;
    lv_obj_t *settings_screen;
    lv_obj_t *month_screen;
    lv_obj_t *brightness_settings_screen;
    lv_obj_t *weather_screen;
    lv_obj_t *firmware_update_screen;
    lv_obj_t *main_header;
    lv_obj_t *main_time_label;
    lv_obj_t *main_date_label;
    lv_obj_t *main_sync_state_label;
    lv_obj_t *main_wifi_button;
    lv_obj_t *main_wifi_icon;
    lv_obj_t *main_wifi_label;
    lv_obj_t *main_settings_button;
    lv_obj_t *obj0;
    lv_obj_t *main_events_container;
    lv_obj_t *main_today_label;
    lv_obj_t *main_events_state_label;
    lv_obj_t *main_events_list;
    lv_obj_t *main_events_hint_label;
    lv_obj_t *main_weather_card;
    lv_obj_t *main_weather_location_label;
    lv_obj_t *main_weather_icon_container;
    lv_obj_t *main_weather_temperature_label;
    lv_obj_t *main_weather_condition_label;
    lv_obj_t *main_weather_detail_label;
    lv_obj_t *main_weather_feels_value_label;
    lv_obj_t *main_weather_humidity_label;
    lv_obj_t *main_weather_humidity_value_label;
    lv_obj_t *main_weather_wind_label;
    lv_obj_t *main_weather_wind_value_label;
    lv_obj_t *main_weather_updated_label;
    lv_obj_t *main_weather_forecast_label;
    lv_obj_t *main_primary_navigation;
    lv_obj_t *main_nav_main_button;
    lv_obj_t *obj1;
    lv_obj_t *main_nav_calendar_button;
    lv_obj_t *obj2;
    lv_obj_t *main_nav_forecast_button;
    lv_obj_t *obj3;
    lv_obj_t *main_nav_indicator;
    lv_obj_t *agenda_header;
    lv_obj_t *header_time_label;
    lv_obj_t *header_date_label;
    lv_obj_t *sync_state_label;
    lv_obj_t *agenda_wifi_button;
    lv_obj_t *agenda_wifi_icon;
    lv_obj_t *agenda_wifi_label;
    lv_obj_t *settings_button;
    lv_obj_t *obj4;
    lv_obj_t *agenda_navigation_bar;
    lv_obj_t *agenda_previous_button;
    lv_obj_t *obj5;
    lv_obj_t *agenda_next_button;
    lv_obj_t *obj6;
    lv_obj_t *agenda_today_button;
    lv_obj_t *obj7;
    lv_obj_t *agenda_period_label;
    lv_obj_t *agenda_week_button;
    lv_obj_t *obj8;
    lv_obj_t *agenda_month_button;
    lv_obj_t *obj9;
    lv_obj_t *day_selector;
    lv_obj_t *day_button_0;
    lv_obj_t *day_name_label_0;
    lv_obj_t *day_date_label_0;
    lv_obj_t *day_button_1;
    lv_obj_t *day_name_label_1;
    lv_obj_t *day_date_label_1;
    lv_obj_t *day_button_2;
    lv_obj_t *day_name_label_2;
    lv_obj_t *day_date_label_2;
    lv_obj_t *day_button_3;
    lv_obj_t *day_name_label_3;
    lv_obj_t *day_date_label_3;
    lv_obj_t *day_button_4;
    lv_obj_t *day_name_label_4;
    lv_obj_t *day_date_label_4;
    lv_obj_t *day_button_5;
    lv_obj_t *day_name_label_5;
    lv_obj_t *day_date_label_5;
    lv_obj_t *day_button_6;
    lv_obj_t *day_name_label_6;
    lv_obj_t *day_date_label_6;
    lv_obj_t *agenda_rows_container;
    lv_obj_t *agenda_state_label;
    lv_obj_t *agenda_footer;
    lv_obj_t *footer_label;
    lv_obj_t *agenda_primary_navigation;
    lv_obj_t *agenda_nav_main_button;
    lv_obj_t *obj10;
    lv_obj_t *agenda_nav_calendar_button;
    lv_obj_t *obj11;
    lv_obj_t *agenda_nav_forecast_button;
    lv_obj_t *obj12;
    lv_obj_t *agenda_nav_indicator;
    lv_obj_t *details_header;
    lv_obj_t *details_back_button;
    lv_obj_t *obj13;
    lv_obj_t *details_heading_label;
    lv_obj_t *details_content;
    lv_obj_t *details_title_label;
    lv_obj_t *details_time_label;
    lv_obj_t *details_location_label;
    lv_obj_t *details_calendar_label;
    lv_obj_t *settings_header;
    lv_obj_t *settings_back_button;
    lv_obj_t *obj14;
    lv_obj_t *settings_heading_label;
    lv_obj_t *settings_status_card;
    lv_obj_t *settings_wifi_caption_label;
    lv_obj_t *settings_wifi_status_label;
    lv_obj_t *settings_time_caption_label;
    lv_obj_t *settings_time_status_label;
    lv_obj_t *settings_feed_caption_label;
    lv_obj_t *settings_feed_status_label;
    lv_obj_t *settings_setup_hint_label;
    lv_obj_t *settings_enter_wifi_button;
    lv_obj_t *obj15;
    lv_obj_t *settings_start_setup_button;
    lv_obj_t *obj16;
    lv_obj_t *settings_sync_now_button;
    lv_obj_t *obj17;
    lv_obj_t *settings_brightness_button;
    lv_obj_t *obj18;
    lv_obj_t *settings_weather_button;
    lv_obj_t *obj19;
    lv_obj_t *settings_firmware_button;
    lv_obj_t *obj20;
    lv_obj_t *month_header;
    lv_obj_t *month_time_label;
    lv_obj_t *month_date_label;
    lv_obj_t *month_sync_state_label;
    lv_obj_t *month_wifi_button;
    lv_obj_t *month_wifi_icon;
    lv_obj_t *month_wifi_label;
    lv_obj_t *month_settings_button;
    lv_obj_t *obj21;
    lv_obj_t *month_navigation_bar;
    lv_obj_t *month_previous_button;
    lv_obj_t *obj22;
    lv_obj_t *month_next_button;
    lv_obj_t *obj23;
    lv_obj_t *month_today_button;
    lv_obj_t *obj24;
    lv_obj_t *month_period_label;
    lv_obj_t *month_state_label;
    lv_obj_t *month_week_button;
    lv_obj_t *obj25;
    lv_obj_t *month_month_button;
    lv_obj_t *obj26;
    lv_obj_t *month_weekday_header;
    lv_obj_t *month_weekday_label_0;
    lv_obj_t *month_weekday_label_1;
    lv_obj_t *month_weekday_label_2;
    lv_obj_t *month_weekday_label_3;
    lv_obj_t *month_weekday_label_4;
    lv_obj_t *month_weekday_label_5;
    lv_obj_t *month_weekday_label_6;
    lv_obj_t *month_grid_container;
    lv_obj_t *month_primary_navigation;
    lv_obj_t *month_nav_main_button;
    lv_obj_t *obj27;
    lv_obj_t *month_nav_calendar_button;
    lv_obj_t *obj28;
    lv_obj_t *month_nav_forecast_button;
    lv_obj_t *obj29;
    lv_obj_t *month_nav_indicator;
    lv_obj_t *brightness_header;
    lv_obj_t *brightness_back_button;
    lv_obj_t *obj30;
    lv_obj_t *appearance_heading_label;
    lv_obj_t *brightness_card;
    lv_obj_t *obj31;
    lv_obj_t *theme_preview_canvas;
    lv_obj_t *theme_preview_surface;
    lv_obj_t *theme_preview_accent;
    lv_obj_t *theme_style_dropdown;
    lv_obj_t *obj32;
    lv_obj_t *dark_theme_switch;
    lv_obj_t *obj33;
    lv_obj_t *brightness_value_label;
    lv_obj_t *brightness_slider;
    lv_obj_t *obj34;
    lv_obj_t *weather_header;
    lv_obj_t *weather_back_button;
    lv_obj_t *obj35;
    lv_obj_t *forecast_time_label;
    lv_obj_t *forecast_date_label;
    lv_obj_t *forecast_sync_state_label;
    lv_obj_t *forecast_wifi_button;
    lv_obj_t *forecast_wifi_icon;
    lv_obj_t *forecast_wifi_label;
    lv_obj_t *weather_location_label;
    lv_obj_t *weather_current_summary_card;
    lv_obj_t *weather_current_icon_container;
    lv_obj_t *weather_temperature_label;
    lv_obj_t *weather_condition_label;
    lv_obj_t *weather_feels_like_label;
    lv_obj_t *weather_humidity_label;
    lv_obj_t *weather_wind_label;
    lv_obj_t *weather_state_label;
    lv_obj_t *weather_updated_label;
    lv_obj_t *weather_attribution_label;
    lv_obj_t *forecast_cards_container;
    lv_obj_t *forecast_day_label_0;
    lv_obj_t *forecast_icon_container_0;
    lv_obj_t *forecast_temp_label_0;
    lv_obj_t *forecast_condition_label_0;
    lv_obj_t *forecast_day_label_1;
    lv_obj_t *forecast_icon_container_1;
    lv_obj_t *forecast_temp_label_1;
    lv_obj_t *forecast_condition_label_1;
    lv_obj_t *forecast_day_label_2;
    lv_obj_t *forecast_icon_container_2;
    lv_obj_t *forecast_temp_label_2;
    lv_obj_t *forecast_condition_label_2;
    lv_obj_t *forecast_day_label_3;
    lv_obj_t *forecast_icon_container_3;
    lv_obj_t *forecast_temp_label_3;
    lv_obj_t *forecast_condition_label_3;
    lv_obj_t *forecast_day_label_4;
    lv_obj_t *forecast_icon_container_4;
    lv_obj_t *forecast_temp_label_4;
    lv_obj_t *forecast_condition_label_4;
    lv_obj_t *forecast_day_label_5;
    lv_obj_t *forecast_icon_container_5;
    lv_obj_t *forecast_temp_label_5;
    lv_obj_t *forecast_condition_label_5;
    lv_obj_t *forecast_day_label_6;
    lv_obj_t *forecast_icon_container_6;
    lv_obj_t *forecast_temp_label_6;
    lv_obj_t *forecast_condition_label_6;
    lv_obj_t *forecast_primary_navigation;
    lv_obj_t *forecast_nav_main_button;
    lv_obj_t *obj36;
    lv_obj_t *forecast_nav_calendar_button;
    lv_obj_t *obj37;
    lv_obj_t *forecast_nav_forecast_button;
    lv_obj_t *obj38;
    lv_obj_t *forecast_nav_indicator;
    lv_obj_t *weather_set_location_button;
    lv_obj_t *obj39;
    lv_obj_t *weather_refresh_button;
    lv_obj_t *obj40;
    lv_obj_t *firmware_header;
    lv_obj_t *firmware_back_button;
    lv_obj_t *obj41;
    lv_obj_t *firmware_heading_label;
    lv_obj_t *obj42;
    lv_obj_t *firmware_version_label;
    lv_obj_t *firmware_state_label;
    lv_obj_t *firmware_instructions_label;
    lv_obj_t *firmware_progress_bar;
    lv_obj_t *firmware_enable_button;
    lv_obj_t *obj43;
    lv_obj_t *firmware_cancel_button;
    lv_obj_t *obj44;
    lv_obj_t *firmware_reboot_button;
    lv_obj_t *obj45;
} objects_t;

extern objects_t objects;

void create_screen_main_screen();
void tick_screen_main_screen();

void create_screen_agenda_screen();
void tick_screen_agenda_screen();

void create_screen_event_details_screen();
void tick_screen_event_details_screen();

void create_screen_settings_screen();
void tick_screen_settings_screen();

void create_screen_month_screen();
void tick_screen_month_screen();

void create_screen_brightness_settings_screen();
void tick_screen_brightness_settings_screen();

void create_screen_weather_screen();
void tick_screen_weather_screen();

void create_screen_firmware_update_screen();
void tick_screen_firmware_update_screen();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /* EEZ_LVGL_UI_SCREENS_H */