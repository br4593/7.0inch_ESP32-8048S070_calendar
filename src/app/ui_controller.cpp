#include "app/ui_controller.hpp"

#include "app/calendar_count_format.hpp"
#include "app/calendar_navigation.hpp"
#include "app/appearance_preferences.hpp"
#include "app/ical_feed.hpp"
#include "app/firmware_version.hpp"
#include "app/theme_palette.hpp"
#include "app/ui_status.hpp"
#include "app/weather_icon.hpp"
#include "app/weather_types.hpp"
#include "board/runtime.h"
#include "board/connectivity_service.hpp"
#include "board/ota_service.hpp"
#include "board/weather_service.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <lvgl.h>
#include <Arduino.h>

extern "C" {
#include "ui_generated/screens.h"
#include "ui_generated/ui.h"
}

namespace calendar {
namespace {

void log_controller_stage(const char* stage) {
    Serial.printf("[boot] controller: %s; heap=%u psram=%u\n", stage,
                  static_cast<unsigned>(ESP.getFreeHeap()),
                  static_cast<unsigned>(ESP.getFreePsram()));
    Serial.flush();
    delay(1);
}

constexpr std::size_t kDayButtonCount = 7;
constexpr std::size_t kMonthCellCount = 42;
constexpr std::size_t kMaxMainRows = 4;
constexpr std::size_t kParseBytesPerTick = 16 * 1024;
constexpr std::size_t kParseBytesPerStep = 512;
constexpr std::uint32_t kParseTimeBudgetUs = 1500;
constexpr std::uint32_t kLiveDateRefreshIntervalMs = 1000;
constexpr std::uint32_t kSettingsRefreshIntervalMs = 250;
constexpr std::uint32_t kFirmwareRefreshIntervalMs = 250;
constexpr std::uint32_t kStatusRefreshIntervalMs = 250;
constexpr int kMockDaySeconds = 24 * 60 * 60;
constexpr std::int64_t kMockWindowStartUtc = 1835308800;  // 28/02/2028 00:00 UTC.

struct RowBinding {
    char event_id[kMaxEventIdBytes + 1]{};
};

struct FirmwareRenderSignature {
    board::OtaState state = board::OtaState::Disabled;
    int32_t progress = 0;
    bool can_arm = false;
    bool can_cancel = false;
    bool can_reboot = false;
    ThemeId theme_id = kDefaultThemeId;
    bool dark_theme = false;
    char instructions[192]{};

    bool matches(const FirmwareRenderSignature& other) const {
        return state == other.state && progress == other.progress &&
               can_arm == other.can_arm && can_cancel == other.can_cancel &&
               can_reboot == other.can_reboot && theme_id == other.theme_id &&
               dark_theme == other.dark_theme &&
               std::strcmp(instructions, other.instructions) == 0;
    }
};

enum class PrimaryPage : std::uint8_t { Main, Calendar, Forecast };

// Calendar feeds commonly use very light Google-style colors. Mix them toward
// the interface navy before drawing them on a light agenda card so the marker
// remains visible while still retaining its calendar-specific hue.
std::uint32_t agenda_indicator_color(std::uint32_t calendar_color,
                                     const ThemePalette& palette) {
    constexpr std::uint32_t kSourceWeight = 30;
    constexpr std::uint32_t kNavyWeight = 70;
    const auto mix_channel = [](std::uint32_t source, std::uint32_t navy) {
        return (source * kSourceWeight + navy * kNavyWeight) / 100;
    };
    const std::uint32_t red = mix_channel((calendar_color >> 16) & 0xFF,
                                          (palette.text_primary >> 16) & 0xFF);
    const std::uint32_t green = mix_channel((calendar_color >> 8) & 0xFF,
                                            (palette.text_primary >> 8) & 0xFF);
    const std::uint32_t blue = mix_channel(calendar_color & 0xFF,
                                           palette.text_primary & 0xFF);
    return (red << 16) | (green << 8) | blue;
}

const char* weekday_name(CivilDate day) {
    static constexpr std::array<const char*, kDayButtonCount> kNames{
        "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    std::tm local{};
    local.tm_year = day.year - 1900;
    local.tm_mon = static_cast<int>(day.month) - 1;
    local.tm_mday = static_cast<int>(day.day);
    local.tm_isdst = -1;
    if (std::mktime(&local) == static_cast<std::time_t>(-1)) return "DAY";
    return kNames[static_cast<std::size_t>(local.tm_wday)];
}

// The seven forecast cards have only 100 px for a condition. Use the
// OpenWeather condition ID for a short, stable first line; the second line
// keeps the precipitation chance visible without wrapping into a third line.
const char* compact_forecast_condition(int condition_id) {
    if (condition_id >= 200 && condition_id < 300) return "Storms";
    if (condition_id >= 300 && condition_id < 400) return "Drizzle";
    if (condition_id >= 500 && condition_id < 600) return "Rain";
    if (condition_id >= 600 && condition_id < 700) return "Snow";
    switch (condition_id) {
        case 701: return "Mist";
        case 711: return "Smoke";
        case 721: return "Haze";
        case 731:
        case 751:
        case 761: return "Dust";
        case 741: return "Fog";
        case 762: return "Ash";
        case 771: return "Squalls";
        case 781: return "Tornado";
        case 800: return "Clear";
        case 801: return "Few clouds";
        case 802: return "Clouds";
        case 803: return "Cloudy";
        case 804: return "Overcast";
        default: return "Weather";
    }
}

std::string date_text(CivilDate date) {
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%02u/%02u/%04d", date.day, date.month, date.year);
    return buffer;
}

std::string tenths_text(std::int32_t value) {
    const int magnitude = std::abs(static_cast<int>(value));
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%s%d.%d", value < 0 ? "-" : "",
                  magnitude / 10, magnitude % 10);
    return buffer;
}

std::int32_t divide_rounded(std::int32_t numerator, std::int32_t denominator) {
    return numerator >= 0 ? (numerator + denominator / 2) / denominator
                          : (numerator - denominator / 2) / denominator;
}

std::int32_t display_temperature_tenths(std::int16_t tenths_c,
                                        board::WeatherUnits units) {
    if (units == board::WeatherUnits::Imperial) {
        return divide_rounded(static_cast<std::int32_t>(tenths_c) * 9, 5) + 320;
    }
    return tenths_c;
}

std::int32_t display_wind_tenths(std::int16_t tenths_mps, board::WeatherUnits units) {
    const std::int32_t canonical = std::max<std::int32_t>(0, tenths_mps);
    if (units == board::WeatherUnits::Imperial) {
        // tenths of m/s -> tenths of mph (1 m/s = 2.23694 mph).
        return divide_rounded(canonical * 2237, 1000);
    }
    // tenths of m/s -> tenths of km/h (1 m/s = 3.6 km/h).
    return divide_rounded(canonical * 36, 10);
}

const char* temperature_unit(board::WeatherUnits units) {
    return units == board::WeatherUnits::Imperial ? "F" : "C";
}

const char* wind_unit(board::WeatherUnits units) {
    return units == board::WeatherUnits::Imperial ? "mph" : "km/h";
}

const char* month_name(unsigned month) {
    static constexpr std::array<const char*, 12> kNames{
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"};
    return month >= 1 && month <= kNames.size() ? kNames[month - 1] : "Month";
}

std::string week_period_text(CalendarPeriod period) {
    const CivilDate inclusive_end = add_days(period.end_exclusive, -1);
    return date_text(period.start) + " - " + date_text(inclusive_end);
}

std::string month_period_text(CivilDate anchor) {
    return std::string(month_name(anchor.month)) + " " + std::to_string(anchor.year);
}

bool local_time(std::int64_t utc, std::tm& result) {
    const std::time_t value = static_cast<std::time_t>(utc);
    return localtime_r(&value, &result) != nullptr;
}

CivilDate date_from_tm(const std::tm& value) {
    return {value.tm_year + 1900, static_cast<unsigned>(value.tm_mon + 1),
            static_cast<unsigned>(value.tm_mday)};
}

std::string local_time_text(std::int64_t utc) {
    std::tm value{};
    if (!local_time(utc, value)) return "--:--";
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", value.tm_hour, value.tm_min);
    return buffer;
}

DayWindow local_day_window(CivilDate date) {
    std::tm start{};
    start.tm_year = date.year - 1900;
    start.tm_mon = static_cast<int>(date.month) - 1;
    start.tm_mday = static_cast<int>(date.day);
    start.tm_isdst = -1;
    const CivilDate next_date = add_days(date, 1);
    std::tm end{};
    end.tm_year = next_date.year - 1900;
    end.tm_mon = static_cast<int>(next_date.month) - 1;
    end.tm_mday = static_cast<int>(next_date.day);
    end.tm_isdst = -1;
    return {date, static_cast<std::int64_t>(std::mktime(&start)),
            static_cast<std::int64_t>(std::mktime(&end))};
}

DayWindow mock_day_window(CivilDate date) {
    // Controller only exposes 28/02 through 05/03/2028. This deliberately
    // models mock UTC days, rather than production timezone conversion.
    const int offset = date.month == 2 ? static_cast<int>(date.day) - 28
                                       : static_cast<int>(date.day) + 1;
    const std::int64_t start = kMockWindowStartUtc + static_cast<std::int64_t>(offset) * kMockDaySeconds;
    return {date, start, start + kMockDaySeconds};
}

std::string event_time_text(const CalendarEvent& event) {
    if (event.time_kind == TimeKind::Timed) {
        std::tm start_tm{};
        std::tm end_tm{};
        if (!local_time(event.start_utc, start_tm) || !local_time(event.end_utc, end_tm)) {
            return "Time unavailable";
        }
        const std::string start = date_text(date_from_tm(start_tm)) + " " + local_time_text(event.start_utc);
        const std::string end = date_text(date_from_tm(end_tm)) + " " + local_time_text(event.end_utc);
        return start + " - " + end;
    }
    const CivilDate inclusive_end = add_days(event.end_date_exclusive, -1);
    const std::string start = date_text(event.start_date);
    return event.start_date == inclusive_end ? "All day: " + start
                                             : "All day: " + start + " - " + date_text(inclusive_end);
}

std::string agenda_time_text(const CalendarEvent& event) {
    return event.time_kind == TimeKind::Timed ? local_time_text(event.start_utc) + " - " + local_time_text(event.end_utc)
                                              : "All day";
}

class CalendarUiController {
public:
    CalendarUiController()
        : clock_({2028, 2, 28}),
          provider_(CalendarEventList{}, ProviderState::Loading),
          navigation_(clock_) {}

    const ThemePalette& palette() const {
        return palette_for(appearance_.theme_id, appearance_.dark_theme);
    }

    const ThemeStyleRecipe& recipe() const {
        return theme_style_recipe(appearance_.theme_id);
    }

    const ThemeMetrics& metrics() const {
        return recipe();
    }

    void initialize() {
        log_controller_stage("initialize entered");
        initialized_ = true;
        appearance_ = load_appearance_preferences();
        persisted_appearance_ = appearance_;
        board_runtime::setBacklightPercent(appearance_.brightness_percent);
        log_controller_stage("appearance loaded");
        // EEZ 0.28 accepts the built-in font token but its fallback export does
        // not emit the setter. Apply it to the screen roots for inherited
        // static and dynamic text, then reinforce detail labels below.
        set_dynamic_font(objects.main_screen);
        set_dynamic_font(objects.agenda_screen);
        set_dynamic_font(objects.event_details_screen);
        set_dynamic_font(objects.settings_screen);
        set_dynamic_font(objects.month_screen);
        set_dynamic_font(objects.brightness_settings_screen);
        set_dynamic_font(objects.weather_screen);
        set_dynamic_font(objects.firmware_update_screen);
        set_dynamic_font(objects.settings_wifi_status_label);
        set_dynamic_font(objects.settings_time_status_label);
        set_dynamic_font(objects.settings_feed_status_label);
        set_dynamic_font(objects.settings_setup_hint_label);
        set_dynamic_font(objects.brightness_value_label);
        for (lv_obj_t* heading : {objects.details_heading_label, objects.settings_heading_label,
                                  objects.appearance_heading_label, objects.firmware_heading_label}) {
            set_emphasis_font(heading);
        }
        set_large_time_font(objects.main_time_label);
        set_large_time_font(objects.forecast_time_label);
        set_emphasis_font(objects.forecast_date_label);
        set_large_time_font(objects.header_time_label);
        set_large_time_font(objects.month_time_label);
        set_large_temperature_font(objects.main_weather_temperature_label);
        set_large_temperature_font(objects.weather_temperature_label);
        for (lv_obj_t* label : {objects.month_weekday_label_0, objects.month_weekday_label_1,
                                objects.month_weekday_label_2, objects.month_weekday_label_3,
                                objects.month_weekday_label_4, objects.month_weekday_label_5,
                                objects.month_weekday_label_6}) {
            set_compact_ascii_font(label);
        }
        if (objects.theme_style_dropdown != nullptr) {
            lv_obj_add_event_cb(objects.theme_style_dropdown, on_dropdown_opened,
                                LV_EVENT_READY, nullptr);
        }
        // Dynamic human-language content may be English, Hebrew, or mixed.
        // Let LVGL resolve alignment from the text while keeping numeric
        // dates and times in their deliberately fixed positions.
        for (lv_obj_t* label : {objects.weather_condition_label,
                                objects.details_title_label,
                                objects.details_location_label}) {
            if (label != nullptr) {
                lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_AUTO,
                                            LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }
        // EEZ exports these placeholders with lv_label_set_text_static().
        // LV_LABEL_LONG_DOT edits the label text in place when truncating, so
        // give each label an owned copy before enabling that mode.
        for (lv_obj_t* label : {objects.main_weather_location_label,
                                objects.weather_location_label, objects.weather_state_label}) {
            if (label != nullptr) {
                lv_label_set_text(label, lv_label_get_text(label));
                lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
            }
        }
        lv_obj_remove_flag(objects.main_events_list, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(objects.main_events_list, LV_OBJ_FLAG_SCROLL_ELASTIC);
        lv_obj_remove_flag(objects.main_events_list, LV_OBJ_FLAG_SCROLL_MOMENTUM);
        lv_obj_set_scrollbar_mode(objects.main_events_list, LV_SCROLLBAR_MODE_OFF);
        log_controller_stage("static widgets prepared");
        attach_weather_icons();
        log_controller_stage("weather image icons attached");
        apply_appearance_to_widgets();
        log_controller_stage("appearance applied");
        apply_provider();
        primary_page_ = PrimaryPage::Main;
        render_main();
        log_controller_stage("main rendered");
    }

    void show_main() {
        if (!initialized_) return;
        close_transient_forms();
        primary_page_ = PrimaryPage::Main;
        if (has_local_time()) {
            const CalendarPeriod before = navigation_.visible_period();
            navigation_.show_today();
            if (navigation_.visible_period() != before && !cached_calendar_document_.empty()) {
                queue_period_parse(false);
            }
        }
        loadScreen(SCREEN_ID_MAIN_SCREEN);
        apply_primary_navigation_styles();
        render_main();
    }

    void show_calendar() {
        if (!initialized_) return;
        close_transient_forms();
        primary_page_ = PrimaryPage::Calendar;
        apply_primary_navigation_styles();
        load_current_calendar_screen();
        render_current_calendar_view(true);
    }

    void show_forecast() {
        if (!initialized_) return;
        close_transient_forms();
        primary_page_ = PrimaryPage::Forecast;
        loadScreen(SCREEN_ID_WEATHER_SCREEN);
        apply_primary_navigation_styles();
        render_weather();
    }

    void show_previous_primary() {
        switch (settings_origin_) {
            case PrimaryPage::Main: show_main(); break;
            case PrimaryPage::Calendar: show_calendar(); break;
            case PrimaryPage::Forecast: show_forecast(); break;
        }
    }

    void select_day(std::uint8_t offset) {
        if (!initialized_ || !has_local_time() || offset >= kDayButtonCount) return;
        navigation_.select_day(add_days(navigation_.visible_period().start, offset));
        // The current week snapshot already contains all seven days. Changing
        // the selected day is a pure filter/render operation and must not scan
        // the complete PSRAM document again.
        render_agenda();
    }

    void show_agenda() {
        if (!initialized_) return;
        hide_display_wifi_form();
        destroy_display_wifi_form();
        navigation_.back();
        if (details_origin_ == PrimaryPage::Main) show_main();
        else show_calendar();
    }

    void show_settings() {
        if (!initialized_) return;
        settings_origin_ = primary_page_;
        // The month table is cheap to recreate and should not compete with the
        // keyboard/textareas for LVGL's fixed heap while Wi-Fi is configured.
        if (month_table_ != nullptr) {
            lv_obj_delete(month_table_);
            month_table_ = nullptr;
        }
        loadScreen(SCREEN_ID_SETTINGS_SCREEN);
        render_settings();
    }

    void show_brightness_settings() {
        if (!initialized_) return;
        hide_display_wifi_form();
        destroy_display_wifi_form();
        loadScreen(SCREEN_ID_BRIGHTNESS_SETTINGS_SCREEN);
        render_appearance_settings();
    }

    void show_settings_from_brightness() {
        if (!initialized_) return;
        loadScreen(SCREEN_ID_SETTINGS_SCREEN);
        render_settings();
    }

    void show_weather() {
        show_forecast();
    }

    void show_settings_from_weather() {
        if (!initialized_) return;
        settings_origin_ = PrimaryPage::Forecast;
        hide_weather_location_form();
        destroy_weather_location_form();
        loadScreen(SCREEN_ID_SETTINGS_SCREEN);
        render_settings();
    }

    void refresh_weather() {
        if (!initialized_) return;
        board::weather_service().request_refresh();
        render_weather();
    }

    void open_weather_location_editor() {
        if (!initialized_) return;
        if (weather_location_form_ == nullptr) {
            lv_mem_monitor_t memory{};
            lv_mem_monitor(&memory);
            if (memory.free_size < 12 * 1024 || memory.free_biggest_size < 6 * 1024) {
                set_label_text_if_changed(objects.weather_state_label,
                                          "Location keyboard unavailable: LVGL memory is low");
                return;
            }
            create_weather_location_form();
        }
        if (weather_location_form_ == nullptr) return;
        const board::WeatherLocationConfiguration config =
            board::weather_service().location_configuration();
        lv_textarea_set_text(weather_location_name_, config.location);
        lv_textarea_set_text(weather_location_latitude_, config.latitude);
        lv_textarea_set_text(weather_location_longitude_, config.longitude);
        lv_dropdown_set_selected(weather_location_units_,
                                 config.units == board::WeatherUnits::Imperial ? 1 : 0);
        lv_label_set_text(weather_location_feedback_,
                          config.api_key_configured
                              ? "Enter a label and decimal coordinates, then tap Save."
                              : "Add the OpenWeather API key from the home-LAN page first.");
        lv_obj_add_flag(objects.weather_refresh_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.weather_set_location_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(weather_location_form_, LV_OBJ_FLAG_HIDDEN);
        weather_location_form_visible_ = true;
        select_weather_location_field(weather_location_name_);
    }

    void show_firmware_update() {
        if (!initialized_) return;
        hide_display_wifi_form();
        destroy_display_wifi_form();
        loadScreen(SCREEN_ID_FIRMWARE_UPDATE_SCREEN);
        render_firmware_update();
    }

    void show_settings_from_firmware() {
        if (!initialized_) return;
        loadScreen(SCREEN_ID_SETTINGS_SCREEN);
        render_settings();
    }

    void arm_firmware_update() {
        if (!initialized_) return;
        board::ota_service().arm();
        render_firmware_update();
    }

    void cancel_firmware_update() {
        if (!initialized_) return;
        board::ota_service().cancel();
        render_firmware_update();
    }

    void reboot_firmware_update() {
        if (!initialized_) return;
        board::ota_service().request_reboot();
        render_firmware_update();
    }

    void toggle_dark_theme() {
        if (!initialized_) return;
        appearance_.dark_theme = lv_obj_has_state(objects.dark_theme_switch, LV_STATE_CHECKED);
        appearance_save_pending_ = true;
        apply_appearance_to_widgets();
        render_current_calendar_view(true);
        if (lv_screen_active() == objects.settings_screen) render_settings();
    }

    void change_theme_style() {
        if (!initialized_ || objects.theme_style_dropdown == nullptr) return;
        const ThemeId selected = theme_id_from_raw(
            static_cast<std::uint8_t>(lv_dropdown_get_selected(objects.theme_style_dropdown)));
        if (selected == appearance_.theme_id) return;
        appearance_.theme_id = selected;
        appearance_save_pending_ = true;
        apply_appearance_to_widgets();
        render_current_calendar_view(true);
        if (lv_screen_active() == objects.settings_screen) render_settings();
    }

    void preview_brightness() {
        if (!initialized_) return;
        const int32_t value = lv_slider_get_value(objects.brightness_slider);
        const std::uint8_t brightness = clamp_brightness_percent(
            static_cast<std::uint8_t>(std::max(0, std::min(100, static_cast<int>(value)))));
        if (brightness == appearance_.brightness_percent) return;
        appearance_.brightness_percent = brightness;
        board_runtime::setBacklightPercent(brightness);
        update_brightness_value_label();
    }

    void commit_brightness() {
        if (!initialized_) return;
        // Bound to release rather than VALUE_CHANGED. tick() owns the actual
        // NVS write, so dragging never writes flash repeatedly.
        appearance_save_pending_ = true;
    }

    void previous_period() {
        if (!initialized_ || !has_local_time()) return;
        navigation_.previous_period();
        queue_period_parse();
    }

    void next_period() {
        if (!initialized_ || !has_local_time()) return;
        navigation_.next_period();
        queue_period_parse();
    }

    void show_today() {
        if (!initialized_ || !has_local_time()) return;
        const CalendarPeriod before_period = navigation_.visible_period();
        navigation_.show_today();
        if (navigation_.visible_period() == before_period) {
            render_current_calendar_view();
            return;
        }
        queue_period_parse();
    }

    void show_week_view() {
        if (!initialized_) return;
        if (navigation_.view_mode() == CalendarViewMode::Week) return;
        navigation_.show_week_view();
        queue_period_parse();
    }

    void show_month_view() {
        if (!initialized_) return;
        if (navigation_.view_mode() == CalendarViewMode::Month) return;
        navigation_.show_month_view();
        queue_period_parse();
    }

    void begin_network_setup() {
        if (!initialized_) return;
        board::connectivity_service().begin_setup();
        render_settings();
    }

    void open_display_wifi_setup() {
        if (!initialized_) return;
        if (display_wifi_form_ == nullptr) {
            lv_mem_monitor_t memory{};
            lv_mem_monitor(&memory);
            if (memory.free_size < 12 * 1024 || memory.free_biggest_size < 6 * 1024) {
                lv_label_set_text(objects.settings_setup_hint_label,
                                  "Wi-Fi keyboard unavailable: LVGL memory is low");
                return;
            }
            create_display_wifi_form();
        }
        if (display_wifi_form_ == nullptr) return;
        clear_display_wifi_form();
        set_settings_shell_visible(false);
        lv_obj_remove_flag(display_wifi_form_, LV_OBJ_FLAG_HIDDEN);
        display_wifi_form_visible_ = true;
        refresh_display_wifi_scan_results();
        select_display_wifi_field(display_wifi_ssid_);
    }

    void sync_now() {
        if (!initialized_) return;
        const board::ConnectivityStatus status = board::connectivity_service().status();
        if (status.wifi_connected && status.configured_feed_count == 0) {
            calendar_setup_help_visible_ = true;
            render_settings();
            return;
        }
        if (!board::connectivity_service().request_sync()) {
            render_settings();
        }
    }

    void tick() {
        if (!initialized_) return;
        commit_pending_appearance();
        commit_pending_display_wifi_scan();
        commit_pending_display_wifi();
        commit_pending_weather_location();
        update_live_date();
        update_status_indicators();
        request_initial_sync();
        collect_downloaded_feed();
        start_pending_period_parse();
        advance_period_parse();
        if (display_wifi_form_visible_) refresh_display_wifi_scan_results();
        const std::uint32_t now_ms = millis();
        if (lv_screen_active() == objects.settings_screen && !display_wifi_form_visible_ &&
            (!settings_render_time_valid_ ||
             static_cast<std::uint32_t>(now_ms - last_settings_render_ms_) >=
                 kSettingsRefreshIntervalMs)) {
            render_settings();
        }
        const board::WeatherServiceStatus weather_status = board::weather_service().status();
        const bool weather_changed = weather_status_changed(weather_status);
        if (lv_screen_active() == objects.weather_screen && !weather_location_form_visible_ &&
            weather_changed) {
            render_weather();
        }
        const std::time_t now = std::time(nullptr);
        const ProviderResult provider = provider_.current();
        const bool provider_changed = !main_render_signature_valid_ ||
            provider.state != last_main_provider_state_ ||
            provider.snapshot.get() != last_main_snapshot_;
        const std::time_t minute = now >= 0 ? now / 60 : 0;
        if (lv_screen_active() == objects.main_screen &&
            (weather_changed || provider_changed || minute != last_main_render_minute_)) {
            render_main();
        }
        if (lv_screen_active() == objects.firmware_update_screen &&
            (!firmware_render_time_valid_ ||
             static_cast<std::uint32_t>(now_ms - last_firmware_render_ms_) >=
                 kFirmwareRefreshIntervalMs)) {
            render_firmware_update();
        }
    }

    void set_provider_state_for_debug(ProviderState state) {
        if (!initialized_) return;
        provider_.set_state(state);
        apply_provider();
        if (navigation_.screen() == NavigationScreen::Agenda) {
            render_agenda(true);
        }
    }

    void refresh_without_selected_event_for_debug() {
        if (!initialized_) return;
        const ProviderResult before_refresh = provider_.current();
        if (!before_refresh.snapshot || !navigation_.selected_event_id()) return;
        CalendarEventList refreshed_events(before_refresh.snapshot->events().begin(),
                                           before_refresh.snapshot->events().end());
        const std::string selected_id = *navigation_.selected_event_id();
        refreshed_events.erase(std::remove_if(refreshed_events.begin(), refreshed_events.end(),
                                              [&selected_id](const CalendarEvent& event) {
                                                  return std::string_view(event.id.data(), event.id.size()) == selected_id;
                                              }),
                               refreshed_events.end());
        provider_.replace(std::move(refreshed_events), before_refresh.state);
        apply_provider();
        details_notice_pending_ = true;
        if (details_origin_ == PrimaryPage::Main) show_main();
        else {
            load_current_calendar_screen();
            render_current_calendar_view(true);
        }
    }

private:
    static bool same_weather_status(const board::WeatherServiceStatus& lhs,
                                    const board::WeatherServiceStatus& rhs) {
        return lhs.state == rhs.state && lhs.configured == rhs.configured &&
               lhs.snapshot_available == rhs.snapshot_available &&
               lhs.five_day_fallback == rhs.five_day_fallback &&
               lhs.refresh_pending == rhs.refresh_pending &&
               lhs.units == rhs.units &&
               lhs.generation == rhs.generation && lhs.updated_utc == rhs.updated_utc &&
               std::strcmp(lhs.location, rhs.location) == 0 &&
               std::strcmp(lhs.detail, rhs.detail) == 0;
    }

    bool weather_status_changed(const board::WeatherServiceStatus& status) {
        const bool changed = !weather_render_signature_valid_ ||
                             !same_weather_status(status, last_weather_status_);
        if (changed) {
            last_weather_status_ = status;
            weather_render_signature_valid_ = true;
        }
        return changed;
    }

    void attach_weather_icons() {
        weather_icon::attach(main_weather_icon_, objects.main_weather_icon_container, true);
        weather_icon::attach(current_weather_icon_, objects.weather_current_icon_container, true);
        const std::array<lv_obj_t*, weather::kMaxForecastDays> containers{
            objects.forecast_icon_container_0, objects.forecast_icon_container_1,
            objects.forecast_icon_container_2, objects.forecast_icon_container_3,
            objects.forecast_icon_container_4, objects.forecast_icon_container_5,
            objects.forecast_icon_container_6};
        for (std::size_t index = 0; index < containers.size(); ++index) {
            weather_icon::attach(forecast_weather_icons_[index], containers[index], false);
        }
    }

    void apply_provider() { navigation_.apply_provider_result(provider_.current()); }

    bool has_local_time() const {
        // NTP establishes the system clock, which keeps running if Wi-Fi later
        // drops. Keep cached-calendar browsing and midnight rollover working
        // offline instead of tying valid local time to current connectivity.
        return std::time(nullptr) >= static_cast<std::time_t>(1700000000);
    }

    DayWindow selected_day_window() const {
        const CivilDate day = navigation_.selected_day();
        return has_local_time() ? local_day_window(day) : mock_day_window(day);
    }

    IcalFeedOptions feed_options_for(CalendarPeriod period) const {
        // A downloaded document is only made available after Jerusalem time
        // synchronization, so mktime supplies the correct 23/24/25-hour local
        // boundaries at DST transitions.
        const DayWindow start = local_day_window(period.start);
        const DayWindow end = local_day_window(period.end_exclusive);
        IcalFeedOptions options;
        options.selection_window = IcalSelectionWindow{
            period.start, period.end_exclusive, start.start_utc, end.start_utc};
        IcalSummaryBuckets buckets;
        buckets.count = navigation_.view_mode() == CalendarViewMode::Month
            ? kMonthCellCount : kDayButtonCount;
        for (std::size_t index = 0; index < buckets.count; ++index) {
            buckets.windows[index] = local_day_window(add_days(period.start, static_cast<int>(index)));
        }
        options.summary_buckets = std::move(buckets);
        return options;
    }

    bool apply_calendar_report(IcalParseReport parsed, bool downloaded_document,
                               std::uint32_t configuration_generation) {
        const std::size_t imported = parsed.events.size();
        const std::size_t skipped = parsed.skipped_malformed + parsed.skipped_unsupported_timezone +
                                    parsed.skipped_recurrence + parsed.skipped_duplicate +
                                    parsed.skipped_outside_window + parsed.skipped_capacity;
        if (parsed.input_too_large) {
            if (downloaded_document) {
                board::connectivity_service().report_ical_import_result(
                    configuration_generation, false, imported, skipped);
            }
            provider_.set_state(ProviderState::Stale);
            apply_provider();
            return true;
        }

        const std::size_t expected_summary_count = navigation_.view_mode() == CalendarViewMode::Month
            ? kMonthCellCount : kDayButtonCount;
        const bool next_summary_valid = parsed.summary_count == expected_summary_count;
        const auto next_summary_counts = parsed.summary_counts;
        const std::size_t next_matched_count = parsed.matched_event_count;
        const bool next_truncated = parsed.events_truncated;
        const ProviderResult current = provider_.current();
        const bool changed = !current.snapshot || current.state != ProviderState::Ready ||
                             !snapshot_matches_events(*current.snapshot, parsed.events);
        if (changed) {
            provider_.replace(std::move(parsed.events), ProviderState::Ready);
        }

        // Publish counts only after the potentially throwing snapshot commit.
        // This prevents a new period summary from appearing over old retained
        // events if PSRAM allocation fails.
        period_summary_counts_ = next_summary_counts;
        period_summary_valid_ = next_summary_valid;
        last_parse_matched_count_ = next_matched_count;
        last_parse_truncated_ = next_truncated;
        if (changed) apply_provider();
        if (downloaded_document) {
            board::connectivity_service().report_ical_import_result(
                configuration_generation, true, imported, skipped);
        }
        return changed;
    }

    bool handle_calendar_allocation_failure(bool downloaded_document,
                                            std::uint32_t configuration_generation = 0) {
        if (downloaded_document) {
            board::connectivity_service().report_ical_import_result(
                configuration_generation, false, 0, 0);
        }
        period_summary_valid_ = false;
        last_parse_matched_count_ = 0;
        last_parse_truncated_ = false;
        provider_.set_state(ProviderState::Stale);
        apply_provider();
        return true;
    }

    void collect_downloaded_feed() {
        board::CalendarDocument document;
        if (!board::connectivity_service().take_downloaded_ical(document)) return;
        // The parser borrows its source. Cancel first, then keep the new source
        // alive in this move-only PSRAM buffer until the incremental import is
        // either committed or rejected.
        period_parse_session_.reset();
        pending_calendar_document_ = std::move(document);
        pending_document_is_download_ = true;
        period_parse_pending_ = true;
        period_summary_valid_ = false;
        provider_.set_state(ProviderState::Loading);
        apply_provider();
    }

    void queue_period_parse(bool show_calendar = true) {
        period_parse_pending_ = true;
        calendar_view_requested_ = calendar_view_requested_ || show_calendar;
        period_summary_valid_ = false;
        provider_.set_state(ProviderState::Loading);
        apply_provider();
    }

    void start_pending_period_parse() {
        if (!period_parse_pending_) return;
        period_parse_pending_ = false;
        period_parse_session_.reset();

        const board::CalendarDocument* source = pending_document_is_download_
            ? &pending_calendar_document_ : &cached_calendar_document_;
        if (source->empty()) {
            if (calendar_view_requested_) load_current_calendar_screen();
            calendar_view_requested_ = false;
            render_current_calendar_view();
            return;
        }

        try {
            active_parse_is_download_ = pending_document_is_download_;
            active_parse_configuration_generation_ = source->configuration_generation();
            period_parse_session_ = std::make_unique<IcalParseSession>(
                source->view(), feed_options_for(navigation_.visible_period()));
            if (calendar_view_requested_) load_current_calendar_screen();
            calendar_view_requested_ = false;
            render_current_calendar_view();
        } catch (const std::bad_alloc&) {
            const bool was_download = pending_document_is_download_;
            pending_document_is_download_ = false;
            pending_calendar_document_ = {};
            handle_calendar_allocation_failure(was_download,
                                               active_parse_configuration_generation_);
            if (calendar_view_requested_) load_current_calendar_screen();
            calendar_view_requested_ = false;
            render_current_calendar_view();
        }
    }

    void advance_period_parse() {
        if (!period_parse_session_) return;
        try {
            const std::uint32_t parse_start_us = micros();
            std::size_t scheduled_bytes = 0;
            bool complete = false;
            do {
                complete = period_parse_session_->step(kParseBytesPerStep);
                scheduled_bytes += kParseBytesPerStep;
            } while (!complete && scheduled_bytes < kParseBytesPerTick &&
                     static_cast<std::uint32_t>(micros() - parse_start_us) <
                         kParseTimeBudgetUs);
            if (!complete) return;
            IcalParseReport parsed = period_parse_session_->take_report();
            period_parse_session_.reset();
            const bool was_download = active_parse_is_download_;
            const bool accepted = !parsed.input_too_large;
            const std::uint32_t parsed_generation = active_parse_configuration_generation_;
            active_parse_is_download_ = false;
            const bool stale_download = was_download &&
                board::connectivity_service().status().calendar_configuration_generation !=
                    parsed_generation;
            if (stale_download) {
                pending_calendar_document_ = {};
                pending_document_is_download_ = false;
                provider_.set_state(ProviderState::Stale);
                apply_provider();
            } else {
                apply_calendar_report(std::move(parsed), was_download, parsed_generation);
            }
            if (was_download && accepted && !stale_download) {
                // Commit the complete source only after its selected view was
                // parsed successfully. Subsequent browsing rereads this PSRAM
                // document and makes no network request.
                cached_calendar_document_ = std::move(pending_calendar_document_);
                pending_document_is_download_ = false;
            } else if (was_download && !stale_download) {
                pending_calendar_document_ = {};
                pending_document_is_download_ = false;
            }
        } catch (const std::bad_alloc&) {
            const bool was_download = active_parse_is_download_;
            period_parse_session_.reset();
            active_parse_is_download_ = false;
            if (was_download) {
                pending_calendar_document_ = {};
                pending_document_is_download_ = false;
            }
            handle_calendar_allocation_failure(was_download,
                                               active_parse_configuration_generation_);
        }
        bool restore_calendar_scroll = false;
        if (lv_screen_active() == objects.event_details_screen &&
            navigation_.screen() == NavigationScreen::Agenda) {
            details_notice_pending_ = true;
            if (details_origin_ == PrimaryPage::Main) show_main();
            else {
                load_current_calendar_screen();
                restore_calendar_scroll = true;
            }
        } else if (lv_screen_active() == objects.event_details_screen &&
                   navigation_.selected_event_id()) {
            const ProviderResult current = provider_.current();
            if (current.snapshot) {
                const CalendarEvent* refreshed = current.snapshot->find_by_id(
                    *navigation_.selected_event_id());
                if (refreshed != nullptr) render_details(*refreshed);
            }
        }
        render_current_calendar_view(restore_calendar_scroll);
    }

    void request_initial_sync() {
        const board::ConnectivityStatus status = board::connectivity_service().status();
        if (!initial_sync_requested_ && status.credentials_saved && status.time_synchronized) {
            if (status.fetch_state == board::CalendarFetchState::NotRequested) {
                initial_sync_requested_ = board::connectivity_service().request_sync();
            } else {
                // A web-form submission has already requested its first sync.
                // Do not start a duplicate download from the UI loop.
                initial_sync_requested_ = true;
            }
        }
    }

    void update_live_date() {
        const std::uint32_t now_ms = millis();
        if (live_date_update_time_valid_ &&
            static_cast<std::uint32_t>(now_ms - last_live_date_update_ms_) <
                kLiveDateRefreshIntervalMs) {
            return;
        }
        last_live_date_update_ms_ = now_ms;
        live_date_update_time_valid_ = true;
        if (!has_local_time()) return;
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        if (now == static_cast<std::time_t>(-1) || localtime_r(&now, &local) == nullptr) return;
        const CivilDate today = date_from_tm(local);
        if (!local_time_was_available_) {
            local_time_was_available_ = true;
            const CalendarPeriod before = navigation_.visible_period();
            navigation_.set_current_day(today);
            navigation_.show_today();
            if (navigation_.visible_period() != before && !cached_calendar_document_.empty()) {
                queue_period_parse(false);
            } else if (navigation_.screen() == NavigationScreen::Agenda) {
                render_current_calendar_view();
            }
        } else if (today != navigation_.current_day()) {
            const CalendarPeriod before = navigation_.visible_period();
            navigation_.set_current_day(today);
            if (navigation_.visible_period() != before && !cached_calendar_document_.empty()) {
                queue_period_parse(false);
            } else if (navigation_.screen() == NavigationScreen::Agenda) {
                render_current_calendar_view();
            }
        }
        char time[8];
        std::snprintf(time, sizeof(time), "%02d:%02d", local.tm_hour, local.tm_min);
        set_label_text_if_changed(objects.header_time_label, time);
        set_label_text_if_changed(objects.month_time_label, time);
        set_label_text_if_changed(objects.main_time_label, time);
        set_label_text_if_changed(objects.forecast_time_label, time);
        const std::string today_text = date_text(today);
        set_label_text_if_changed(objects.main_date_label, today_text.c_str());
        set_label_text_if_changed(objects.forecast_date_label, today_text.c_str());
        set_label_text_if_changed(objects.month_date_label, today_text.c_str());
    }

    void close_transient_forms() {
        hide_display_wifi_form();
        destroy_display_wifi_form();
        hide_weather_location_form();
        destroy_weather_location_form();
    }

    void render_settings() {
        last_settings_render_ms_ = millis();
        settings_render_time_valid_ = true;
        const board::ConnectivityStatus status = board::connectivity_service().status();
        style_settings_actions(status);
        std::string wifi;
        if (status.setup_ap_active) {
            wifi = std::string("Phone Wi-Fi: ") + status.setup_ssid + "  password: " + status.setup_password;
            if (status.wifi_detail[0] != '\0') wifi += std::string("\n") + status.wifi_detail;
        } else if (status.wifi_connected) {
            wifi = std::string("Connected using saved home Wi-Fi. Open http://") +
                   status.station_address + " on your home Wi-Fi";
        } else if (status.wifi_credentials_saved) {
            wifi = status.wifi_detail[0] != '\0'
                ? std::string("Saved home Wi-Fi: ") + status.wifi_detail
                : "Saved home Wi-Fi: connecting...";
        } else {
            wifi = "No home Wi-Fi saved";
        }
        const char* time = status.time_synchronized ? "Jerusalem time synchronized" : "Waiting for Jerusalem network time";
        const char* fetch_feedback = status.fetch_state == board::CalendarFetchState::Failed
            ? status.fetch_detail : status.summary;
        std::string feed;
        if (status.configured_feed_count == 0) {
            feed = std::string("No private calendars saved - ") + fetch_feedback;
        } else {
            feed = std::to_string(status.configured_feed_count) +
                   (status.configured_feed_count == 1 ? " private calendar saved - "
                                                      : " private calendars saved - ") +
                   fetch_feedback;
        }
        std::string hint;
        if (status.setup_ap_active) {
            hint = std::string("Join the protected Wi-Fi above, then open http://") + status.setup_address +
                   " on your phone. Select your home Wi-Fi and enter its password only.";
        } else if (status.wifi_connected && status.configured_feed_count == 0) {
            hint = calendar_setup_help_visible_
                ? std::string("On your phone: open http://") + status.station_address +
                    "\nChoose Calendars and paste a private iCal address."
                : std::string("Add a calendar at http://") + status.station_address +
                    " on your home Wi-Fi.";
        } else if (status.wifi_connected) {
            hint = std::string("Return your phone to home Wi-Fi, then open http://") + status.station_address +
                   " to manage private Google iCal addresses and see fetch results.";
        } else {
            hint = "Tap Set up to scan and choose the home Wi-Fi. The iCal address is entered only after the display joins it.";
        }
        const bool appearance_changed = !settings_status_layout_valid_ ||
            appearance_.theme_id != last_settings_status_theme_id_ ||
            appearance_.dark_theme != last_settings_status_dark_theme_;
        update_settings_status_value(0, objects.settings_wifi_caption_label,
                                     objects.settings_wifi_status_label, wifi,
                                     appearance_changed);
        update_settings_status_value(1, objects.settings_time_caption_label,
                                     objects.settings_time_status_label, time,
                                     appearance_changed);
        update_settings_status_value(2, objects.settings_feed_caption_label,
                                     objects.settings_feed_status_label, feed,
                                     appearance_changed);
        set_label_text_if_changed(objects.settings_setup_hint_label, hint.c_str());
        last_settings_status_theme_id_ = appearance_.theme_id;
        last_settings_status_dark_theme_ = appearance_.dark_theme;
        settings_status_layout_valid_ = true;
    }

    // LV_LABEL_LONG_DOT replaces the displayed text with an ellipsized copy.
    // Keep the unmodified source text here, rather than comparing it with the
    // label, so an unchanged overflow value does not trigger a 250 ms redraw.
    void update_settings_status_value(std::size_t index, lv_obj_t* caption,
                                      lv_obj_t* value, const std::string& text,
                                      bool force_layout) {
        if (index >= settings_status_text_.size() || value == nullptr || caption == nullptr) return;
        if (!force_layout && settings_status_text_valid_[index] &&
            settings_status_text_[index] == text) {
            return;
        }
        settings_status_text_[index] = text;
        settings_status_text_valid_[index] = true;
        // Restore wrapping before measuring a new value so a formerly
        // ellipsized two-line value can grow correctly.
        lv_label_set_long_mode(value, LV_LABEL_LONG_WRAP);
        lv_obj_set_height(value, LV_SIZE_CONTENT);
        lv_label_set_text(value, text.c_str());
        lv_obj_update_layout(value);
        // Two lines fit each 50 px status row. Overflow remains explicit.
        const int32_t measured = lv_obj_get_height(value);
        const int32_t height = std::min<int32_t>(48, measured);
        lv_obj_set_height(value, height);
        if (measured > height) lv_label_set_long_mode(value, LV_LABEL_LONG_DOT);
        lv_obj_set_y(value, lv_obj_get_y(caption) +
            (lv_obj_get_height(caption) - height) / 2);
    }

    void style_settings_actions(const board::ConnectivityStatus& status) {
        const lv_obj_t* primary = !status.wifi_connected
            ? objects.settings_enter_wifi_button
            : objects.settings_sync_now_button;
        if (objects.settings_sync_now_button != nullptr) {
            set_label_text_if_changed(lv_obj_get_child(objects.settings_sync_now_button, 0),
                status.wifi_connected && status.configured_feed_count == 0
                    ? "Add your calendar" : "Sync calendar");
        }
        if (settings_actions_styled_ && primary == last_settings_primary_ &&
            appearance_.theme_id == last_settings_theme_id_ &&
            appearance_.dark_theme == last_settings_dark_theme_) {
            return;
        }
        for (lv_obj_t* button : {objects.settings_enter_wifi_button,
                                 objects.settings_start_setup_button,
                                 objects.settings_sync_now_button,
                                 objects.settings_brightness_button,
                                 objects.settings_weather_button,
                                 objects.settings_firmware_button}) {
            if (button == nullptr) continue;
            const bool prominent = button == primary;
            const bool open = recipe().surface_grammar() == SurfaceGrammar::OpenSurfaces;
            const bool strong = recipe().surface_grammar() == SurfaceGrammar::StrongBlocks;
            lv_obj_set_style_bg_color(button,
                lv_color_hex(prominent ? palette().action : palette().surface_subtle),
                LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(button,
                lv_color_hex(prominent ? palette().action_pressed : palette().action_soft),
                LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_width(
                button, prominent || open ? 0
                                          : (strong ? recipe().structural_border_width
                                                    : recipe().divider_width),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(
                button, lv_color_hex(strong ? palette().text_primary : palette().divider),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_side(button, LV_BORDER_SIDE_FULL,
                                         LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(button, metrics().control_radius, LV_PART_MAIN | LV_STATE_DEFAULT);
            if (lv_obj_get_child_count(button) != 0) {
                const std::uint32_t ink = prominent
                    ? palette().on_action
                    : palette().text_primary;
                style_button_text(button, ink, ink);
            }
        }
        last_settings_primary_ = primary;
        last_settings_theme_id_ = appearance_.theme_id;
        last_settings_dark_theme_ = appearance_.dark_theme;
        settings_actions_styled_ = true;
    }

    void render_appearance_settings() {
        if (objects.theme_style_dropdown != nullptr) {
            lv_dropdown_set_selected(objects.theme_style_dropdown,
                                     static_cast<std::uint32_t>(appearance_.theme_id));
        }
        if (objects.dark_theme_switch != nullptr) {
            if (appearance_.dark_theme) {
                lv_obj_add_state(objects.dark_theme_switch, LV_STATE_CHECKED);
            } else {
                lv_obj_remove_state(objects.dark_theme_switch, LV_STATE_CHECKED);
            }
        }
        // Non-interactive swatches make the saved palette choice visible
        // without adding another appearance setting or LVGL callback.
        for (const auto& preview : std::array<std::pair<lv_obj_t*, std::uint32_t>, 3>{
                 {{objects.theme_preview_canvas, palette().canvas},
                  {objects.theme_preview_surface, palette().surface},
                  {objects.theme_preview_accent, palette().action}}}) {
            if (preview.first == nullptr) continue;
            lv_obj_set_style_bg_color(preview.first, lv_color_hex(preview.second),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(preview.first, LV_OPA_COVER,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(preview.first, lv_color_hex(palette().divider),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(preview.first, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        if (objects.brightness_slider != nullptr) {
            lv_slider_set_value(objects.brightness_slider, appearance_.brightness_percent, LV_ANIM_OFF);
        }
        if (objects.brightness_value_label != nullptr) {
            update_brightness_value_label();
        }
    }

    void update_brightness_value_label() {
        if (objects.brightness_value_label == nullptr) return;
        char text[8];
        std::snprintf(text, sizeof(text), "%u%%",
                      static_cast<unsigned>(appearance_.brightness_percent));
        set_label_text_if_changed(objects.brightness_value_label, text);
    }

    void ensure_main_row(std::size_t index) {
        if (main_rows_[index] != nullptr) return;
        lv_obj_t* row = lv_button_create(objects.main_events_list);
        main_rows_[index] = row;
        lv_obj_set_pos(row, 0, static_cast<lv_coord_t>(index * 56));
        lv_obj_set_size(row, lv_pct(100), 54);
        lv_obj_set_style_bg_color(row, lv_color_hex(palette().surface), LV_PART_MAIN);
        lv_obj_set_style_bg_color(row, lv_color_hex(palette().action_soft), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_border_color(row, lv_color_hex(palette().divider), LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(row, metrics().card_radius, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(row, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(row, on_main_event_row_clicked, LV_EVENT_CLICKED,
                            &main_row_bindings_[index]);

        main_row_colors_[index] = lv_obj_create(row);
        lv_obj_set_pos(main_row_colors_[index], 7, 20);
        lv_obj_set_size(main_row_colors_[index], 12, 12);
        lv_obj_remove_flag(main_row_colors_[index], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(main_row_colors_[index], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_border_width(main_row_colors_[index], 0, LV_PART_MAIN);
        lv_obj_set_style_radius(main_row_colors_[index], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        style_event_row(row, main_row_colors_[index], true);

        main_row_titles_[index] = lv_label_create(row);
        lv_obj_set_pos(main_row_titles_[index], 27, 3);
        lv_obj_set_size(main_row_titles_[index], 255, 24);
        // LONG_DOT edits the label buffer while laying it out. Give the label
        // an owned buffer before enabling it, then keep using copied text.
        lv_label_set_text(main_row_titles_[index], "");
        lv_label_set_long_mode(main_row_titles_[index], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(main_row_titles_[index], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        set_dynamic_font(main_row_titles_[index]);

        main_row_sources_[index] = lv_label_create(row);
        lv_obj_set_pos(main_row_sources_[index], 27, 27);
        lv_obj_set_size(main_row_sources_[index], 255, 22);
        lv_label_set_text(main_row_sources_[index], "");
        lv_label_set_long_mode(main_row_sources_[index], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(main_row_sources_[index], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        set_dynamic_font(main_row_sources_[index]);

        main_row_times_[index] = lv_label_create(row);
        lv_obj_set_pos(main_row_times_[index], 286, 15);
        lv_obj_set_size(main_row_times_[index], 130, 24);
        lv_obj_set_style_text_align(main_row_times_[index], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
        lv_obj_set_style_text_color(main_row_times_[index], lv_color_hex(palette().text_muted), LV_PART_MAIN);
        set_compact_ascii_font(main_row_times_[index]);
    }

    static void on_main_event_row_clicked(lv_event_t* event) {
        auto* binding = static_cast<RowBinding*>(lv_event_get_user_data(event));
        if (binding == nullptr || binding->event_id[0] == '\0') return;
        instance().open_main_event(binding->event_id);
    }

    void open_main_event(const std::string& event_id) {
        const ProviderResult result = provider_.current();
        apply_provider();
        if (!result.snapshot || !navigation_.open_event(event_id, *result.snapshot, 0)) {
            render_main();
            return;
        }
        const CalendarEvent* event = result.snapshot->find_by_id(event_id);
        if (event == nullptr) {
            show_main();
            return;
        }
        details_origin_ = PrimaryPage::Main;
        render_details(*event);
        loadScreen(SCREEN_ID_EVENT_DETAILS_SCREEN);
    }

    void render_main() {
        apply_provider();
        set_provider_header(objects.main_sync_state_label);
        const bool show_details_notice = details_notice_pending_;
        details_notice_pending_ = false;
        const bool time_available = has_local_time();
        if (!time_available) {
            set_label_text_if_changed(objects.main_time_label, "--:--");
            set_label_text_if_changed(objects.main_date_label, "Date unavailable");
        }
        const ProviderResult result = provider_.current();
        last_main_provider_state_ = result.state;
        last_main_snapshot_ = result.snapshot.get();
        last_main_render_minute_ = std::time(nullptr) / 60;
        main_render_signature_valid_ = true;
        const CalendarEventList matches = result.snapshot && time_available
            ? filter_and_sort_agenda(*result.snapshot, local_day_window(navigation_.current_day()))
            : CalendarEventList{};
        const std::size_t visible = std::min(matches.size(), kMaxMainRows);
        for (std::size_t index = 0; index < visible; ++index) {
            ensure_main_row(index);
            RowBinding& binding = main_row_bindings_[index];
            std::strncpy(binding.event_id, matches[index].id.c_str(), kMaxEventIdBytes);
            binding.event_id[kMaxEventIdBytes] = '\0';
            set_label_text_if_changed(main_row_titles_[index], display_title(matches[index]).c_str());
            set_label_text_if_changed(main_row_sources_[index],
                                      matches[index].calendar.display_name.c_str());
            lv_obj_set_style_text_color(main_row_sources_[index],
                                        lv_color_hex(palette().text_muted),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            const std::string time = agenda_time_text(matches[index]);
            set_label_text_if_changed(main_row_times_[index], time.c_str());
            lv_obj_set_style_bg_color(main_row_colors_[index],
                                      lv_color_hex(agenda_indicator_color(
                                          matches[index].calendar.color_rgb888, palette())),
                                      LV_PART_MAIN);
            lv_obj_remove_flag(main_rows_[index], LV_OBJ_FLAG_HIDDEN);
        }
        for (std::size_t index = visible; index < main_rows_.size(); ++index) {
            if (main_rows_[index] != nullptr) lv_obj_add_flag(main_rows_[index], LV_OBJ_FLAG_HIDDEN);
            main_row_bindings_[index].event_id[0] = '\0';
        }

        set_label_text_if_changed(objects.main_today_label, "Today");
        if (!time_available) {
            set_label_text_if_changed(objects.main_events_state_label, "Waiting for time");
            set_label_text_if_changed(objects.main_events_hint_label, "Connect to set today's date");
        } else if (navigation_.provider_state() == ProviderState::Loading) {
            set_label_text_if_changed(objects.main_events_state_label, "Loading today's calendar...");
            set_label_text_if_changed(objects.main_events_hint_label, "Reading the saved calendar");
        } else if (navigation_.provider_state() == ProviderState::Stale) {
            set_label_text_if_changed(objects.main_events_state_label, "Showing the last saved calendar");
            set_label_text_if_changed(objects.main_events_hint_label, "Tap an event for details");
        } else if (navigation_.provider_state() == ProviderState::Error) {
            set_label_text_if_changed(objects.main_events_state_label, "Calendar unavailable - check Settings");
            set_label_text_if_changed(objects.main_events_hint_label, "Open Settings for connection help");
        } else if (matches.empty()) {
            set_label_text_if_changed(objects.main_events_state_label, "No events today");
            set_label_text_if_changed(objects.main_events_hint_label, "Your calendar is clear");
        } else if (matches.size() > kMaxMainRows) {
            lv_label_set_text_fmt(objects.main_events_state_label, "%u of %u events",
                                  static_cast<unsigned>(kMaxMainRows),
                                  static_cast<unsigned>(matches.size()));
            set_label_text_if_changed(objects.main_events_hint_label, "Tap an event for details");
        } else {
            lv_label_set_text_fmt(objects.main_events_state_label, "%u event%s",
                                  static_cast<unsigned>(matches.size()),
                                  matches.size() == 1 ? "" : "s");
            set_label_text_if_changed(objects.main_events_hint_label, "Tap an event for details");
        }
        if (show_details_notice) {
            set_label_text_if_changed(objects.main_events_hint_label,
                                      "This event is no longer available");
        }

        const board::WeatherServiceStatus status = board::weather_service().status();
        last_weather_status_ = status;
        weather_render_signature_valid_ = true;
        calendar::weather::WeatherSnapshot weather{};
        const bool weather_available = board::weather_service().snapshot(weather) && weather.current.available;
        set_label_text_if_changed(objects.main_weather_location_label,
                                  status.location[0] ? status.location : "Weather");
        set_label_text_if_changed(objects.main_weather_forecast_label, "Forecast >");
        const char* temp_unit = temperature_unit(status.units);
        const char* speed_unit = wind_unit(status.units);
        if (weather_available) {
            weather_icon::update(main_weather_icon_, weather.current.condition_id, true);
            const std::string temperature =
                tenths_text(display_temperature_tenths(weather.current.temperature_tenths_c,
                                                        status.units)) +
                " " + temp_unit;
            set_label_text_if_changed(objects.main_weather_temperature_label, temperature.c_str());
            set_label_text_if_changed(objects.main_weather_condition_label,
                                      weather.current.description[0]
                                          ? weather.current.description.data() : "Current conditions");
            const std::string feels = tenths_text(display_temperature_tenths(
                weather.current.feels_like_tenths_c, status.units)) + " " + temp_unit;
            const std::string humidity = std::to_string(
                static_cast<unsigned>(weather.current.humidity_percent)) + "%";
            const std::string wind = tenths_text(display_wind_tenths(
                weather.current.wind_tenths_mps, status.units)) + " " + speed_unit;
            set_label_text_if_changed(objects.main_weather_feels_value_label, feels.c_str());
            set_label_text_if_changed(objects.main_weather_humidity_label, "Humidity");
            set_label_text_if_changed(objects.main_weather_humidity_value_label, humidity.c_str());
            set_label_text_if_changed(objects.main_weather_wind_label, "Wind");
            set_label_text_if_changed(objects.main_weather_wind_value_label, wind.c_str());
        } else {
            weather_icon::update(main_weather_icon_, 0, false);
            const std::string missing_temperature = std::string("--.- ") + temp_unit;
            set_label_text_if_changed(objects.main_weather_temperature_label,
                                      missing_temperature.c_str());
            set_label_text_if_changed(objects.main_weather_condition_label,
                                      status.detail[0] ? status.detail : "Weather unavailable");
            const std::string missing_feels = std::string("-- ") + temp_unit;
            const std::string missing_wind = std::string("-- ") + speed_unit;
            set_label_text_if_changed(objects.main_weather_feels_value_label, missing_feels.c_str());
            set_label_text_if_changed(objects.main_weather_humidity_label, "Humidity");
            set_label_text_if_changed(objects.main_weather_humidity_value_label, "--%");
            set_label_text_if_changed(objects.main_weather_wind_label, "Wind");
            set_label_text_if_changed(objects.main_weather_wind_value_label, missing_wind.c_str());
        }
        if (status.updated_utc > 0) {
            std::tm local{};
            const std::time_t updated = static_cast<std::time_t>(status.updated_utc);
            if (localtime_r(&updated, &local) != nullptr) {
                char updated_text[24];
                std::snprintf(updated_text, sizeof(updated_text), "Updated %02d:%02d",
                              local.tm_hour, local.tm_min);
                set_label_text_if_changed(objects.main_weather_updated_label, updated_text);
            }
        } else {
            set_label_text_if_changed(objects.main_weather_updated_label, "Not updated yet");
        }
        const std::array<lv_obj_t*, 13> labels{
            objects.main_today_label, objects.main_events_state_label, objects.main_events_hint_label,
            objects.main_weather_location_label, objects.main_weather_temperature_label,
            objects.main_weather_condition_label, objects.main_weather_feels_value_label,
            objects.main_weather_humidity_label, objects.main_weather_humidity_value_label,
            objects.main_weather_wind_label, objects.main_weather_wind_value_label,
            objects.main_weather_updated_label, objects.main_sync_state_label};
        for (lv_obj_t* label : labels) set_dynamic_font(label);
        set_large_temperature_font(objects.main_weather_temperature_label);
        set_large_time_font(objects.main_time_label);
        set_emphasis_font(objects.main_today_label);
        set_emphasis_font(objects.main_date_label);
    }

    void render_weather() {
        set_provider_header(objects.forecast_sync_state_label);
        const board::WeatherServiceStatus status = board::weather_service().status();
        last_weather_status_ = status;
        weather_render_signature_valid_ = true;
        calendar::weather::WeatherSnapshot snapshot{};
        const bool available = board::weather_service().snapshot(snapshot);
        set_label_text_if_changed(objects.weather_location_label,
                                  status.location[0] ? status.location : "Weather");
        set_label_text_if_changed(objects.weather_state_label, status.detail);
        const char* temp_unit = temperature_unit(status.units);
        const char* speed_unit = wind_unit(status.units);

        if (available && snapshot.current.available) {
            weather_icon::update(current_weather_icon_, snapshot.current.condition_id, true);
            const std::string temperature =
                tenths_text(display_temperature_tenths(snapshot.current.temperature_tenths_c,
                                                        status.units)) +
                " " + temp_unit;
            set_label_text_if_changed(objects.weather_temperature_label, temperature.c_str());
            set_label_text_if_changed(objects.weather_condition_label,
                                      snapshot.current.description[0]
                                          ? snapshot.current.description.data() : "Current conditions");
            const std::string feels = "Feels like " +
                tenths_text(display_temperature_tenths(snapshot.current.feels_like_tenths_c,
                                                        status.units)) +
                " " + temp_unit;
            set_label_text_if_changed(objects.weather_feels_like_label, feels.c_str());
            char humidity[24];
            std::snprintf(humidity, sizeof(humidity), "Humidity %u%%",
                          static_cast<unsigned>(snapshot.current.humidity_percent));
            set_label_text_if_changed(objects.weather_humidity_label, humidity);
            const std::string wind = "Wind " +
                tenths_text(display_wind_tenths(snapshot.current.wind_tenths_mps, status.units)) +
                " " + speed_unit;
            set_label_text_if_changed(objects.weather_wind_label, wind.c_str());
        } else {
            weather_icon::update(current_weather_icon_, 0, false);
            const std::string missing_temperature = std::string("--.- ") + temp_unit;
            set_label_text_if_changed(objects.weather_temperature_label,
                                      missing_temperature.c_str());
            set_label_text_if_changed(objects.weather_condition_label, "No weather data yet");
            const std::string missing_feels = std::string("Feels like --.- ") + temp_unit;
            set_label_text_if_changed(objects.weather_feels_like_label, missing_feels.c_str());
            set_label_text_if_changed(objects.weather_humidity_label, "Humidity --%");
            const std::string missing_wind = std::string("Wind --.- ") + speed_unit;
            set_label_text_if_changed(objects.weather_wind_label, missing_wind.c_str());
        }

        if (status.updated_utc > 0) {
            std::tm local{};
            const std::time_t updated = static_cast<std::time_t>(status.updated_utc);
            if (localtime_r(&updated, &local) != nullptr) {
                char updated_text[24];
                std::snprintf(updated_text, sizeof(updated_text), "Updated %02d:%02d",
                              local.tm_hour, local.tm_min);
                set_label_text_if_changed(objects.weather_updated_label, updated_text);
            }
        } else {
            set_label_text_if_changed(objects.weather_updated_label, "Not updated yet");
        }

        for (std::size_t index = 0; index < forecast_day_labels_.size(); ++index) {
            if (available && index < snapshot.forecast_count) {
                const auto& day = snapshot.forecast[index];
                char day_text[16];
                const CivilDate forecast_date{day.date.year, day.date.month, day.date.day};
                std::snprintf(day_text, sizeof(day_text), "%s\n%02u/%02u",
                              weekday_name(forecast_date),
                              static_cast<unsigned>(day.date.day),
                              static_cast<unsigned>(day.date.month));
                set_label_text_if_changed(forecast_day_labels_[index], day_text);
                const auto whole_degrees = [](std::int16_t tenths) {
                    return static_cast<int>((tenths >= 0 ? tenths + 5 : tenths - 5) / 10);
                };
                char temperature_text[24];
                std::snprintf(temperature_text, sizeof(temperature_text), "%d / %d",
                              whole_degrees(display_temperature_tenths(
                                  day.minimum_tenths_c, status.units)),
                              whole_degrees(display_temperature_tenths(
                                  day.maximum_tenths_c, status.units)));
                set_label_text_if_changed(forecast_temp_labels_[index], temperature_text);
                weather_icon::update(forecast_weather_icons_[index],
                                     day.dominant_condition_id, true);
                char condition_text[40];
                std::snprintf(condition_text, sizeof(condition_text), "%s\nPrecip %u%%",
                              compact_forecast_condition(day.dominant_condition_id),
                              static_cast<unsigned>(day.maximum_precipitation_probability_percent));
                set_label_text_if_changed(forecast_condition_labels_[index], condition_text);
            } else {
                weather_icon::update(forecast_weather_icons_[index], 0, false);
                set_label_text_if_changed(forecast_day_labels_[index], "--/--");
                const std::string missing_forecast = "-- / --";
                set_label_text_if_changed(forecast_temp_labels_[index],
                                          missing_forecast.c_str());
                set_label_text_if_changed(forecast_condition_labels_[index], "No forecast");
            }
            set_compact_ascii_font(forecast_day_labels_[index]);
            set_emphasis_font(forecast_temp_labels_[index]);
            const char* value = lv_label_get_text(forecast_temp_labels_[index]);
            if (lv_text_get_width(value, std::strlen(value), &lv_font_montserrat_20, 0) >
                lv_obj_get_width(forecast_temp_labels_[index])) {
                set_compact_ascii_font(forecast_temp_labels_[index]);
            }
            lv_obj_set_style_base_dir(forecast_temp_labels_[index], LV_BASE_DIR_LTR,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            // Forecast conditions are intentionally mapped to short ASCII
            // labels. Compact type keeps both lines inside the 100 px card.
            set_compact_ascii_font(forecast_condition_labels_[index]);
            lv_obj_set_style_text_letter_space(forecast_condition_labels_[index], -1,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(forecast_condition_labels_[index], LV_TEXT_ALIGN_CENTER,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        const std::array<lv_obj_t*, 9> weather_labels{
            objects.weather_location_label, objects.weather_temperature_label,
            objects.weather_condition_label, objects.weather_feels_like_label,
            objects.weather_humidity_label, objects.weather_wind_label,
            objects.weather_updated_label, objects.weather_state_label,
            objects.weather_attribution_label};
        for (lv_obj_t* label : weather_labels) set_dynamic_font(label);
        set_large_temperature_font(objects.weather_temperature_label);
    }

    void render_firmware_update() {
        last_firmware_render_ms_ = millis();
        firmware_render_time_valid_ = true;
        const board::OtaStatus status = board::ota_service().status();
        const char* state = "Disabled";
        switch (status.state) {
            case board::OtaState::Disabled: state = "Firmware upload disabled"; break;
            case board::OtaState::Armed: state = "Upload window open"; break;
            case board::OtaState::Receiving: state = "Receiving firmware"; break;
            case board::OtaState::Verifying: state = "Verifying firmware"; break;
            case board::OtaState::ReadyToReboot: state = "Firmware ready"; break;
            case board::OtaState::Failed: state = "Firmware update failed"; break;
        }
        FirmwareRenderSignature next{};
        next.state = status.state;
        next.theme_id = appearance_.theme_id;
        next.dark_theme = appearance_.dark_theme;
        if (status.state == board::OtaState::Armed) {
            std::snprintf(next.instructions, sizeof(next.instructions),
                          "Open %s on home Wi-Fi\nCode: %s  (%lu seconds)",
                          status.upload_url, status.one_time_code,
                          static_cast<unsigned long>(status.seconds_remaining));
        } else if (status.state == board::OtaState::Receiving) {
            std::snprintf(next.instructions, sizeof(next.instructions), "Received %lu KiB",
                          static_cast<unsigned long>(status.received_bytes / 1024U));
        } else {
            std::snprintf(next.instructions, sizeof(next.instructions), "%s", status.detail);
        }
        if (status.state == board::OtaState::ReadyToReboot) next.progress = 100;
        else if (status.total_bytes != 0) {
            next.progress = static_cast<int32_t>(std::min<std::size_t>(100,
                (status.received_bytes * 100U) / status.total_bytes));
        }
        next.can_arm = status.state == board::OtaState::Disabled ||
                       status.state == board::OtaState::Failed;
        next.can_cancel = status.state == board::OtaState::Armed;
        next.can_reboot = status.state == board::OtaState::ReadyToReboot;
        const bool text_or_appearance_changed = !firmware_paint_signature_valid_ ||
            next.state != firmware_paint_signature_.state ||
            next.theme_id != firmware_paint_signature_.theme_id ||
            next.dark_theme != firmware_paint_signature_.dark_theme ||
            std::strcmp(next.instructions, firmware_paint_signature_.instructions) != 0;
        if (firmware_paint_signature_valid_ &&
            next.matches(firmware_paint_signature_)) {
            return;
        }
        firmware_paint_signature_ = next;
        firmware_paint_signature_valid_ = true;
        const std::string version = std::string("Current firmware: ") + kFirmwareVersion;
        set_label_text_if_changed(objects.firmware_version_label, version.c_str());
        set_label_text_if_changed(objects.firmware_state_label, state);
        set_label_text_if_changed(objects.firmware_instructions_label, next.instructions);
        if (lv_bar_get_value(objects.firmware_progress_bar) != next.progress) {
            lv_bar_set_value(objects.firmware_progress_bar, next.progress, LV_ANIM_OFF);
        }
        style_firmware_actions(next);
        if (text_or_appearance_changed) {
            set_dynamic_font(objects.firmware_version_label);
            set_dynamic_font(objects.firmware_state_label);
            set_dynamic_font(objects.firmware_instructions_label);
        }
    }

    void style_firmware_actions(const FirmwareRenderSignature& signature) {
        if (firmware_actions_paint_valid_ &&
            signature.can_arm == last_firmware_can_arm_ &&
            signature.can_cancel == last_firmware_can_cancel_ &&
            signature.can_reboot == last_firmware_can_reboot_ &&
            signature.theme_id == last_firmware_theme_id_ &&
            signature.dark_theme == last_firmware_dark_theme_) {
            return;
        }
        if (signature.can_arm) lv_obj_remove_state(objects.firmware_enable_button, LV_STATE_DISABLED);
        else lv_obj_add_state(objects.firmware_enable_button, LV_STATE_DISABLED);
        if (signature.can_cancel) lv_obj_remove_state(objects.firmware_cancel_button, LV_STATE_DISABLED);
        else lv_obj_add_state(objects.firmware_cancel_button, LV_STATE_DISABLED);
        if (signature.can_reboot) lv_obj_remove_state(objects.firmware_reboot_button, LV_STATE_DISABLED);
        else lv_obj_add_state(objects.firmware_reboot_button, LV_STATE_DISABLED);
        for (const auto& action : std::array<std::pair<lv_obj_t*, bool>, 3>{{
                 {objects.firmware_enable_button, signature.can_arm},
                 {objects.firmware_cancel_button, signature.can_cancel},
                 {objects.firmware_reboot_button, signature.can_reboot}}}) {
            lv_obj_t* button = action.first;
            const bool primary = action.second;
            lv_obj_set_style_bg_color(button,
                lv_color_hex(primary ? palette().action : palette().surface_subtle), LV_PART_MAIN);
            lv_obj_set_style_bg_color(button,
                lv_color_hex(primary ? palette().action_pressed : palette().action_soft),
                LV_PART_MAIN | LV_STATE_PRESSED);
            style_button_text(button, primary ? palette().on_action : palette().text_muted,
                              primary ? palette().on_action : palette().text_primary);
        }
        last_firmware_can_arm_ = signature.can_arm;
        last_firmware_can_cancel_ = signature.can_cancel;
        last_firmware_can_reboot_ = signature.can_reboot;
        last_firmware_theme_id_ = signature.theme_id;
        last_firmware_dark_theme_ = signature.dark_theme;
        firmware_actions_paint_valid_ = true;
    }

    void apply_ascii_letter_spacing(lv_obj_t* object) const {
        if (object == nullptr) return;
        if (lv_obj_check_type(object, &lv_label_class)) {
            lv_obj_set_style_text_letter_space(object, recipe().ascii_title_letter_space,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        const std::uint32_t children = lv_obj_get_child_count(object);
        for (std::uint32_t index = 0; index < children; ++index) {
            apply_ascii_letter_spacing(lv_obj_get_child(object, static_cast<int32_t>(index)));
        }
    }

    void style_surface_card(lv_obj_t* object) const {
        if (object == nullptr) return;
        std::uint32_t background = palette().surface;
        std::uint32_t border = palette().divider;
        std::uint8_t border_width = recipe().structural_border_width;
        switch (recipe().surface_grammar()) {
            case SurfaceGrammar::LayeredCards:
                background = palette().surface;
                break;
            case SurfaceGrammar::RuledGrid:
                background = palette().surface;
                break;
            case SurfaceGrammar::StrongBlocks:
                background = palette().surface_subtle;
                border = palette().text_primary;
                break;
            case SurfaceGrammar::OpenSurfaces:
                background = palette().surface;
                border_width = 0;
                break;
        }
        lv_obj_set_style_bg_color(object, lv_color_hex(background), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(object, lv_color_hex(border), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(object, border_width, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_side(object, LV_BORDER_SIDE_FULL, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(object, recipe().card_radius, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(object, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        if (lv_obj_check_type(object, &lv_button_class)) {
            neutralize_color_filter(object);
            lv_obj_set_style_bg_color(object, lv_color_hex(palette().action_soft),
                                      LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
        }
    }

    void style_event_row(lv_obj_t* row, lv_obj_t* marker, bool compact) const {
        if (row == nullptr) return;
        neutralize_color_filter(row);
        lv_obj_set_style_text_color(row, lv_color_hex(palette().text_primary), LV_PART_MAIN);
        lv_obj_set_style_text_color(row, lv_color_hex(palette().text_primary),
                                    LV_PART_MAIN | LV_STATE_PRESSED);
        std::uint32_t background = palette().surface;
        std::uint32_t border = palette().divider;
        std::uint8_t border_width = 0;
        lv_border_side_t border_side = LV_BORDER_SIDE_FULL;
        switch (recipe().surface_grammar()) {
            case SurfaceGrammar::LayeredCards:
                background = palette().surface_subtle;
                border_width = recipe().structural_border_width;
                break;
            case SurfaceGrammar::RuledGrid:
                border_width = recipe().divider_width;
                border_side = LV_BORDER_SIDE_BOTTOM;
                break;
            case SurfaceGrammar::StrongBlocks:
                background = palette().surface_subtle;
                border = palette().text_primary;
                border_width = recipe().structural_border_width;
                break;
            case SurfaceGrammar::OpenSurfaces:
                break;
        }
        lv_obj_set_style_bg_color(row, lv_color_hex(background), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(row, lv_color_hex(palette().action_soft),
                                  LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_border_color(row, lv_color_hex(border), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(row, border_width, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_side(row, border_side, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(row, recipe().card_radius, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        if (marker != nullptr) {
            const int32_t size = recipe().event_marker_size;
            const int32_t row_height = 54;
            lv_obj_set_pos(marker, compact ? 7 : 8, (row_height - size) / 2);
            lv_obj_set_size(marker, size, size);
            lv_obj_set_style_border_color(marker, lv_color_hex(palette().text_primary),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(
                marker,
                recipe().surface_grammar() == SurfaceGrammar::StrongBlocks
                    ? recipe().structural_border_width : 0,
                LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_side(marker, LV_BORDER_SIDE_FULL,
                                         LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(marker, recipe().event_marker_radius,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    void style_dropdown_popup(lv_obj_t* dropdown) const {
        if (dropdown == nullptr || !lv_obj_check_type(dropdown, &lv_dropdown_class)) return;
        lv_obj_t* list = lv_dropdown_get_list(dropdown);
        if (list == nullptr) return;
        const int32_t font_height = lv_font_get_line_height(&lv_font_dejavu_16_persian_hebrew);
        const int32_t line_space = std::max<int32_t>(0, 44 - font_height);
        lv_obj_set_style_bg_color(list, lv_color_hex(palette().surface),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(list, &lv_font_dejavu_16_persian_hebrew,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(list, lv_color_hex(palette().text_primary),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_line_space(list, line_space, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(list, lv_color_hex(palette().border),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(list, recipe().divider_width,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_side(list, LV_BORDER_SIDE_FULL,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(list, recipe().input_radius, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(list, lv_color_hex(palette().selection),
                                  LV_PART_SELECTED | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(list, lv_color_hex(palette().on_selection),
                                    LV_PART_SELECTED | LV_STATE_DEFAULT);
        lv_obj_set_style_text_line_space(list, line_space,
                                         LV_PART_SELECTED | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(list, lv_color_hex(palette().action_soft),
                                  LV_PART_SELECTED | LV_STATE_PRESSED);
        lv_obj_set_style_text_color(list, lv_color_hex(palette().text_primary),
                                    LV_PART_SELECTED | LV_STATE_PRESSED);
    }

    void neutralize_color_filter(lv_obj_t* object) const {
        // LVGL's default theme applies a pressed color filter through an
        // inheritable style. The palette supplies complete pressed colors, so
        // neutralize that filter locally before applying role-pair values.
        for (lv_state_t state : {LV_STATE_DEFAULT, LV_STATE_PRESSED, LV_STATE_CHECKED}) {
            lv_obj_set_style_color_filter_dsc(object, nullptr, LV_PART_MAIN | state);
            lv_obj_set_style_color_filter_opa(object, LV_OPA_TRANSP, LV_PART_MAIN | state);
        }
        if (lv_obj_check_type(object, &lv_button_class)) {
            lv_obj_set_style_shadow_width(object, 0, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(object, 0, LV_PART_MAIN | LV_STATE_PRESSED);
        }
    }

    void style_button_text(lv_obj_t* button, std::uint32_t normal,
                           std::uint32_t pressed) const {
        neutralize_color_filter(button);
        lv_obj_set_style_text_color(button, lv_color_hex(normal), LV_PART_MAIN);
        lv_obj_set_style_text_color(button, lv_color_hex(pressed), LV_PART_MAIN | LV_STATE_PRESSED);
        // Single-label controls share one font and center within the final
        // content rectangle, including border changes in selected states.
        if (lv_obj_get_child_count(button) == 1) {
            lv_obj_t* label = lv_obj_get_child(button, 0);
            if (lv_obj_check_type(label, &lv_label_class)) {
                set_compact_ascii_font(label);
                lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
                lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                lv_obj_center(label);
            }
        }
        // Labels do not enter PRESSED when their button does. Inherit the
        // resolved parent ink so the displayed label follows the button state.
        for (std::uint32_t i = 0; i < lv_obj_get_child_count(button); ++i) {
            lv_obj_t* child = lv_obj_get_child(button, static_cast<int32_t>(i));
            if (!lv_obj_check_type(child, &lv_label_class)) continue;
            for (lv_state_t state : {LV_STATE_DEFAULT, LV_STATE_PRESSED, LV_STATE_CHECKED}) {
                lv_obj_remove_local_style_prop(child, LV_STYLE_TEXT_COLOR, LV_PART_MAIN | state);
            }
            neutralize_color_filter(child);
        }
    }

    void apply_appearance_to_tree(lv_obj_t* object) const {
        if (object == nullptr) return;
        neutralize_color_filter(object);
        lv_obj_set_style_bg_color(object, lv_color_hex(palette().surface),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(object, lv_color_hex(palette().text_secondary),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(object, lv_color_hex(palette().divider),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        if (lv_obj_check_type(object, &lv_button_class)) {
            lv_obj_set_style_radius(object, metrics().control_radius,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        const std::uint32_t children = lv_obj_get_child_count(object);
        for (std::uint32_t index = 0; index < children; ++index) {
            apply_appearance_to_tree(lv_obj_get_child(object, static_cast<int32_t>(index)));
        }
    }

    void apply_appearance_to_widgets() {
        // The tree walk below resets button styles even when the primary
        // Settings action and theme signature appear unchanged.
        settings_actions_styled_ = false;
        // The tree walk below can override screen-local text and control
        // styling even if the selected theme ID did not change. Force the
        // next screen render to restore its derived layout and paint.
        settings_status_layout_valid_ = false;
        firmware_paint_signature_valid_ = false;
        firmware_actions_paint_valid_ = false;
        // EEZ owns these static trees. Walk every generated screen so a
        // runtime theme change also updates inactive screens and their labels.
        const std::array<lv_obj_t*, 8> screens{
            objects.main_screen, objects.agenda_screen, objects.event_details_screen, objects.settings_screen,
            objects.month_screen, objects.brightness_settings_screen, objects.weather_screen,
            objects.firmware_update_screen};
        for (lv_obj_t* screen : screens) {
            apply_appearance_to_tree(screen);
            if (screen != nullptr) {
                lv_obj_set_style_bg_color(screen, lv_color_hex(palette().canvas),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }

        const std::array<lv_obj_t*, 8> headers{
            objects.main_header, objects.agenda_header, objects.details_header, objects.settings_header,
            objects.month_header, objects.brightness_header, objects.weather_header,
            objects.firmware_header};
        for (lv_obj_t* header : headers) {
            if (header == nullptr) continue;
            lv_obj_set_style_bg_color(header, lv_color_hex(palette().header), LV_PART_MAIN | LV_STATE_DEFAULT);
            const bool ruled = recipe().surface_grammar() == SurfaceGrammar::RuledGrid ||
                                recipe().surface_grammar() == SurfaceGrammar::StrongBlocks;
            lv_obj_set_style_border_color(header,
                                          lv_color_hex(ruled ? palette().action : palette().divider),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(header, recipe().structural_border_width,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM,
                                         LV_PART_MAIN | LV_STATE_DEFAULT);
            const std::uint32_t children = lv_obj_get_child_count(header);
            for (std::uint32_t index = 0; index < children; ++index) {
                lv_obj_set_style_text_color(lv_obj_get_child(header, static_cast<int32_t>(index)),
                                            lv_color_hex(palette().on_header),
                                            LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            apply_ascii_letter_spacing(header);
        }
        const std::array<lv_obj_t*, 12> header_buttons{
            objects.main_settings_button, objects.settings_button,
            objects.details_back_button, objects.settings_back_button,
            objects.month_settings_button, objects.brightness_back_button,
            objects.weather_back_button, objects.firmware_back_button,
            objects.main_wifi_button, objects.agenda_wifi_button,
            objects.month_wifi_button, objects.forecast_wifi_button};
        for (lv_obj_t* button : header_buttons) {
            if (button == nullptr) continue;
            const bool connection_control = button == objects.main_wifi_button ||
                button == objects.agenda_wifi_button || button == objects.month_wifi_button ||
                button == objects.forecast_wifi_button;
            const bool strong = !connection_control &&
                recipe().surface_grammar() == SurfaceGrammar::StrongBlocks;
            const bool ruled = recipe().surface_grammar() == SurfaceGrammar::RuledGrid;
            lv_obj_set_style_bg_color(button,
                                      lv_color_hex(strong ? palette().today_fill
                                                          : palette().action),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(button, strong ? LV_OPA_COVER : LV_OPA_TRANSP,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(button, lv_color_hex(strong ? palette().action_pressed
                                                                  : palette().action_soft),
                                      LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_bg_opa(button, LV_OPA_COVER,
                                    LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_width(
                button,
                recipe().surface_grammar() == SurfaceGrammar::OpenSurfaces
                    ? 0 : recipe().structural_border_width,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(button,
                                          lv_color_hex(ruled ? palette().action
                                                              : palette().status_normal),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_side(button, LV_BORDER_SIDE_FULL,
                                         LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(button, metrics().control_radius, LV_PART_MAIN | LV_STATE_DEFAULT);
            if (lv_obj_get_child_count(button) != 0) {
                style_button_text(button, strong ? palette().today_text : palette().on_header,
                                  strong ? palette().on_action : palette().text_primary);
            }
        }

        const std::array<lv_obj_t*, 11> content_buttons{
            objects.settings_enter_wifi_button, objects.settings_start_setup_button,
            objects.settings_sync_now_button, objects.settings_brightness_button,
            objects.settings_weather_button, objects.settings_firmware_button,
            objects.firmware_enable_button,
            objects.firmware_cancel_button, objects.firmware_reboot_button,
            objects.weather_refresh_button, objects.weather_set_location_button};
        for (lv_obj_t* button : content_buttons) {
            if (button == nullptr) continue;
            const bool open = recipe().surface_grammar() == SurfaceGrammar::OpenSurfaces;
            const bool strong = recipe().surface_grammar() == SurfaceGrammar::StrongBlocks;
            lv_obj_set_style_bg_opa(button, LV_OPA_COVER,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(button, lv_color_hex(palette().surface_subtle),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(button, lv_color_hex(palette().action_soft),
                                      LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_width(
                button, open ? 0
                             : (strong ? recipe().structural_border_width
                                       : recipe().divider_width),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(
                button, lv_color_hex(strong ? palette().text_primary : palette().divider),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_side(button, LV_BORDER_SIDE_FULL,
                                         LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(button, metrics().control_radius, LV_PART_MAIN | LV_STATE_DEFAULT);
            if (lv_obj_get_child_count(button) != 0) {
                style_button_text(button, palette().text_primary, palette().text_primary);
            }
        }

        apply_dynamic_row_appearance();
        apply_weather_visual_hierarchy();
        apply_primary_navigation_styles();
        style_calendar_period_controls();

        for (lv_obj_t* card : {objects.main_events_container, objects.main_weather_card,
                               objects.forecast_cards_container, objects.weather_current_summary_card,
                               objects.details_content,
                               objects.settings_status_card, objects.brightness_card}) {
            style_surface_card(card);
        }

        if (objects.theme_style_dropdown != nullptr) {
            apply_manual_input_appearance(objects.theme_style_dropdown);
            lv_obj_set_style_bg_color(objects.theme_style_dropdown, lv_color_hex(palette().selection),
                                      LV_PART_SELECTED | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(objects.theme_style_dropdown, lv_color_hex(palette().on_selection),
                                        LV_PART_SELECTED | LV_STATE_DEFAULT);
            style_dropdown_popup(objects.theme_style_dropdown);
        }

        if (objects.dark_theme_switch != nullptr) {
            lv_obj_set_style_bg_color(objects.dark_theme_switch, lv_color_hex(palette().surface_muted),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(objects.dark_theme_switch, lv_color_hex(palette().border),
                                          LV_PART_MAIN);
            lv_obj_set_style_border_width(objects.dark_theme_switch, 1, LV_PART_MAIN);
            lv_obj_set_style_bg_color(objects.dark_theme_switch, lv_color_hex(palette().action),
                                      LV_PART_INDICATOR | LV_STATE_CHECKED);
            lv_obj_set_style_bg_color(objects.dark_theme_switch, lv_color_hex(palette().text_secondary),
                                      LV_PART_KNOB);
            lv_obj_set_style_bg_color(objects.dark_theme_switch, lv_color_hex(palette().on_action),
                                      LV_PART_KNOB | LV_STATE_CHECKED);
        }
        if (objects.brightness_slider != nullptr) {
            lv_obj_set_style_bg_color(objects.brightness_slider, lv_color_hex(palette().surface_muted),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(objects.brightness_slider, lv_color_hex(palette().action),
                                      LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(objects.brightness_slider, lv_color_hex(palette().action),
                                      LV_PART_KNOB | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(objects.brightness_slider, lv_color_hex(palette().surface),
                                          LV_PART_KNOB);
            lv_obj_set_style_border_width(objects.brightness_slider, 2, LV_PART_KNOB);
        }
        for (lv_obj_t* control : {objects.dark_theme_switch, objects.brightness_slider}) {
            for (lv_part_t part : {LV_PART_MAIN, LV_PART_INDICATOR, LV_PART_KNOB}) {
                for (lv_state_t state : {LV_STATE_DEFAULT, LV_STATE_CHECKED, LV_STATE_PRESSED}) {
                    lv_obj_set_style_color_filter_dsc(control, nullptr, part | state);
                    lv_obj_set_style_color_filter_opa(control, LV_OPA_TRANSP, part | state);
                }
            }
        }
        apply_manual_input_appearance(display_wifi_ssid_);
        apply_manual_input_appearance(display_wifi_password_);
        apply_manual_input_appearance(display_wifi_network_picker_);
        apply_manual_input_appearance(display_wifi_keyboard_);
        apply_manual_input_appearance(weather_location_name_);
        apply_manual_input_appearance(weather_location_latitude_);
        apply_manual_input_appearance(weather_location_longitude_);
        apply_manual_input_appearance(weather_location_units_);
        apply_manual_input_appearance(weather_location_keyboard_);
        if (display_wifi_keyboard_ != nullptr) {
            lv_obj_set_style_bg_color(display_wifi_keyboard_, lv_color_hex(palette().action_soft),
                                      LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(display_wifi_keyboard_, lv_color_hex(palette().text_primary),
                                        LV_PART_ITEMS | LV_STATE_DEFAULT);
        }
        if (weather_location_keyboard_ != nullptr) {
            lv_obj_set_style_bg_color(weather_location_keyboard_, lv_color_hex(palette().action_soft),
                                      LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(weather_location_keyboard_, lv_color_hex(palette().text_primary),
                                        LV_PART_ITEMS | LV_STATE_DEFAULT);
        }
        for (lv_obj_t* ascii_group : {objects.agenda_navigation_bar, objects.day_selector,
                                      objects.agenda_primary_navigation,
                                      objects.month_navigation_bar, objects.month_weekday_header,
                                      objects.month_primary_navigation,
                                      objects.main_primary_navigation,
                                      objects.forecast_primary_navigation}) {
            apply_ascii_letter_spacing(ascii_group);
        }
        if (month_table_ != nullptr) {
            lv_obj_set_style_radius(month_table_, recipe().card_radius,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
            // Keep the 42-cell month grid at one physical pixel. A two-pixel
            // Bauhaus rule would consume useful cell space on the 480 px panel.
            lv_obj_set_style_border_width(month_table_, 1,
                                          LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(month_table_, lv_color_hex(palette().divider),
                                          LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_text_letter_space(month_table_, recipe().ascii_title_letter_space,
                                               LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_invalidate(month_table_);
        }
        render_appearance_settings();
        render_firmware_update();
        update_status_indicators(true);
    }

    void apply_dynamic_row_appearance() const {
        for (std::size_t index = 0; index < main_rows_.size(); ++index) {
            lv_obj_t* row = main_rows_[index];
            if (row == nullptr) continue;
            style_event_row(row, main_row_colors_[index], true);
            if (main_row_titles_[index] != nullptr) {
                lv_obj_set_style_text_color(main_row_titles_[index],
                                            lv_color_hex(palette().text_primary),
                                            LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            if (main_row_times_[index] != nullptr) {
                lv_obj_set_style_text_color(main_row_times_[index],
                                            lv_color_hex(palette().text_muted),
                                            LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            if (main_row_sources_[index] != nullptr) {
                lv_obj_set_style_text_color(main_row_sources_[index],
                                            lv_color_hex(palette().text_muted), LV_PART_MAIN);
            }
        }
        for (std::size_t index = 0; index < rows_.size(); ++index) {
            lv_obj_t* row = rows_[index];
            if (row == nullptr) continue;
            style_event_row(row, row_colors_[index], false);
            if (row_titles_[index] != nullptr) {
                lv_obj_set_style_text_color(row_titles_[index],
                                            lv_color_hex(palette().text_primary),
                                            LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(row_titles_[index],
                                            lv_color_hex(palette().text_primary),
                                            LV_PART_MAIN | LV_STATE_PRESSED);
            }
            if (row_times_[index] != nullptr) {
                lv_obj_set_style_text_color(row_times_[index],
                                            lv_color_hex(palette().text_muted),
                                            LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(row_times_[index],
                                            lv_color_hex(palette().text_muted),
                                            LV_PART_MAIN | LV_STATE_PRESSED);
            }
            if (row_sources_[index] != nullptr) {
                lv_obj_set_style_text_color(row_sources_[index],
                                            lv_color_hex(palette().text_muted), LV_PART_MAIN);
            }
        }
    }

    void style_icon_well(lv_obj_t* object, bool prominent) const {
        if (object == nullptr) return;
        bool filled = false;
        std::uint8_t border_width = 0;
        std::uint8_t radius = 0;
        std::uint32_t border = palette().divider;
        switch (recipe().forecast_well_grammar()) {
            case ForecastWellGrammar::CurrentOnly:
                filled = prominent;
                border_width = prominent ? recipe().divider_width : 0;
                radius = prominent ? recipe().card_radius : 0;
                break;
            case ForecastWellGrammar::None:
                break;
            case ForecastWellGrammar::GeometricCurrent:
                filled = true;
                border_width = recipe().structural_border_width;
                border = prominent ? palette().action : palette().text_primary;
                break;
            case ForecastWellGrammar::Open:
                filled = true;
                radius = recipe().card_radius;
                break;
        }
        lv_obj_set_style_bg_opa(object, filled ? LV_OPA_COVER : LV_OPA_TRANSP,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(object, lv_color_hex(palette().action_soft),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(object, border_width,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(object, lv_color_hex(border),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_side(object, LV_BORDER_SIDE_FULL,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(object, radius,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void apply_weather_visual_hierarchy() const {
        for (lv_obj_t* card : {objects.main_events_container, objects.main_weather_card,
                               objects.forecast_cards_container}) style_surface_card(card);
        style_icon_well(objects.main_weather_icon_container, true);
        style_icon_well(objects.weather_current_icon_container, true);
        const std::array<lv_obj_t*, 7> forecast_icons{
            objects.forecast_icon_container_0, objects.forecast_icon_container_1,
            objects.forecast_icon_container_2, objects.forecast_icon_container_3,
            objects.forecast_icon_container_4, objects.forecast_icon_container_5,
            objects.forecast_icon_container_6};
        lv_obj_update_layout(objects.forecast_cards_container);
        // EEZ owns the seven columns; center their shared grid inside each
        // theme's actual content width rather than its outer border rectangle.
        const int32_t grid_width = 7 * 100 + 6 * 7;
        const int32_t inset =
            (lv_obj_get_content_width(objects.forecast_cards_container) - grid_width) / 2;
        for (std::size_t index = 0; index < forecast_icons.size(); ++index) {
            const int32_t x = inset + static_cast<int32_t>(index) * 107;
            lv_obj_set_x(forecast_day_labels_[index], x);
            lv_obj_set_x(forecast_temp_labels_[index], x);
            lv_obj_set_x(forecast_condition_labels_[index], x);
            lv_obj_set_x(forecast_icons[index], x + 29);
            style_icon_well(forecast_icons[index], false);
        }

        for (lv_obj_t* label : {objects.main_weather_location_label,
                                objects.main_weather_temperature_label,
                                objects.weather_location_label, objects.weather_temperature_label}) {
            if (label != nullptr) {
                lv_obj_set_style_text_color(label, lv_color_hex(palette().text_primary), LV_PART_MAIN);
            }
        }
        for (lv_obj_t* label : {objects.main_weather_updated_label, objects.weather_updated_label,
                                objects.weather_state_label, objects.weather_attribution_label}) {
            if (label != nullptr) {
                lv_obj_set_style_text_color(label, lv_color_hex(palette().text_muted), LV_PART_MAIN);
            }
        }
        for (lv_obj_t* label : forecast_day_labels_) {
            if (label != nullptr) {
                lv_obj_set_style_text_color(label, lv_color_hex(palette().text_primary), LV_PART_MAIN);
            }
        }
        for (lv_obj_t* label : forecast_condition_labels_) {
            if (label != nullptr) {
                lv_obj_set_style_text_color(label, lv_color_hex(palette().text_muted), LV_PART_MAIN);
            }
        }
    }

    void apply_primary_navigation_styles() const {
        const auto style_group = [this](lv_obj_t* bar, lv_obj_t* main, lv_obj_t* calendar,
                                        lv_obj_t* forecast, lv_obj_t* indicator,
                                        PrimaryPage selected) {
            if (bar != nullptr) {
                lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, LV_PART_MAIN);
                const bool ruled = recipe().navigation_grammar() == NavigationGrammar::RuledUnderline ||
                                    recipe().navigation_grammar() == NavigationGrammar::FilledTabAndUnderline;
                lv_obj_set_style_border_color(bar,
                                              lv_color_hex(ruled ? palette().action
                                                                  : palette().divider),
                                              LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_width(bar, recipe().structural_border_width,
                                              LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP,
                                             LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            const std::array<std::pair<lv_obj_t*, PrimaryPage>, 3> buttons{{
                {main, PrimaryPage::Main}, {calendar, PrimaryPage::Calendar},
                {forecast, PrimaryPage::Forecast}}};
            lv_obj_t* active_button = nullptr;
            for (const auto& entry : buttons) {
                lv_obj_t* button = entry.first;
                if (button == nullptr) continue;
                const bool active = entry.second == selected;
                if (active) active_button = button;
                const bool filled = active &&
                    recipe().navigation_grammar() == NavigationGrammar::FilledTabAndUnderline;
                const bool soft = active &&
                    recipe().navigation_grammar() == NavigationGrammar::SoftSelectedTab;
                lv_obj_set_style_bg_color(button,
                                          lv_color_hex(filled ? palette().selection
                                                              : palette().action_soft),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(button, (filled || soft) ? LV_OPA_COVER : LV_OPA_TRANSP,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(button, lv_color_hex(palette().action_soft),
                                          LV_PART_MAIN | LV_STATE_PRESSED);
                lv_obj_set_style_bg_opa(button, LV_OPA_COVER,
                                        LV_PART_MAIN | LV_STATE_PRESSED);
                lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
                lv_obj_set_style_radius(button, metrics().control_radius, LV_PART_MAIN);
                if (lv_obj_get_child_count(button) != 0) {
                    lv_obj_t* label = lv_obj_get_child(button, 0);
                    set_compact_ascii_font(label);
                    lv_obj_set_style_text_letter_space(label, recipe().ascii_title_letter_space,
                                                       LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                    const std::uint32_t active_color = filled
                        ? palette().on_selection
                        : (soft ? palette().text_primary : palette().action);
                    style_button_text(button, active ? active_color : palette().text_muted,
                                      palette().text_primary);
                }
            }
            if (indicator != nullptr) {
                lv_obj_remove_flag(indicator, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_remove_flag(indicator, LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_set_style_bg_color(indicator, lv_color_hex(palette().action),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(indicator, LV_OPA_COVER,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_width(indicator, 0, LV_PART_MAIN);
                const bool rounded = recipe().navigation_grammar() == NavigationGrammar::AccentUnderline ||
                                     recipe().navigation_grammar() == NavigationGrammar::SoftSelectedTab;
                lv_obj_set_style_radius(indicator,
                                        rounded ? recipe().nav_indicator_height / 2U : 0,
                                        LV_PART_MAIN);
                lv_obj_set_size(indicator, recipe().nav_indicator_width,
                                recipe().nav_indicator_height);
                if (bar != nullptr && active_button != nullptr) {
                    lv_obj_update_layout(bar);
                    const int32_t x = lv_obj_get_x(active_button) +
                        (lv_obj_get_width(active_button) - recipe().nav_indicator_width) / 2;
                    const int32_t y = lv_obj_get_height(bar) - recipe().nav_indicator_height;
                    lv_obj_set_pos(indicator, x, y);
                }
            }
        };
        style_group(objects.main_primary_navigation, objects.main_nav_main_button,
                    objects.main_nav_calendar_button, objects.main_nav_forecast_button,
                    objects.main_nav_indicator,
                    PrimaryPage::Main);
        style_group(objects.agenda_primary_navigation, objects.agenda_nav_main_button,
                    objects.agenda_nav_calendar_button, objects.agenda_nav_forecast_button,
                    objects.agenda_nav_indicator,
                    PrimaryPage::Calendar);
        style_group(objects.month_primary_navigation, objects.month_nav_main_button,
                    objects.month_nav_calendar_button, objects.month_nav_forecast_button,
                    objects.month_nav_indicator,
                    PrimaryPage::Calendar);
        style_group(objects.forecast_primary_navigation, objects.forecast_nav_main_button,
                    objects.forecast_nav_calendar_button, objects.forecast_nav_forecast_button,
                    objects.forecast_nav_indicator,
                    PrimaryPage::Forecast);
    }

    void apply_manual_input_appearance(lv_obj_t* object) const {
        if (object == nullptr) return;
        lv_obj_set_style_bg_color(object, lv_color_hex(palette().input_inset), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(object, lv_color_hex(palette().text_primary), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(object, lv_color_hex(palette().border), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(object, recipe().divider_width, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_side(object, LV_BORDER_SIDE_FULL, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(object, metrics().input_radius, LV_PART_MAIN | LV_STATE_DEFAULT);
        if (lv_obj_check_type(object, &lv_dropdown_class) ||
            lv_obj_check_type(object, &lv_textarea_class)) {
            // LVGL one-line textareas otherwise replace the requested height
            // with content sizing. Reserve the bundled 24 px font line inside
            // a stable 44 px target, including a two-pixel focused border.
            lv_obj_set_style_pad_all(object, 8, LV_PART_MAIN);
            lv_obj_set_height(object, 44);
        }
        if (lv_obj_check_type(object, &lv_dropdown_class)) {
            lv_obj_set_style_bg_color(object, lv_color_hex(palette().selection),
                                      LV_PART_SELECTED | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(object, lv_color_hex(palette().on_selection),
                                        LV_PART_SELECTED | LV_STATE_DEFAULT);
            style_dropdown_popup(object);
        }
        if (lv_obj_check_type(object, &lv_textarea_class)) {
            lv_obj_set_style_text_color(object, lv_color_hex(palette().text_secondary),
                                        LV_PART_TEXTAREA_PLACEHOLDER | LV_STATE_DEFAULT);
            lv_obj_set_style_outline_color(object, lv_color_hex(palette().focus),
                                           LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_outline_width(object, recipe().focus_width,
                                           LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_outline_pad(object, 2,
                                         LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_outline_opa(object, LV_OPA_COVER,
                                         LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_border_color(object, lv_color_hex(palette().focus),
                                          LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_border_width(
                object, std::max<std::uint8_t>(recipe().divider_width,
                                               recipe().structural_border_width),
                                          LV_PART_MAIN | LV_STATE_FOCUSED);
        }
    }

    static void on_dropdown_opened(lv_event_t* event) {
        instance().style_dropdown_popup(
            static_cast<lv_obj_t*>(lv_event_get_target(event)));
    }

    void commit_pending_appearance() {
        if (!appearance_save_pending_) return;
        appearance_save_pending_ = false;
        if (appearance_.theme_id == persisted_appearance_.theme_id &&
            appearance_.dark_theme == persisted_appearance_.dark_theme &&
            appearance_.brightness_percent == persisted_appearance_.brightness_percent) {
            return;
        }
        if (save_appearance_preferences(appearance_)) {
            persisted_appearance_ = appearance_;
        }
    }

    void set_label_text_if_changed(lv_obj_t* label, const char* text) const {
        if (label == nullptr || text == nullptr) return;
        const char* current = lv_label_get_text(label);
        if (current == nullptr || std::strcmp(current, text) != 0) {
            lv_label_set_text(label, text);
        }
    }

    void set_dynamic_font(lv_obj_t* object) const {
        if (object == nullptr) return;
        lv_obj_set_style_text_font(object, &lv_font_dejavu_16_persian_hebrew, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_base_dir(object, LV_BASE_DIR_AUTO, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void set_emphasis_font(lv_obj_t* object) const {
        if (object == nullptr) return;
        lv_obj_set_style_text_font(object, &lv_font_montserrat_20,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void set_large_time_font(lv_obj_t* object) const {
        if (object == nullptr) return;
        lv_obj_set_style_text_font(object, &lv_font_montserrat_28,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_base_dir(object, LV_BASE_DIR_LTR,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void set_large_temperature_font(lv_obj_t* object) const {
        if (object == nullptr) return;
        const char* value = lv_label_get_text(object);
        const lv_font_t* font = &lv_font_montserrat_32;
        const int32_t width = lv_obj_get_width(object);
        if (lv_text_get_width(value, std::strlen(value), font, 0) > width) {
            font = &lv_font_montserrat_28;
        }
        if (lv_text_get_width(value, std::strlen(value), font, 0) > width) {
            font = &lv_font_montserrat_20;
        }
        lv_obj_set_style_text_font(object, font,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_base_dir(object, LV_BASE_DIR_LTR,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void set_compact_ascii_font(lv_obj_t* object) const {
        if (object == nullptr) return;
        lv_obj_set_style_text_font(object, &lv_font_montserrat_16,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_base_dir(object, LV_BASE_DIR_LTR,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void set_date_navigation_enabled(bool enabled) const {
        const std::array<lv_obj_t*, 6> controls{
            objects.agenda_previous_button, objects.agenda_today_button,
            objects.agenda_next_button, objects.month_previous_button,
            objects.month_today_button, objects.month_next_button};
        for (lv_obj_t* control : controls) {
            if (control == nullptr) continue;
            if (enabled) {
                lv_obj_remove_state(control, LV_STATE_DISABLED);
            } else {
                lv_obj_add_state(control, LV_STATE_DISABLED);
            }
        }
    }

    void style_calendar_period_controls() const {
        for (lv_obj_t* button : {objects.agenda_previous_button, objects.agenda_today_button,
                                 objects.agenda_next_button, objects.agenda_week_button,
                                 objects.agenda_month_button, objects.month_previous_button,
                                 objects.month_today_button, objects.month_next_button,
                                 objects.month_week_button, objects.month_month_button}) {
            const bool week = button == objects.agenda_week_button || button == objects.month_week_button;
            const bool month = button == objects.agenda_month_button || button == objects.month_month_button;
            const bool selected = (week && navigation_.view_mode() == CalendarViewMode::Week) ||
                                  (month && navigation_.view_mode() == CalendarViewMode::Month);
            lv_obj_set_style_bg_color(button,
                lv_color_hex(selected ? palette().action_soft : palette().surface_subtle), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_bg_color(button, lv_color_hex(palette().action_soft),
                                      LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_color(button,
                lv_color_hex(selected ? palette().focus : palette().divider), LV_PART_MAIN);
            lv_obj_set_style_border_width(button, selected ? 2 : 1, LV_PART_MAIN);
            lv_obj_set_style_radius(button, recipe().control_radius, LV_PART_MAIN);
            style_button_text(button, palette().text_primary, palette().text_primary);
        }
    }

    void set_provider_header(lv_obj_t* label) const {
        const board::ConnectivityStatus connectivity = board::connectivity_service().status();
        const ProviderResult provider = provider_.current();
        const CalendarStatusPresentation status = derive_calendar_status(
            connectivity, provider.state,
            provider.snapshot != nullptr || !cached_calendar_document_.empty());
        set_label_text_if_changed(label, calendar_freshness_text(status.freshness, connectivity).c_str());
        lv_obj_set_style_text_color(label, lv_color_hex(status_color(status.freshness.severity)),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        set_dynamic_font(label);
    }

    std::string calendar_freshness_text(const UiStatusPresentation& freshness,
                                         const board::ConnectivityStatus& connectivity) const {
        if (connectivity.last_successful_import_utc <= 0 ||
            (freshness.severity != UiStatusSeverity::Normal &&
             std::strstr(freshness.label, "saved events") == nullptr &&
             std::strcmp(freshness.label, "Offline copy") != 0)) return freshness.label;
        std::tm synced{};
        if (!local_time(connectivity.last_successful_import_utc, synced)) return freshness.label;
        const char* prefix = freshness.severity == UiStatusSeverity::Normal ? "Synced"
            : (freshness.severity == UiStatusSeverity::Error
                ? (connectivity.fetch_state == board::CalendarFetchState::Failed
                    ? "Sync failed - saved" : "Import failed - saved") : "Offline - saved");
        char message[64];
        const CivilDate synced_day = date_from_tm(synced);
        if (synced_day == navigation_.current_day()) {
            std::snprintf(message, sizeof(message), "%s %02d:%02d", prefix,
                          synced.tm_hour, synced.tm_min);
        } else {
            std::snprintf(message, sizeof(message), "%s %02d/%02d %02d:%02d", prefix,
                          synced.tm_mday, synced.tm_mon + 1, synced.tm_hour, synced.tm_min);
        }
        return message;
    }

    std::uint32_t status_color(UiStatusSeverity severity) const {
        switch (severity) {
            case UiStatusSeverity::Normal: return palette().status_normal;
            case UiStatusSeverity::Attention: return palette().status_attention;
            case UiStatusSeverity::Error: return palette().status_error;
            case UiStatusSeverity::Neutral: return palette().on_header;
        }
        return palette().text_muted;
    }

    void update_status_indicator(lv_obj_t* icon, lv_obj_t* label,
                                 const UiStatusPresentation& presentation) const {
        set_label_text_if_changed(icon, LV_SYMBOL_WIFI);
        set_label_text_if_changed(label, presentation.label);
        style_button_text(lv_obj_get_parent(label), status_color(presentation.severity),
                          palette().text_primary);
        set_compact_ascii_font(icon);
        set_compact_ascii_font(label);
        lv_obj_set_height(icon, LV_SIZE_CONTENT);
        lv_obj_set_height(label, LV_SIZE_CONTENT);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 8, 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 36, 0);
    }

    void update_status_indicators(bool force = false) {
        const std::uint32_t now = millis();
        if (!force && status_render_time_valid_ &&
            static_cast<std::uint32_t>(now - last_status_render_ms_) < kStatusRefreshIntervalMs) {
            return;
        }
        last_status_render_ms_ = now;
        status_render_time_valid_ = true;
        const board::ConnectivityStatus connectivity = board::connectivity_service().status();
        const ProviderResult provider = provider_.current();
        const CalendarStatusPresentation status = derive_calendar_status(
            connectivity, provider.state,
            provider.snapshot != nullptr || !cached_calendar_document_.empty());
        if (!force && last_connection_status_label_ == status.connection.label &&
            last_calendar_status_label_ == status.freshness.label &&
            last_status_success_utc_ == connectivity.last_successful_import_utc &&
            last_status_day_ == navigation_.current_day() &&
            last_status_theme_id_ == appearance_.theme_id &&
            last_status_dark_theme_ == appearance_.dark_theme) return;
        last_connection_status_label_ = status.connection.label;
        last_calendar_status_label_ = status.freshness.label;
        last_status_success_utc_ = connectivity.last_successful_import_utc;
        last_status_day_ = navigation_.current_day();
        last_status_theme_id_ = appearance_.theme_id;
        last_status_dark_theme_ = appearance_.dark_theme;
        update_status_indicator(objects.main_wifi_icon, objects.main_wifi_label, status.connection);
        update_status_indicator(objects.agenda_wifi_icon, objects.agenda_wifi_label, status.connection);
        update_status_indicator(objects.month_wifi_icon, objects.month_wifi_label, status.connection);
        update_status_indicator(objects.forecast_wifi_icon, objects.forecast_wifi_label, status.connection);
        const std::string freshness_text = calendar_freshness_text(status.freshness, connectivity);
        for (lv_obj_t* freshness : {objects.main_sync_state_label, objects.sync_state_label,
                                    objects.month_sync_state_label, objects.forecast_sync_state_label}) {
            set_label_text_if_changed(freshness, freshness_text.c_str());
            lv_obj_set_style_text_color(freshness, lv_color_hex(status_color(status.freshness.severity)),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            set_dynamic_font(freshness);
        }
    }

    static void clear_sensitive_text(char* data, std::size_t size) {
        volatile char* volatile_data = data;
        for (std::size_t index = 0; index < size; ++index) volatile_data[index] = '\0';
    }

    void set_settings_shell_visible(bool visible) {
        const std::array<lv_obj_t*, 7> settings_shell{
            objects.settings_status_card, objects.settings_enter_wifi_button,
            objects.settings_start_setup_button, objects.settings_sync_now_button,
            objects.settings_brightness_button, objects.settings_weather_button,
            objects.settings_firmware_button};
        for (lv_obj_t* object : settings_shell) {
            if (object == nullptr) continue;
            if (visible) {
                lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    lv_obj_t* create_display_wifi_button(lv_obj_t* parent, const char* text,
                                         lv_coord_t x, lv_coord_t y, lv_coord_t width,
                                         lv_event_cb_t callback, std::uint32_t color) {
        const bool prominent = color == palette().action;
        const bool open = recipe().surface_grammar() == SurfaceGrammar::OpenSurfaces;
        const bool strong = recipe().surface_grammar() == SurfaceGrammar::StrongBlocks;
        lv_obj_t* button = lv_button_create(parent);
        lv_obj_set_pos(button, x, y);
        lv_obj_set_size(button, width, 44);
        lv_obj_set_style_bg_color(button,
                                  lv_color_hex(prominent ? palette().action
                                                         : palette().surface_subtle),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(button,
                                  lv_color_hex(prominent ? palette().action_pressed
                                                         : palette().action_soft),
                                  LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_border_width(
            button, prominent || open ? 0
                                      : (strong ? recipe().structural_border_width
                                                : recipe().divider_width),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(
            button, lv_color_hex(strong ? palette().text_primary : palette().divider),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_side(button, LV_BORDER_SIDE_FULL,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(button, metrics().control_radius, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);
        lv_obj_t* label = lv_label_create(button);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_color(label,
                                    lv_color_hex(prominent ? palette().on_action
                                                           : palette().text_primary),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        set_dynamic_font(label);
        lv_obj_center(label);
        style_button_text(button, prominent ? palette().on_action : palette().text_primary,
                          prominent ? palette().on_action : palette().text_primary);
        return button;
    }

    void create_display_wifi_form() {
        display_wifi_form_ = lv_obj_create(objects.settings_screen);
        lv_obj_set_pos(display_wifi_form_, 0, 64);
        lv_obj_set_size(display_wifi_form_, 800, 416);
        // Keep the overlay clickable so taps in its blank areas cannot reach
        // the generated Settings controls beneath it.
        lv_obj_remove_flag(display_wifi_form_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(display_wifi_form_, lv_color_hex(palette().canvas), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(display_wifi_form_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(display_wifi_form_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t* title = lv_label_create(display_wifi_form_);
        lv_obj_set_pos(title, 20, 8);
        lv_obj_set_size(title, 420, 24);
        lv_label_set_text(title, "Enter home Wi-Fi");
        lv_obj_set_style_text_color(title, lv_color_hex(palette().text_primary), LV_PART_MAIN | LV_STATE_DEFAULT);
        set_dynamic_font(title);

        display_wifi_ssid_ = lv_textarea_create(display_wifi_form_);
        lv_obj_set_pos(display_wifi_ssid_, 20, 36);
        lv_obj_set_size(display_wifi_ssid_, 490, 44);
        lv_textarea_set_one_line(display_wifi_ssid_, true);
        lv_textarea_set_max_length(display_wifi_ssid_, 32);
        lv_textarea_set_placeholder_text(display_wifi_ssid_, "Network name (SSID)");
        set_dynamic_font(display_wifi_ssid_);
        lv_obj_add_event_cb(display_wifi_ssid_, on_display_wifi_field_clicked, LV_EVENT_CLICKED, nullptr);

        display_wifi_scan_button_ = create_display_wifi_button(display_wifi_form_, "Scan Wi-Fi", 522, 36, 124,
                                                                 on_display_wifi_scan_clicked, palette().action);
        create_display_wifi_button(display_wifi_form_, "Cancel", 658, 36, 122,
                                   on_display_wifi_cancel_clicked, palette().text_muted);

        display_wifi_password_ = lv_textarea_create(display_wifi_form_);
        lv_obj_set_pos(display_wifi_password_, 20, 90);
        lv_obj_set_size(display_wifi_password_, 760, 44);
        lv_textarea_set_one_line(display_wifi_password_, true);
        lv_textarea_set_max_length(display_wifi_password_, 63);
        lv_textarea_set_password_mode(display_wifi_password_, true);
        lv_textarea_set_password_show_time(display_wifi_password_, 0);
        lv_textarea_set_placeholder_text(display_wifi_password_, "Password");
        set_dynamic_font(display_wifi_password_);
        lv_obj_add_event_cb(display_wifi_password_, on_display_wifi_field_clicked, LV_EVENT_CLICKED, nullptr);

        display_wifi_feedback_ = lv_label_create(display_wifi_form_);
        lv_obj_set_pos(display_wifi_feedback_, 20, 136);
        lv_obj_set_size(display_wifi_feedback_, 760, 24);
        lv_label_set_text(display_wifi_feedback_, "Credentials are stored on this display and are never shown again.");
        lv_label_set_long_mode(display_wifi_feedback_, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(display_wifi_feedback_, lv_color_hex(palette().text_muted), LV_PART_MAIN | LV_STATE_DEFAULT);
        set_dynamic_font(display_wifi_feedback_);

        display_wifi_network_picker_ = lv_dropdown_create(display_wifi_form_);
        lv_obj_set_pos(display_wifi_network_picker_, 20, 168);
        lv_obj_set_size(display_wifi_network_picker_, 610, 44);
        lv_dropdown_set_text(display_wifi_network_picker_, "Nearby Wi-Fi appears here after a scan");
        lv_dropdown_set_options(display_wifi_network_picker_, "");
        set_dynamic_font(display_wifi_network_picker_);
        lv_obj_add_event_cb(display_wifi_network_picker_, on_display_wifi_network_selected,
                            LV_EVENT_VALUE_CHANGED, nullptr);
        lv_obj_add_event_cb(display_wifi_network_picker_, on_dropdown_opened,
                            LV_EVENT_READY, nullptr);
        lv_obj_add_flag(display_wifi_network_picker_, LV_OBJ_FLAG_HIDDEN);

        create_display_wifi_button(display_wifi_form_, "Connect", 650, 168, 130,
                                   on_display_wifi_connect_clicked, palette().action);

        display_wifi_keyboard_ = lv_keyboard_create(display_wifi_form_);
        lv_obj_set_size(display_wifi_keyboard_, 776, 188);
        // Bottom-align with a physical margin. This reserves enough height for
        // all four LVGL text-keyboard rows on the 800 x 480 panel.
        lv_obj_align(display_wifi_keyboard_, LV_ALIGN_BOTTOM_MID, 0, -8);
        lv_keyboard_set_mode(display_wifi_keyboard_, LV_KEYBOARD_MODE_TEXT_LOWER);
        lv_keyboard_set_popovers(display_wifi_keyboard_, false);
        lv_obj_add_event_cb(display_wifi_keyboard_, on_display_wifi_keyboard_ready,
                            LV_EVENT_READY, nullptr);
        lv_obj_add_event_cb(display_wifi_keyboard_, on_display_wifi_cancel_clicked,
                            LV_EVENT_CANCEL, nullptr);
        lv_obj_set_style_pad_top(display_wifi_keyboard_, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(display_wifi_keyboard_, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(display_wifi_keyboard_, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(display_wifi_keyboard_, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_row(display_wifi_keyboard_, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_column(display_wifi_keyboard_, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
        set_dynamic_font(display_wifi_keyboard_);
        lv_obj_move_foreground(display_wifi_keyboard_);

        // These controls are created after the generated EEZ shell. Style them
        // explicitly so they do not retain LVGL's light default theme in dark
        // mode (and remain consistent after later theme changes).
        apply_manual_input_appearance(display_wifi_ssid_);
        apply_manual_input_appearance(display_wifi_password_);
        apply_manual_input_appearance(display_wifi_network_picker_);
        apply_manual_input_appearance(display_wifi_keyboard_);
        for (lv_obj_t* field : {display_wifi_ssid_, display_wifi_password_}) {
            lv_obj_set_style_outline_color(field, lv_color_hex(palette().focus),
                                            LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_outline_width(field, recipe().focus_width,
                                           LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_outline_pad(field, 2,
                                         LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_outline_opa(field, LV_OPA_COVER,
                                         LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_border_color(field, lv_color_hex(palette().focus),
                                          LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_border_width(
                field, std::max<std::uint8_t>(recipe().divider_width,
                                              recipe().structural_border_width),
                                          LV_PART_MAIN | LV_STATE_FOCUSED);
        }
        lv_obj_set_style_bg_color(display_wifi_keyboard_, lv_color_hex(palette().action_soft),
                                  LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(display_wifi_keyboard_, lv_color_hex(palette().text_primary),
                                    LV_PART_ITEMS | LV_STATE_DEFAULT);

        lv_obj_add_flag(display_wifi_form_, LV_OBJ_FLAG_HIDDEN);
    }

    void clear_display_wifi_form() {
        if (display_wifi_ssid_ != nullptr) lv_textarea_set_text(display_wifi_ssid_, "");
        if (display_wifi_password_ != nullptr) lv_textarea_set_text(display_wifi_password_, "");
        if (display_wifi_feedback_ != nullptr) {
            lv_label_set_text(display_wifi_feedback_, "Credentials are stored on this display and are never shown again.");
        }
        if (display_wifi_network_picker_ != nullptr) {
            lv_dropdown_set_text(display_wifi_network_picker_, "Nearby Wi-Fi appears here after a scan");
            lv_dropdown_set_options(display_wifi_network_picker_, "");
            lv_obj_add_flag(display_wifi_network_picker_, LV_OBJ_FLAG_HIDDEN);
        }
        clear_sensitive_text(pending_display_wifi_ssid_.data(), pending_display_wifi_ssid_.size());
        clear_sensitive_text(pending_display_wifi_password_.data(), pending_display_wifi_password_.size());
        pending_display_wifi_save_ = false;
        pending_display_wifi_scan_ = false;
        display_wifi_scan_count_ = 0;
        display_wifi_scan_results_rendered_ = false;
    }

    void hide_display_wifi_form() {
        if (display_wifi_form_ == nullptr || !display_wifi_form_visible_) return;
        clear_display_wifi_form();
        lv_keyboard_set_textarea(display_wifi_keyboard_, nullptr);
        for (lv_obj_t* field : {display_wifi_ssid_, display_wifi_password_}) {
            if (field != nullptr) lv_obj_remove_state(field, LV_STATE_FOCUSED);
        }
        lv_obj_add_flag(display_wifi_form_, LV_OBJ_FLAG_HIDDEN);
        display_wifi_form_visible_ = false;
        set_settings_shell_visible(true);
    }

    void destroy_display_wifi_form() {
        if (display_wifi_form_ == nullptr) return;
        lv_obj_delete(display_wifi_form_);
        display_wifi_form_ = nullptr;
        display_wifi_ssid_ = nullptr;
        display_wifi_password_ = nullptr;
        display_wifi_feedback_ = nullptr;
        display_wifi_keyboard_ = nullptr;
        display_wifi_scan_button_ = nullptr;
        display_wifi_network_picker_ = nullptr;
        display_wifi_form_visible_ = false;
    }

    void select_display_wifi_field(lv_obj_t* field) {
        if (!display_wifi_form_visible_ || field == nullptr) return;
        for (lv_obj_t* candidate : {display_wifi_ssid_, display_wifi_password_}) {
            if (candidate != nullptr) lv_obj_remove_state(candidate, LV_STATE_FOCUSED);
        }
        lv_keyboard_set_textarea(display_wifi_keyboard_, field);
        lv_obj_add_state(field, LV_STATE_FOCUSED);
        lv_label_set_text(display_wifi_feedback_,
                          field == display_wifi_ssid_
                              ? "Typing in: network name (highlighted box)"
                              : "Typing in: password (highlighted box)");
    }

    void queue_display_wifi_save() {
        if (!display_wifi_form_visible_ || pending_display_wifi_save_) return;
        const char* ssid = lv_textarea_get_text(display_wifi_ssid_);
        const char* password = lv_textarea_get_text(display_wifi_password_);
        if (ssid == nullptr || ssid[0] == '\0') {
            lv_label_set_text(display_wifi_feedback_, "Enter a Wi-Fi network name.");
            select_display_wifi_field(display_wifi_ssid_);
            return;
        }
        // LVGL's textarea limit is character-oriented. The Wi-Fi/NVS boundary
        // is byte-oriented, so reject (rather than truncate) multibyte input
        // that would exceed the 802.11/NVS limits.
        if (std::strlen(ssid) > 32 || (password != nullptr && std::strlen(password) > 63)) {
            lv_label_set_text(display_wifi_feedback_, "Wi-Fi name/password is too long.");
            return;
        }
        std::snprintf(pending_display_wifi_ssid_.data(), pending_display_wifi_ssid_.size(), "%s", ssid);
        std::snprintf(pending_display_wifi_password_.data(), pending_display_wifi_password_.size(), "%s",
                      password == nullptr ? "" : password);
        // Do not retain the password in the LVGL textarea while the board
        // service writes Preferences on the next normal UI tick.
        lv_textarea_set_text(display_wifi_password_, "");
        lv_label_set_text(display_wifi_feedback_, "Saving Wi-Fi credentials...");
        pending_display_wifi_save_ = true;
    }

    void queue_display_wifi_scan() {
        if (!display_wifi_form_visible_ || pending_display_wifi_scan_) return;
        pending_display_wifi_scan_ = true;
        lv_label_set_text(display_wifi_feedback_, "Scanning nearby Wi-Fi...");
    }

    void commit_pending_display_wifi_scan() {
        if (!pending_display_wifi_scan_) return;
        pending_display_wifi_scan_ = false;
        const board::WifiScanRequestResult result =
            board::connectivity_service().request_wifi_scan();
        if (!display_wifi_form_visible_) return;
        switch (result) {
            case board::WifiScanRequestResult::Queued:
            case board::WifiScanRequestResult::AlreadyPending:
                lv_label_set_text(display_wifi_feedback_, "Scanning nearby Wi-Fi...");
                break;
            case board::WifiScanRequestResult::ServiceStarting:
                lv_label_set_text(display_wifi_feedback_,
                                  "Wi-Fi starts after the display check. Saved Wi-Fi reconnects automatically.");
                break;
            case board::WifiScanRequestResult::CalendarBusy:
                lv_label_set_text(display_wifi_feedback_,
                                  "Connected using saved Wi-Fi. Scan after the calendar download finishes.");
                break;
            case board::WifiScanRequestResult::WeatherBusy:
                lv_label_set_text(display_wifi_feedback_,
                                  "Connected using saved Wi-Fi. Scan after the weather update finishes.");
                break;
            case board::WifiScanRequestResult::OtaBusy:
                lv_label_set_text(display_wifi_feedback_,
                                  "Wi-Fi scan waits until the firmware update finishes.");
                break;
        }
    }

    void refresh_display_wifi_scan_results() {
        const board::WifiScanResults results = board::connectivity_service().wifi_scan_results();
        if (display_wifi_scan_results_rendered_ && results.generation == display_wifi_scan_generation_) return;
        display_wifi_scan_results_rendered_ = true;
        display_wifi_scan_generation_ = results.generation;
        if (results.in_progress) {
            lv_label_set_text(display_wifi_feedback_, "Scanning nearby Wi-Fi...");
            return;
        }
        if (!results.completed) return;

        display_wifi_scan_count_ = results.count;
        if (results.failed) {
            lv_obj_add_flag(display_wifi_network_picker_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(display_wifi_feedback_, "Wi-Fi scan failed. Try again or type the network name.");
            return;
        }
        if (results.count == 0) {
            lv_obj_add_flag(display_wifi_network_picker_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(display_wifi_feedback_, "No nearby Wi-Fi found. You can type a hidden network name.");
            return;
        }

        std::array<char, board::kMaxWifiScanNetworks * 34 + 1> options{};
        std::size_t used = 0;
        for (std::size_t index = 0; index < results.count; ++index) {
            const int written = std::snprintf(options.data() + used, options.size() - used, "%s%s",
                                              index == 0 ? "" : "\n", results.networks[index].ssid);
            if (written < 0 || static_cast<std::size_t>(written) >= options.size() - used) break;
            used += static_cast<std::size_t>(written);
        }
        lv_dropdown_set_text(display_wifi_network_picker_, nullptr);
        lv_dropdown_set_options(display_wifi_network_picker_, options.data());
        lv_obj_remove_flag(display_wifi_network_picker_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(display_wifi_feedback_, "%u nearby Wi-Fi network(s) found. Choose one or type a name.",
                              static_cast<unsigned>(results.count));
    }

    void select_display_wifi_network() {
        if (!display_wifi_form_visible_ || display_wifi_scan_count_ == 0) return;
        const std::uint32_t selected = lv_dropdown_get_selected(display_wifi_network_picker_);
        if (selected >= display_wifi_scan_count_) return;
        std::array<char, 33> ssid{};
        lv_dropdown_get_selected_str(display_wifi_network_picker_, ssid.data(), ssid.size());
        if (ssid[0] == '\0') return;
        lv_textarea_set_text(display_wifi_ssid_, ssid.data());
        select_display_wifi_field(display_wifi_password_);
    }

    void commit_pending_display_wifi() {
        if (!pending_display_wifi_save_) return;
        pending_display_wifi_save_ = false;
        const bool saved = board::connectivity_service().save_display_wifi_credentials(
            pending_display_wifi_ssid_.data(), pending_display_wifi_password_.data());
        clear_sensitive_text(pending_display_wifi_ssid_.data(), pending_display_wifi_ssid_.size());
        clear_sensitive_text(pending_display_wifi_password_.data(), pending_display_wifi_password_.size());
        if (saved) {
            hide_display_wifi_form();
            if (lv_screen_active() == objects.settings_screen) render_settings();
            return;
        }
        if (display_wifi_form_visible_) {
            lv_label_set_text(display_wifi_feedback_, "Could not save Wi-Fi. Check the name/password or wait for sync.");
        }
    }

    static void on_display_wifi_field_clicked(lv_event_t* event) {
        instance().select_display_wifi_field(static_cast<lv_obj_t*>(lv_event_get_target(event)));
    }

    static void on_display_wifi_connect_clicked(lv_event_t* event) {
        (void)event;
        instance().queue_display_wifi_save();
    }

    static void on_display_wifi_keyboard_ready(lv_event_t* event) {
        auto& controller = instance();
        if (!controller.display_wifi_form_visible_) return;
        lv_obj_t* field = lv_keyboard_get_textarea(
            static_cast<lv_obj_t*>(lv_event_get_target(event)));
        if (field == controller.display_wifi_ssid_) {
            const char* ssid = lv_textarea_get_text(controller.display_wifi_ssid_);
            if (ssid == nullptr || ssid[0] == '\0') {
                lv_label_set_text(controller.display_wifi_feedback_,
                                  "Enter a Wi-Fi network name.");
                return;
            }
            controller.select_display_wifi_field(controller.display_wifi_password_);
        } else if (field == controller.display_wifi_password_) {
            controller.queue_display_wifi_save();
        }
    }

    static void on_display_wifi_scan_clicked(lv_event_t* event) {
        (void)event;
        instance().queue_display_wifi_scan();
    }

    static void on_display_wifi_network_selected(lv_event_t* event) {
        (void)event;
        instance().select_display_wifi_network();
    }

    static void on_display_wifi_cancel_clicked(lv_event_t* event) {
        (void)event;
        instance().hide_display_wifi_form();
    }

    void create_weather_location_form() {
        weather_location_form_ = lv_obj_create(objects.weather_screen);
        if (weather_location_form_ == nullptr) return;
        lv_obj_set_pos(weather_location_form_, 0, 64);
        lv_obj_set_size(weather_location_form_, 800, 416);
        lv_obj_remove_flag(weather_location_form_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(weather_location_form_, lv_color_hex(palette().canvas),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(weather_location_form_, 0,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(weather_location_form_, 0,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t* title = lv_label_create(weather_location_form_);
        lv_obj_set_pos(title, 20, 8);
        lv_obj_set_size(title, 420, 24);
        lv_label_set_text(title, "Weather location");
        lv_obj_set_style_text_color(title, lv_color_hex(palette().text_primary),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        set_dynamic_font(title);

        for (const auto& caption : std::array<std::pair<const char*, int32_t>, 4>{{
                 {"Location", 20}, {"Latitude", 218}, {"Longitude", 363}, {"Units", 508}}}) {
            lv_obj_t* label = lv_label_create(weather_location_form_);
            lv_label_set_text(label, caption.first);
            set_dynamic_font(label);
            lv_obj_set_style_text_color(label, lv_color_hex(palette().text_secondary), LV_PART_MAIN);
            lv_obj_set_pos(label, caption.second, 34);
            lv_obj_set_size(label, 135, 24);
        }
        weather_location_name_ = lv_textarea_create(weather_location_form_);
        lv_obj_set_pos(weather_location_name_, 20, 60);
        lv_obj_set_size(weather_location_name_, 188, 44);
        lv_textarea_set_one_line(weather_location_name_, true);
        lv_textarea_set_max_length(weather_location_name_, 48);
        lv_textarea_set_placeholder_text(weather_location_name_, "Location label");
        lv_obj_add_event_cb(weather_location_name_, on_weather_location_field_clicked,
                            LV_EVENT_CLICKED, nullptr);

        weather_location_latitude_ = lv_textarea_create(weather_location_form_);
        lv_obj_set_pos(weather_location_latitude_, 218, 60);
        lv_obj_set_size(weather_location_latitude_, 135, 44);
        lv_textarea_set_one_line(weather_location_latitude_, true);
        lv_textarea_set_max_length(weather_location_latitude_, 15);
        lv_textarea_set_placeholder_text(weather_location_latitude_, "Latitude");
        lv_obj_add_event_cb(weather_location_latitude_, on_weather_location_field_clicked,
                            LV_EVENT_CLICKED, nullptr);

        weather_location_longitude_ = lv_textarea_create(weather_location_form_);
        lv_obj_set_pos(weather_location_longitude_, 363, 60);
        lv_obj_set_size(weather_location_longitude_, 135, 44);
        lv_textarea_set_one_line(weather_location_longitude_, true);
        lv_textarea_set_max_length(weather_location_longitude_, 15);
        lv_textarea_set_placeholder_text(weather_location_longitude_, "Longitude");
        lv_obj_add_event_cb(weather_location_longitude_, on_weather_location_field_clicked,
                            LV_EVENT_CLICKED, nullptr);

        weather_location_units_ = lv_dropdown_create(weather_location_form_);
        lv_obj_set_pos(weather_location_units_, 508, 60);
        lv_obj_set_size(weather_location_units_, 156, 44);
        lv_dropdown_set_options(weather_location_units_,
                                "Metric\nImperial");
        lv_obj_add_event_cb(weather_location_units_, on_dropdown_opened,
                            LV_EVENT_READY, nullptr);

        create_display_wifi_button(weather_location_form_, "Cancel", 674, 60, 106,
                                   on_weather_location_cancel_clicked, palette().text_muted);

        weather_location_feedback_ = lv_label_create(weather_location_form_);
        lv_obj_set_pos(weather_location_feedback_, 20, 116);
        lv_obj_set_size(weather_location_feedback_, 610, 32);
        lv_label_set_long_mode(weather_location_feedback_, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(weather_location_feedback_, lv_color_hex(palette().text_muted),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        set_dynamic_font(weather_location_feedback_);

        create_display_wifi_button(weather_location_form_, "Save", 650, 114, 130,
                                   on_weather_location_save_clicked, palette().action);

        weather_location_keyboard_ = lv_keyboard_create(weather_location_form_);
        lv_obj_set_size(weather_location_keyboard_, 776, 236);
        lv_obj_align(weather_location_keyboard_, LV_ALIGN_BOTTOM_MID, 0, -8);
        lv_keyboard_set_popovers(weather_location_keyboard_, false);
        lv_obj_add_event_cb(weather_location_keyboard_, on_weather_location_keyboard_ready,
                            LV_EVENT_READY, nullptr);
        lv_obj_add_event_cb(weather_location_keyboard_, on_weather_location_cancel_clicked,
                            LV_EVENT_CANCEL, nullptr);
        lv_obj_set_style_pad_all(weather_location_keyboard_, 3,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_row(weather_location_keyboard_, 3,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_column(weather_location_keyboard_, 3,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        set_dynamic_font(weather_location_keyboard_);
        lv_obj_move_foreground(weather_location_keyboard_);

        for (lv_obj_t* field : {weather_location_name_, weather_location_latitude_,
                                weather_location_longitude_, weather_location_units_,
                                weather_location_keyboard_}) {
            set_dynamic_font(field);
            apply_manual_input_appearance(field);
        }
        for (lv_obj_t* field : {weather_location_name_, weather_location_latitude_,
                                weather_location_longitude_}) {
            lv_obj_set_style_outline_color(field, lv_color_hex(palette().focus),
                                            LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_outline_width(field, recipe().focus_width,
                                           LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_outline_pad(field, 2,
                                         LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_outline_opa(field, LV_OPA_COVER,
                                         LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_border_color(field, lv_color_hex(palette().focus),
                                          LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_border_width(
                field, std::max<std::uint8_t>(recipe().divider_width,
                                              recipe().structural_border_width),
                                          LV_PART_MAIN | LV_STATE_FOCUSED);
        }
        lv_obj_set_style_bg_color(weather_location_keyboard_, lv_color_hex(palette().action_soft),
                                  LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(weather_location_keyboard_, lv_color_hex(palette().text_primary),
                                    LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_add_flag(weather_location_form_, LV_OBJ_FLAG_HIDDEN);
    }

    void select_weather_location_field(lv_obj_t* field) {
        if (!weather_location_form_visible_ || field == nullptr) return;
        for (lv_obj_t* candidate : {weather_location_name_, weather_location_latitude_,
                                    weather_location_longitude_}) {
            if (candidate != nullptr) lv_obj_remove_state(candidate, LV_STATE_FOCUSED);
        }
        lv_keyboard_set_mode(weather_location_keyboard_,
                             field == weather_location_name_
                                 ? LV_KEYBOARD_MODE_TEXT_LOWER
                                 : LV_KEYBOARD_MODE_NUMBER);
        lv_keyboard_set_textarea(weather_location_keyboard_, field);
        lv_obj_add_state(field, LV_STATE_FOCUSED);
        const char* field_name = field == weather_location_name_
            ? "location label"
            : (field == weather_location_latitude_ ? "latitude" : "longitude");
        const board::WeatherLocationConfiguration config =
            board::weather_service().location_configuration();
        char prompt[128];
        std::snprintf(prompt, sizeof(prompt),
                      config.api_key_configured
                          ? "Typing in: %s"
                          : "Typing in: %s - add the API key on the LAN page",
                      field_name);
        lv_label_set_text(weather_location_feedback_, prompt);
    }

    void queue_weather_location_save() {
        if (!weather_location_form_visible_ || pending_weather_location_save_) return;
        const char* name = lv_textarea_get_text(weather_location_name_);
        const char* latitude = lv_textarea_get_text(weather_location_latitude_);
        const char* longitude = lv_textarea_get_text(weather_location_longitude_);
        const board::WeatherUnits units = lv_dropdown_get_selected(weather_location_units_) == 1
            ? board::WeatherUnits::Imperial
            : board::WeatherUnits::Metric;
        const board::WeatherLocationConfiguration config =
            board::weather_service().location_configuration();
        if (!config.api_key_configured) {
            lv_label_set_text(weather_location_feedback_,
                              "Add the OpenWeather API key from the home-LAN page first.");
            return;
        }
        if (name == nullptr || *name == '\0' || latitude == nullptr || *latitude == '\0' ||
            longitude == nullptr || *longitude == '\0') {
            lv_label_set_text(weather_location_feedback_,
                              "Enter a label, latitude, and longitude.");
            return;
        }
        if (std::strlen(name) > 48 || std::strlen(latitude) > 15 ||
            std::strlen(longitude) > 15) {
            lv_label_set_text(weather_location_feedback_, "One or more values are too long.");
            return;
        }
        std::snprintf(pending_weather_location_name_.data(),
                      pending_weather_location_name_.size(), "%s", name);
        std::snprintf(pending_weather_location_latitude_.data(),
                      pending_weather_location_latitude_.size(), "%s", latitude);
        std::snprintf(pending_weather_location_longitude_.data(),
                      pending_weather_location_longitude_.size(), "%s", longitude);
        pending_weather_units_ = units;
        pending_weather_location_save_ = true;
        lv_label_set_text(weather_location_feedback_, "Saving weather location...");
    }

    void commit_pending_weather_location() {
        if (!pending_weather_location_save_) return;
        pending_weather_location_save_ = false;
        const bool saved = board::weather_service().save_location(
            pending_weather_location_latitude_.data(),
            pending_weather_location_longitude_.data(),
            pending_weather_location_name_.data(), pending_weather_units_);
        if (saved) {
            hide_weather_location_form();
            render_weather();
        } else if (weather_location_form_visible_) {
            lv_label_set_text(weather_location_feedback_,
                              "Invalid coordinates, or weather is currently updating. Try again.");
        }
    }

    void clear_weather_location_pending() {
        pending_weather_location_save_ = false;
        pending_weather_location_name_.fill('\0');
        pending_weather_location_latitude_.fill('\0');
        pending_weather_location_longitude_.fill('\0');
        pending_weather_units_ = board::WeatherUnits::Metric;
    }

    void hide_weather_location_form() {
        if (weather_location_form_ == nullptr || !weather_location_form_visible_) return;
        lv_keyboard_set_textarea(weather_location_keyboard_, nullptr);
        for (lv_obj_t* field : {weather_location_name_, weather_location_latitude_,
                                weather_location_longitude_}) {
            if (field != nullptr) lv_obj_remove_state(field, LV_STATE_FOCUSED);
        }
        clear_weather_location_pending();
        lv_obj_add_flag(weather_location_form_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(objects.weather_refresh_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(objects.weather_set_location_button, LV_OBJ_FLAG_HIDDEN);
        weather_location_form_visible_ = false;
    }

    void destroy_weather_location_form() {
        if (weather_location_form_ == nullptr) return;
        clear_weather_location_pending();
        lv_obj_delete(weather_location_form_);
        weather_location_form_ = nullptr;
        weather_location_name_ = nullptr;
        weather_location_latitude_ = nullptr;
        weather_location_longitude_ = nullptr;
        weather_location_units_ = nullptr;
        weather_location_feedback_ = nullptr;
        weather_location_keyboard_ = nullptr;
        weather_location_form_visible_ = false;
    }

    static void on_weather_location_field_clicked(lv_event_t* event) {
        instance().select_weather_location_field(
            static_cast<lv_obj_t*>(lv_event_get_target(event)));
    }

    static void on_weather_location_save_clicked(lv_event_t* event) {
        (void)event;
        instance().queue_weather_location_save();
    }

    static void on_weather_location_keyboard_ready(lv_event_t* event) {
        auto& controller = instance();
        lv_obj_t* field = lv_keyboard_get_textarea(
            static_cast<lv_obj_t*>(lv_event_get_target(event)));
        if (field == controller.weather_location_name_) {
            controller.select_weather_location_field(controller.weather_location_latitude_);
        } else if (field == controller.weather_location_latitude_) {
            controller.select_weather_location_field(controller.weather_location_longitude_);
        } else {
            controller.queue_weather_location_save();
        }
    }

    static void on_weather_location_cancel_clicked(lv_event_t* event) {
        (void)event;
        instance().hide_weather_location_form();
    }

    void load_current_calendar_screen() {
        const bool month = navigation_.view_mode() == CalendarViewMode::Month;
        lv_obj_t* target = month ? objects.month_screen : objects.agenda_screen;
        if (lv_screen_active() == target) return;
        loadScreen(month ? SCREEN_ID_MONTH_SCREEN : SCREEN_ID_AGENDA_SCREEN);
    }

    void render_current_calendar_view(bool restore_scroll = false) {
        if (navigation_.view_mode() == CalendarViewMode::Month) {
            render_month();
        } else {
            render_agenda(restore_scroll);
        }
    }

    void create_month_grid() {
        if (month_table_ != nullptr) return;

        lv_mem_monitor_t memory{};
        lv_mem_monitor(&memory);
        if (memory.free_size < 8 * 1024 || memory.free_biggest_size < 4 * 1024) {
            return;
        }

        // A table is one LVGL object for all 42 cells. The earlier button plus
        // two-label implementation needed 126 objects and exhausted LVGL's
        // 64 KiB heap during boot, after which a null label was positioned.
        month_table_ = lv_table_create(objects.month_grid_container);
        if (month_table_ == nullptr) return;
        lv_obj_set_pos(month_table_, 0, 0);
        lv_obj_set_size(month_table_, lv_pct(100), lv_pct(100));
        lv_table_set_row_count(month_table_, 6);
        lv_table_set_column_count(month_table_, 7);
        lv_obj_set_style_pad_all(month_table_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(month_table_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(month_table_, metrics().card_radius, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(month_table_, &lv_font_montserrat_16,
                                   LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_base_dir(month_table_, LV_BASE_DIR_LTR,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_base_dir(month_table_, LV_BASE_DIR_LTR,
                                  LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(month_table_, LV_TEXT_ALIGN_CENTER,
                                    LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_text_letter_space(month_table_, recipe().ascii_title_letter_space,
                                           LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(month_table_, 4, LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(month_table_, 4, LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(month_table_, 4, LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(month_table_, 4, LV_PART_ITEMS | LV_STATE_DEFAULT);
        // Montserrat 16 is 18 px high in the pinned LVGL build. Two lines of
        // day/count text plus 4 px top and bottom padding are exactly 44 px.
        // Capping the row prevents a wrapped count from hiding the sixth week.
        lv_obj_set_style_min_height(month_table_, 44, LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_max_height(month_table_, 44, LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(month_table_, 1,
                                      LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(month_table_, lv_color_hex(palette().divider),
                                      LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_update_layout(month_table_);
        const int32_t measured_table_width = lv_obj_get_content_width(month_table_);
        const std::uint32_t table_width = measured_table_width >= 7
            ? static_cast<std::uint32_t>(measured_table_width) : 756U;
        const std::uint32_t base_column_width = table_width / 7;
        const std::uint32_t extra_columns = table_width % 7;
        const std::array<lv_obj_t*, 7> headings{
            objects.month_weekday_label_0, objects.month_weekday_label_1,
            objects.month_weekday_label_2, objects.month_weekday_label_3,
            objects.month_weekday_label_4, objects.month_weekday_label_5,
            objects.month_weekday_label_6};
        lv_area_t table_bounds{}, header_content{};
        lv_obj_get_coords(month_table_, &table_bounds);
        lv_obj_get_content_coords(objects.month_weekday_header, &header_content);
        int32_t heading_x = table_bounds.x1 - header_content.x1;
        for (std::size_t column = 0; column < 7; ++column) {
            const std::uint32_t width = base_column_width +
                                       (column < extra_columns ? 1U : 0U);
            lv_table_set_column_width(month_table_, static_cast<std::uint32_t>(column), width);
            lv_obj_set_x(headings[column], heading_x);
            lv_obj_set_width(headings[column], static_cast<int32_t>(width));
            heading_x += static_cast<int32_t>(width);
        }
        // The table clears its pointer-selected row/column before the generic
        // CLICKED event. VALUE_CHANGED runs during RELEASED while both are
        // still available.
        lv_obj_add_event_cb(month_table_, on_month_cell_clicked, LV_EVENT_VALUE_CHANGED, nullptr);
        lv_obj_add_event_cb(month_table_, on_month_cell_draw, LV_EVENT_DRAW_TASK_ADDED, nullptr);
        lv_obj_add_flag(month_table_, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    }

    static void on_month_cell_clicked(lv_event_t* event) {
        if (!instance().has_local_time()) return;
        lv_obj_t* table = static_cast<lv_obj_t*>(lv_event_get_target(event));
        if (table == nullptr) return;
        std::uint32_t row = LV_TABLE_CELL_NONE;
        std::uint32_t column = LV_TABLE_CELL_NONE;
        lv_table_get_selected_cell(table, &row, &column);
        if (row >= 6 || column >= 7) return;
        instance().open_month_day(static_cast<std::uint8_t>(row * 7 + column));
    }

    static void on_month_cell_draw(lv_event_t* event) {
        lv_draw_task_t* task = lv_event_get_draw_task(event);
        if (task == nullptr) return;
        lv_draw_dsc_base_t* base = static_cast<lv_draw_dsc_base_t*>(
            lv_draw_task_get_draw_dsc(task));
        if (base == nullptr || base->part != LV_PART_ITEMS || base->id1 >= 6 || base->id2 >= 7) return;

        const std::size_t index = static_cast<std::size_t>(base->id1) * 7 + base->id2;
        const CalendarUiController& controller = instance();
        const CivilDate day = add_days(controller.navigation_.visible_period().start,
                                       static_cast<int>(index));
        const CivilDate anchor = controller.navigation_.visible_anchor_day();
        const bool time_available = controller.has_local_time();
        const bool selected = time_available && day == controller.navigation_.selected_day();
        const bool today = time_available && day == controller.navigation_.current_day();
        const bool in_month = !time_available ||
                              (day.year == anchor.year && day.month == anchor.month);

        const ThemePalette& colors = controller.palette();
        if (lv_draw_fill_dsc_t* fill = lv_draw_task_get_fill_dsc(task)) {
            fill->color = lv_color_hex(selected ? colors.selection
                                      : (today ? colors.today_fill
                                               : (in_month ? colors.surface
                                                           : colors.surface_muted)));
            fill->opa = LV_OPA_COVER;
        }
        if (lv_draw_border_dsc_t* border = lv_draw_task_get_border_dsc(task)) {
            border->color = lv_color_hex(selected && today ? colors.on_selection
                                        : (selected ? colors.focus
                                                    : (today ? colors.today_ring : colors.divider)));
            border->width = today ? 3 : (selected ? 2 : 1);
            border->opa = LV_OPA_COVER;
        }
        if (lv_draw_label_dsc_t* label = lv_draw_task_get_label_dsc(task)) {
            label->color = lv_color_hex(selected ? colors.on_selection
                                       : (today ? colors.today_text
                                                : (in_month ? colors.text_secondary
                                                            : colors.text_muted)));
            label->align = LV_TEXT_ALIGN_CENTER;
        }
    }

    void open_month_day(std::uint8_t index) {
        const CivilDate day = add_days(navigation_.visible_period().start, index);
        navigation_.select_day(day);
        navigation_.show_week_view();
        queue_period_parse();
    }

    void render_month() {
        apply_provider();
        style_calendar_period_controls();
        create_month_grid();
        const CalendarPeriod period = navigation_.visible_period();
        const CivilDate anchor = navigation_.visible_anchor_day();
        const bool time_available = has_local_time();
        set_date_navigation_enabled(time_available);

        if (month_table_ != nullptr) {
            if (time_available) {
                lv_obj_remove_state(month_table_, LV_STATE_DISABLED);
            } else {
                lv_obj_add_state(month_table_, LV_STATE_DISABLED);
            }
            for (std::size_t index = 0; index < kMonthCellCount; ++index) {
                const CivilDate day = add_days(period.start, static_cast<int>(index));
                char value[32];
                if (!time_available) {
                    value[0] = '\0';
                } else if (period_summary_valid_ &&
                    navigation_.provider_state() == ProviderState::Ready) {
                    const IcalSummaryCount count = period_summary_counts_[index];
                    const EventCountText count_text = format_event_count(count, false);
                    if (count_text[0] != '\0') {
                        std::snprintf(value, sizeof(value), "%u\n%s", day.day, count_text.data());
                    } else {
                        std::snprintf(value, sizeof(value), "%u", day.day);
                    }
                } else {
                    std::snprintf(value, sizeof(value), "%u", day.day);
                }
                lv_table_set_cell_value(month_table_, static_cast<std::uint32_t>(index / 7),
                                        static_cast<std::uint32_t>(index % 7), value);
            }
            lv_obj_invalidate(month_table_);
        }

        const std::string period_text = time_available
            ? month_period_text(anchor) : "Waiting for local date";
        lv_label_set_text(objects.month_period_label, period_text.c_str());
        set_provider_header(objects.month_sync_state_label);
        if (!time_available) lv_label_set_text(objects.month_time_label, "--:--");
        std::uint32_t month_status_color = palette().status_normal;
        if (month_table_ == nullptr) {
            lv_label_set_text(objects.month_state_label, "Month grid unavailable");
            month_status_color = palette().status_error;
        } else if (!time_available) {
            lv_label_set_text(objects.month_state_label, "Waiting for network time");
            month_status_color = palette().status_attention;
        } else if (navigation_.provider_state() == ProviderState::Loading) {
            lv_label_set_text(objects.month_state_label,
                              cached_calendar_document_.empty() && pending_calendar_document_.empty()
                                  ? "Waiting for calendar data"
                                  : "Loading this month...");
            month_status_color = palette().status_attention;
        } else if (navigation_.provider_state() == ProviderState::Stale) {
            lv_label_set_text(objects.month_state_label, "Offline copy available");
            month_status_color = palette().status_attention;
        } else if (navigation_.provider_state() == ProviderState::Error) {
            lv_label_set_text(objects.month_state_label, "Calendar unavailable; check Settings");
            month_status_color = palette().status_error;
        } else if (last_parse_matched_count_ == 0) {
            lv_label_set_text(objects.month_state_label, "No events in view");
        } else if (last_parse_truncated_) {
            lv_label_set_text_fmt(objects.month_state_label,
                                  "%u in view; list shortened",
                                  static_cast<unsigned>(last_parse_matched_count_));
        } else {
            lv_label_set_text_fmt(objects.month_state_label,
                                  "%u in view; tap a date",
                                  static_cast<unsigned>(last_parse_matched_count_));
        }
        // This label overlaps the month header, so body-text colors disappear
        // in light mode. Reapply the header status palette after every render
        // to cover initial load and theme changes.
        lv_obj_set_style_text_color(objects.month_state_label,
                                    lv_color_hex(month_status_color), LV_PART_MAIN | LV_STATE_DEFAULT);
        set_dynamic_font(objects.month_period_label);
        set_dynamic_font(objects.month_sync_state_label);
        set_dynamic_font(objects.month_state_label);
        set_dynamic_font(objects.month_time_label);
        set_large_time_font(objects.month_time_label);
        set_emphasis_font(objects.month_date_label);
        set_emphasis_font(objects.month_period_label);
    }

    void update_day_strip() {
        const CivilDate period_start = navigation_.visible_period().start;
        const CivilDate current = navigation_.current_day();
        const CivilDate selected = navigation_.selected_day();
        const bool time_available = has_local_time();
        for (std::size_t index = 0; index < kDayButtonCount; ++index) {
            const CivilDate day = add_days(period_start, static_cast<int>(index));
            lv_obj_t* button = day_buttons_[index];
            lv_label_set_text(day_names_[index], time_available ? weekday_name(day) : "---");
            const bool count_known = time_available && period_summary_valid_ &&
                                     navigation_.provider_state() == ProviderState::Ready;
            const IcalSummaryCount count = count_known
                ? period_summary_counts_[index] : IcalSummaryCount{};
            DayCountText day_text{};
            if (time_available) {
                day_text = format_day_count(day, count, count_known);
            } else {
                std::snprintf(day_text.data(), day_text.size(), "--/--");
            }
            lv_label_set_text(day_dates_[index], day_text.data());
            set_dynamic_font(day_names_[index]);
            set_dynamic_font(day_dates_[index]);
            set_compact_ascii_font(day_names_[index]);
            set_compact_ascii_font(day_dates_[index]);

            const bool is_selected = time_available && day == selected;
            const bool is_today = time_available && day == current;
            if (time_available) {
                lv_obj_remove_state(button, LV_STATE_DISABLED);
            } else {
                lv_obj_add_state(button, LV_STATE_DISABLED);
            }
            lv_obj_set_style_bg_color(button,
                                      lv_color_hex(is_selected ? palette().selection
                                                               : palette().surface),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(button, lv_color_hex(is_selected ? palette().action_pressed
                                                                        : palette().action_soft),
                                      LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_color(button,
                                          lv_color_hex(is_selected && is_today ? palette().on_selection
                                                       : (is_selected ? palette().focus
                                                                      : (is_today ? palette().today_ring
                                                                                  : palette().divider))),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(button, is_today ? 3 : (is_selected ? 2 : 1),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(button, metrics().control_radius,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
            style_button_text(button, is_selected ? palette().on_selection : palette().text_secondary,
                              is_selected ? palette().on_action : palette().text_primary);
        }
    }

    void ensure_row(std::size_t index) {
        if (rows_[index] != nullptr) return;
        lv_obj_t* row = lv_button_create(objects.agenda_rows_container);
        rows_[index] = row;
        lv_obj_set_pos(row, 0, static_cast<lv_coord_t>(index * 60));
        lv_obj_set_size(row, lv_pct(100), 54);
        lv_obj_set_style_bg_color(row, lv_color_hex(palette().surface), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(row, lv_color_hex(palette().action_soft), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_border_color(row, lv_color_hex(palette().divider), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(row, metrics().card_radius, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(row, on_event_row_clicked, LV_EVENT_CLICKED, &row_bindings_[index]);

        row_colors_[index] = lv_obj_create(row);
        lv_obj_set_pos(row_colors_[index], 8, 20);
        lv_obj_set_size(row_colors_[index], 14, 14);
        lv_obj_remove_flag(row_colors_[index], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(row_colors_[index], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(row_colors_[index], lv_color_hex(palette().action),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(row_colors_[index], lv_color_hex(palette().text_primary),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(row_colors_[index], 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(row_colors_[index], LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
        style_event_row(row, row_colors_[index], false);

        row_titles_[index] = lv_label_create(row);
        lv_obj_set_pos(row_titles_[index], 30, 5);
        lv_obj_set_size(row_titles_[index], 480, 20);
        lv_label_set_text(row_titles_[index], "");
        lv_label_set_long_mode(row_titles_[index], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(row_titles_[index], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        set_dynamic_font(row_titles_[index]);
        lv_obj_set_style_text_color(row_titles_[index], lv_color_hex(palette().text_primary),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(row_titles_[index], lv_color_hex(palette().text_primary),
                                    LV_PART_MAIN | LV_STATE_PRESSED);

        row_sources_[index] = lv_label_create(row);
        lv_obj_set_pos(row_sources_[index], 30, 28);
        lv_obj_set_size(row_sources_[index], 480, 20);
        lv_label_set_text(row_sources_[index], "");
        lv_label_set_long_mode(row_sources_[index], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(row_sources_[index], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        set_dynamic_font(row_sources_[index]);

        row_times_[index] = lv_label_create(row);
        lv_obj_set_pos(row_times_[index], 520, 15);
        lv_obj_set_size(row_times_[index], 195, 24);
        lv_obj_set_style_text_align(row_times_[index], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
        set_dynamic_font(row_times_[index]);
        lv_obj_set_style_text_color(row_times_[index], lv_color_hex(palette().text_muted),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(row_times_[index], lv_color_hex(palette().text_muted),
                                    LV_PART_MAIN | LV_STATE_PRESSED);
    }

    static void on_event_row_clicked(lv_event_t* event) {
        auto* binding = static_cast<RowBinding*>(lv_event_get_user_data(event));
        if (binding == nullptr || binding->event_id[0] == '\0') return;
        instance().open_event(binding->event_id);
    }

    void open_event(const std::string& event_id) {
        const ProviderResult result = provider_.current();
        apply_provider();
        const int32_t scroll_y = lv_obj_get_scroll_y(objects.agenda_rows_container);
        const std::size_t saved_scroll = scroll_y > 0 ? static_cast<std::size_t>(scroll_y) : 0;
        if (!result.snapshot || !navigation_.open_event(event_id, *result.snapshot, saved_scroll)) {
            render_agenda();
            return;
        }
        const CalendarEvent* event = result.snapshot->find_by_id(event_id);
        if (event == nullptr) {  // Defensive check for a provider refresh between lookups.
            show_agenda();
            return;
        }
        details_origin_ = PrimaryPage::Calendar;
        render_details(*event);
        loadScreen(SCREEN_ID_EVENT_DETAILS_SCREEN);
    }

    void render_rows(const CalendarEventList& events) {
        const std::size_t visible_count = events.size() < kMaxAgendaRows ? events.size() : kMaxAgendaRows;
        for (std::size_t index = 0; index < visible_count; ++index) {
            ensure_row(index);
            RowBinding& binding = row_bindings_[index];
            std::strncpy(binding.event_id, events[index].id.c_str(), kMaxEventIdBytes);
            binding.event_id[kMaxEventIdBytes] = '\0';
            lv_label_set_text(row_titles_[index], display_title(events[index]).c_str());
            lv_label_set_text(row_sources_[index], events[index].calendar.display_name.c_str());
            lv_obj_set_style_text_color(row_sources_[index],
                                        lv_color_hex(palette().text_muted),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            const std::string time = agenda_time_text(events[index]);
            lv_label_set_text(row_times_[index], time.c_str());
            lv_obj_set_style_bg_color(row_colors_[index],
                                      lv_color_hex(agenda_indicator_color(
                                          events[index].calendar.color_rgb888, palette())),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_remove_flag(rows_[index], LV_OBJ_FLAG_HIDDEN);
        }
        for (std::size_t index = visible_count; index < rows_.size(); ++index) {
            if (rows_[index] != nullptr) lv_obj_add_flag(rows_[index], LV_OBJ_FLAG_HIDDEN);
            row_bindings_[index].event_id[0] = '\0';
        }
    }

    void render_agenda(bool restore_scroll = false) {
        apply_provider();
        style_calendar_period_controls();
        const bool time_available = has_local_time();
        set_date_navigation_enabled(time_available);
        update_day_strip();
        const CalendarPeriod period = navigation_.visible_period();
        const ProviderResult result = provider_.current();
        const CalendarEventList matches = result.snapshot && time_available
            ? filter_and_sort_agenda(*result.snapshot, selected_day_window())
            : CalendarEventList{};
        render_rows(matches);

        std::size_t summary_index = 0;
        CivilDate summary_day = period.start;
        while (summary_day != navigation_.selected_day() && summary_index < kDayButtonCount) {
            summary_day = add_days(summary_day, 1);
            ++summary_index;
        }
        const bool exact_count_known = period_summary_valid_ && summary_index < kDayButtonCount;
        const IcalSummaryCount exact_count = exact_count_known
            ? period_summary_counts_[summary_index] : IcalSummaryCount{};
        set_provider_header(objects.sync_state_label);
        if (navigation_.provider_state() == ProviderState::Loading) {
            lv_label_set_text(objects.agenda_state_label,
                              cached_calendar_document_.empty() && pending_calendar_document_.empty()
                                  ? "Waiting for calendar data"
                                  : "Reading this week's calendar...");
        } else if (navigation_.provider_state() == ProviderState::Stale) {
            lv_label_set_text(objects.agenda_state_label,
                              "Could not load this period; previous calendar remains available");
        } else if (navigation_.provider_state() == ProviderState::Error) {
            lv_label_set_text(objects.agenda_state_label,
                              "Calendar unavailable - open Settings to check the connection");
        } else if (exact_count_known && exact_count.event_count == 0) {
            lv_label_set_text(objects.agenda_state_label, "No events for this day");
        } else if (exact_count_known &&
                   (exact_count.overflow || exact_count.event_count > kMaxAgendaRows ||
                    exact_count.event_count > matches.size())) {
            const std::size_t shown = std::min(matches.size(), kMaxAgendaRows);
            if (exact_count.overflow) {
                lv_label_set_text_fmt(objects.agenda_state_label, "Showing %u of 65535+ events",
                                      static_cast<unsigned>(shown));
            } else if (last_parse_truncated_ && exact_count.event_count > matches.size()) {
                lv_label_set_text_fmt(objects.agenda_state_label,
                                      "Showing %u of %u events (256-period limit)",
                                      static_cast<unsigned>(shown),
                                      static_cast<unsigned>(exact_count.event_count));
            } else {
                lv_label_set_text_fmt(objects.agenda_state_label, "Showing %u of %u events",
                                      static_cast<unsigned>(shown),
                                      static_cast<unsigned>(exact_count.event_count));
            }
        } else if (matches.empty()) {
            lv_label_set_text(objects.agenda_state_label, "No events for this day");
        } else if (matches.size() > kMaxAgendaRows) {
            lv_label_set_text_fmt(objects.agenda_state_label, "Showing %u of %u events",
                                  static_cast<unsigned>(kMaxAgendaRows), static_cast<unsigned>(matches.size()));
        } else {
            const EventCountText count_text = exact_count_known
                ? format_event_count(exact_count, true)
                : format_event_count({static_cast<std::uint16_t>(matches.size()), false}, true);
            lv_label_set_text(objects.agenda_state_label, count_text.data());
        }
        set_dynamic_font(objects.agenda_state_label);
        if (!has_local_time()) lv_label_set_text(objects.header_time_label, "--:--");
        const std::string selected_date = time_available
            ? date_text(navigation_.selected_day()) : "Date unavailable";
        const std::string period_text = time_available
            ? week_period_text(period) : "Waiting for local date";
        lv_label_set_text(objects.header_date_label, selected_date.c_str());
        lv_label_set_text(objects.agenda_period_label, period_text.c_str());
        set_dynamic_font(objects.header_time_label);
        set_dynamic_font(objects.header_date_label);
        set_dynamic_font(objects.agenda_period_label);
        set_large_time_font(objects.header_time_label);
        set_emphasis_font(objects.header_date_label);
        set_emphasis_font(objects.agenda_period_label);
        const char* footer = details_notice_pending_
            ? "This event is no longer available"
            : (navigation_.provider_state() == ProviderState::Loading
            ? "Reading the cached calendar"
            : (navigation_.provider_state() == ProviderState::Error
                   ? "Open Settings to check Wi-Fi and calendar sync"
                   : (matches.empty() ? "Choose another day" : "Tap an event for details")));
        lv_label_set_text(objects.footer_label, footer);
        details_notice_pending_ = false;
        set_dynamic_font(objects.footer_label);

        if (restore_scroll) {
            lv_obj_scroll_to_y(objects.agenda_rows_container,
                               static_cast<int32_t>(navigation_.agenda_scroll_position()), LV_ANIM_OFF);
        } else {
            lv_obj_scroll_to_y(objects.agenda_rows_container, 0, LV_ANIM_OFF);
        }
    }

    void render_details(const CalendarEvent& event) {
        lv_label_set_text(objects.details_title_label, display_title(event).c_str());
        const std::string time = event_time_text(event);
        lv_label_set_text(objects.details_time_label, time.c_str());
        std::string calendar_name("Calendar: ");
        calendar_name.append(event.calendar.display_name.data(), event.calendar.display_name.size());
        lv_label_set_text(objects.details_calendar_label, calendar_name.c_str());
        if (event.location && !event.location->empty()) {
            std::string location("Location: ");
            location.append(event.location->data(), event.location->size());
            lv_label_set_text(objects.details_location_label, location.c_str());
            lv_obj_remove_flag(objects.details_location_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(objects.details_location_label, LV_OBJ_FLAG_HIDDEN);
        }
        set_dynamic_font(objects.details_title_label);
        set_dynamic_font(objects.details_time_label);
        set_dynamic_font(objects.details_location_label);
        set_dynamic_font(objects.details_calendar_label);
        // EEZ owns the content shell and label widths. Content-dependent
        // heights let long fields wrap completely inside its scroll viewport.
        int32_t next_y = 24;
        for (lv_obj_t* label : {objects.details_title_label, objects.details_time_label,
                                objects.details_location_label, objects.details_calendar_label}) {
            if (lv_obj_has_flag(label, LV_OBJ_FLAG_HIDDEN)) continue;
            lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
            lv_obj_set_height(label, LV_SIZE_CONTENT);
            lv_obj_set_y(label, next_y);
            lv_obj_update_layout(label);
            next_y += lv_obj_get_height(label) + 24;
        }
    }

    static CalendarUiController& instance() {
        static CalendarUiController controller;
        return controller;
    }

    friend CalendarUiController& controller_instance();

    FixedClock clock_;
    MockCalendarProvider provider_;
    NavigationState navigation_;
    board::CalendarDocument cached_calendar_document_;
    board::CalendarDocument pending_calendar_document_;
    std::unique_ptr<IcalParseSession> period_parse_session_;
    std::array<IcalSummaryCount, kMaxIcalSummaryBuckets> period_summary_counts_{};
    std::size_t last_parse_matched_count_ = 0;
    bool initialized_ = false;
    bool local_time_was_available_ = false;
    bool initial_sync_requested_ = false;
    bool period_parse_pending_ = false;
    bool pending_document_is_download_ = false;
    bool active_parse_is_download_ = false;
    std::uint32_t active_parse_configuration_generation_ = 0;
    bool calendar_view_requested_ = false;
    bool period_summary_valid_ = false;
    bool last_parse_truncated_ = false;
    bool details_notice_pending_ = false;
    bool calendar_setup_help_visible_ = false;
    bool display_wifi_form_visible_ = false;
    bool pending_display_wifi_save_ = false;
    bool pending_display_wifi_scan_ = false;
    bool weather_location_form_visible_ = false;
    bool pending_weather_location_save_ = false;
    AppearancePreferences appearance_{};
    AppearancePreferences persisted_appearance_{};
    bool appearance_save_pending_ = false;
    std::uint32_t last_live_date_update_ms_ = 0;
    std::uint32_t last_settings_render_ms_ = 0;
    std::uint32_t last_firmware_render_ms_ = 0;
    std::uint32_t last_status_render_ms_ = 0;
    bool live_date_update_time_valid_ = false;
    bool settings_render_time_valid_ = false;
    bool firmware_render_time_valid_ = false;
    std::array<std::string, 3> settings_status_text_{};
    std::array<bool, 3> settings_status_text_valid_{};
    ThemeId last_settings_status_theme_id_ = kDefaultThemeId;
    bool last_settings_status_dark_theme_ = false;
    bool settings_status_layout_valid_ = false;
    FirmwareRenderSignature firmware_paint_signature_{};
    bool firmware_paint_signature_valid_ = false;
    bool firmware_actions_paint_valid_ = false;
    bool last_firmware_can_arm_ = false;
    bool last_firmware_can_cancel_ = false;
    bool last_firmware_can_reboot_ = false;
    ThemeId last_firmware_theme_id_ = kDefaultThemeId;
    bool last_firmware_dark_theme_ = false;
    bool status_render_time_valid_ = false;
    const char* last_connection_status_label_ = nullptr;
    const char* last_calendar_status_label_ = nullptr;
    std::int64_t last_status_success_utc_ = 0;
    CivilDate last_status_day_{};
    ThemeId last_status_theme_id_ = kDefaultThemeId;
    bool last_status_dark_theme_ = false;
    const lv_obj_t* last_settings_primary_ = nullptr;
    ThemeId last_settings_theme_id_ = kDefaultThemeId;
    bool last_settings_dark_theme_ = false;
    bool settings_actions_styled_ = false;
    PrimaryPage primary_page_ = PrimaryPage::Main;
    PrimaryPage settings_origin_ = PrimaryPage::Main;
    PrimaryPage details_origin_ = PrimaryPage::Calendar;
    std::time_t last_main_render_minute_ = -1;
    ProviderState last_main_provider_state_ = ProviderState::Loading;
    const CalendarSnapshot* last_main_snapshot_ = nullptr;
    bool main_render_signature_valid_ = false;
    board::WeatherServiceStatus last_weather_status_{};
    bool weather_render_signature_valid_ = false;
    bool display_wifi_scan_results_rendered_ = false;
    lv_obj_t* display_wifi_form_ = nullptr;
    lv_obj_t* display_wifi_ssid_ = nullptr;
    lv_obj_t* display_wifi_password_ = nullptr;
    lv_obj_t* display_wifi_feedback_ = nullptr;
    lv_obj_t* display_wifi_keyboard_ = nullptr;
    lv_obj_t* display_wifi_scan_button_ = nullptr;
    lv_obj_t* display_wifi_network_picker_ = nullptr;
    lv_obj_t* weather_location_form_ = nullptr;
    lv_obj_t* weather_location_name_ = nullptr;
    lv_obj_t* weather_location_latitude_ = nullptr;
    lv_obj_t* weather_location_longitude_ = nullptr;
    lv_obj_t* weather_location_units_ = nullptr;
    lv_obj_t* weather_location_feedback_ = nullptr;
    lv_obj_t* weather_location_keyboard_ = nullptr;
    std::uint32_t display_wifi_scan_generation_ = 0;
    std::size_t display_wifi_scan_count_ = 0;
    std::array<char, 33> pending_display_wifi_ssid_{};
    std::array<char, 64> pending_display_wifi_password_{};
    std::array<char, 49> pending_weather_location_name_{};
    std::array<char, 16> pending_weather_location_latitude_{};
    std::array<char, 16> pending_weather_location_longitude_{};
    board::WeatherUnits pending_weather_units_ = board::WeatherUnits::Metric;
    std::array<lv_obj_t*, kMaxAgendaRows> rows_{};
    std::array<lv_obj_t*, kMaxAgendaRows> row_colors_{};
    std::array<lv_obj_t*, kMaxAgendaRows> row_titles_{};
    std::array<lv_obj_t*, kMaxAgendaRows> row_sources_{};
    std::array<lv_obj_t*, kMaxAgendaRows> row_times_{};
    std::array<RowBinding, kMaxAgendaRows> row_bindings_{};
    std::array<lv_obj_t*, kMaxMainRows> main_rows_{};
    std::array<lv_obj_t*, kMaxMainRows> main_row_colors_{};
    std::array<lv_obj_t*, kMaxMainRows> main_row_titles_{};
    std::array<lv_obj_t*, kMaxMainRows> main_row_sources_{};
    std::array<lv_obj_t*, kMaxMainRows> main_row_times_{};
    std::array<RowBinding, kMaxMainRows> main_row_bindings_{};
    weather_icon::State main_weather_icon_{};
    weather_icon::State current_weather_icon_{};
    std::array<weather_icon::State, weather::kMaxForecastDays> forecast_weather_icons_{};
    lv_obj_t* month_table_ = nullptr;
    std::array<lv_obj_t*, kDayButtonCount> day_buttons_{
        objects.day_button_0, objects.day_button_1, objects.day_button_2, objects.day_button_3,
        objects.day_button_4, objects.day_button_5, objects.day_button_6};
    std::array<lv_obj_t*, kDayButtonCount> day_names_{
        objects.day_name_label_0, objects.day_name_label_1, objects.day_name_label_2, objects.day_name_label_3,
        objects.day_name_label_4, objects.day_name_label_5, objects.day_name_label_6};
    std::array<lv_obj_t*, kDayButtonCount> day_dates_{
        objects.day_date_label_0, objects.day_date_label_1, objects.day_date_label_2, objects.day_date_label_3,
        objects.day_date_label_4, objects.day_date_label_5, objects.day_date_label_6};
    std::array<lv_obj_t*, calendar::weather::kMaxForecastDays> forecast_day_labels_{
        objects.forecast_day_label_0, objects.forecast_day_label_1, objects.forecast_day_label_2,
        objects.forecast_day_label_3, objects.forecast_day_label_4,
        objects.forecast_day_label_5, objects.forecast_day_label_6};
    std::array<lv_obj_t*, calendar::weather::kMaxForecastDays> forecast_temp_labels_{
        objects.forecast_temp_label_0, objects.forecast_temp_label_1, objects.forecast_temp_label_2,
        objects.forecast_temp_label_3, objects.forecast_temp_label_4,
        objects.forecast_temp_label_5, objects.forecast_temp_label_6};
    std::array<lv_obj_t*, calendar::weather::kMaxForecastDays> forecast_condition_labels_{
        objects.forecast_condition_label_0, objects.forecast_condition_label_1,
        objects.forecast_condition_label_2, objects.forecast_condition_label_3,
        objects.forecast_condition_label_4, objects.forecast_condition_label_5,
        objects.forecast_condition_label_6};
};

CalendarUiController& controller_instance() {
    return CalendarUiController::instance();
}

}  // namespace

void initialize_calendar_ui() { controller_instance().initialize(); }
void tick_calendar_ui() { controller_instance().tick(); }
void select_calendar_day(std::uint8_t day_offset) { controller_instance().select_day(day_offset); }
void show_calendar_main() { controller_instance().show_main(); }
void show_calendar_primary_page() { controller_instance().show_calendar(); }
void show_calendar_forecast() { controller_instance().show_forecast(); }
void show_calendar_previous_primary_page() { controller_instance().show_previous_primary(); }
void show_calendar_agenda() { controller_instance().show_agenda(); }
void show_calendar_settings() { controller_instance().show_settings(); }
void show_previous_calendar_period() { controller_instance().previous_period(); }
void show_next_calendar_period() { controller_instance().next_period(); }
void show_calendar_today() { controller_instance().show_today(); }
void show_calendar_week_view() { controller_instance().show_week_view(); }
void show_calendar_month_view() { controller_instance().show_month_view(); }
void begin_calendar_network_setup() { controller_instance().begin_network_setup(); }
void open_calendar_display_wifi_setup() { controller_instance().open_display_wifi_setup(); }
void sync_calendar_now() { controller_instance().sync_now(); }
void show_calendar_brightness_settings() { controller_instance().show_brightness_settings(); }
void show_calendar_settings_from_brightness() { controller_instance().show_settings_from_brightness(); }
void change_calendar_theme_style() { controller_instance().change_theme_style(); }
void toggle_calendar_dark_theme() { controller_instance().toggle_dark_theme(); }
void preview_calendar_brightness() { controller_instance().preview_brightness(); }
void commit_calendar_brightness() { controller_instance().commit_brightness(); }
void show_calendar_weather() { controller_instance().show_weather(); }
void show_calendar_settings_from_weather() { controller_instance().show_settings_from_weather(); }
void refresh_calendar_weather() { controller_instance().refresh_weather(); }
void open_calendar_weather_location_editor() { controller_instance().open_weather_location_editor(); }
void show_calendar_firmware_update() { controller_instance().show_firmware_update(); }
void show_calendar_settings_from_firmware() { controller_instance().show_settings_from_firmware(); }
void arm_calendar_firmware_update() { controller_instance().arm_firmware_update(); }
void cancel_calendar_firmware_update() { controller_instance().cancel_firmware_update(); }
void reboot_calendar_firmware_update() { controller_instance().reboot_firmware_update(); }
void set_calendar_provider_state_for_debug(ProviderState state) { controller_instance().set_provider_state_for_debug(state); }
void refresh_without_selected_event_for_debug() { controller_instance().refresh_without_selected_event_for_debug(); }

}  // namespace calendar
