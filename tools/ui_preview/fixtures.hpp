#pragma once
#include "app/appearance_preferences.hpp"
#include "board/connectivity_service.hpp"
#include "board/ota_service.hpp"
#include "board/weather_service.hpp"
#include <ctime>
#include <string>
namespace preview {
extern calendar::AppearancePreferences appearance;
extern board::ConnectivityStatus connection;
extern board::WifiScanResults wifi_scan_results;
extern board::OtaStatus ota_status;
extern board::WeatherServiceStatus weather_status;
extern calendar::weather::WeatherSnapshot weather;
extern board::CalendarDocument pending_document;
extern std::time_t now;
extern unsigned appearance_save_count;
extern unsigned backlight_update_count;
extern std::uint8_t last_backlight_percent;
extern std::uint16_t ambient_light_raw;
void set_fixture(bool mixed, bool imperial);
void queue_calendar(const std::string &document);
void reset_runtime_counters();
} // namespace preview
