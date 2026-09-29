// Desktop fixtures only. This file is excluded from PlatformIO's src tree.
#include "app/appearance_preferences.hpp"
#include "board/ota_service.hpp"
#include "board/runtime.h"
#include "fixtures.hpp"
#include <cstdlib>
#include <cstring>
#include <utility>

namespace preview {
calendar::AppearancePreferences appearance;
board::ConnectivityStatus connection;
board::WifiScanResults wifi_scan_results;
board::OtaStatus ota_status;
board::WeatherServiceStatus weather_status;
calendar::weather::WeatherSnapshot weather;
board::CalendarDocument pending_document;
std::time_t now = 0;
unsigned appearance_save_count = 0;
unsigned backlight_update_count = 0;
std::uint8_t last_backlight_percent = 0;
std::uint16_t ambient_light_raw = 300;
void reset_runtime_counters() {
  appearance_save_count = 0;
  backlight_update_count = 0;
  last_backlight_percent = 0;
}
void queue_calendar(const std::string &text) {
  board::CalendarDocument document;
  document.reserve(text.size());
  document.append(reinterpret_cast<const std::uint8_t *>(text.data()),
                  text.size());
  document.set_configuration_generation(
      connection.calendar_configuration_generation);
  pending_document = std::move(document);
  connection.fetch_state = board::CalendarFetchState::Succeeded;
  connection.import_result_known = false;
}
void set_fixture(bool mixed, bool imperial) {
  connection = {};
  connection.service_initialized = true;
  connection.state = board::ConnectivityState::Ready;
  connection.wifi_connected = connection.time_synchronized = true;
  connection.wifi_credentials_saved = connection.calendar_feed_saved =
      connection.credentials_saved = true;
  connection.configured_feed_count = 2;
  connection.calendar_configuration_generation = 1;
  std::strcpy(connection.station_address, "192.168.1.42");
  std::strcpy(connection.summary, "Calendar imported");
  wifi_scan_results = {};
  wifi_scan_results.completed = true;
  wifi_scan_results.generation = 1;
  wifi_scan_results.count = 2;
  std::strcpy(wifi_scan_results.networks[0].ssid, "Calendar Lab");
  wifi_scan_results.networks[0].rssi = -41;
  wifi_scan_results.networks[0].secured = true;
  std::strcpy(wifi_scan_results.networks[1].ssid,
              mixed ? "רשת בית / Home" : "Guest Network");
  wifi_scan_results.networks[1].rssi = -67;
  wifi_scan_results.networks[1].secured = true;
  ota_status = {};
  ota_status.state = board::OtaState::Disabled;
  std::strcpy(ota_status.detail, "Firmware updates are disabled");
  weather_status = {};
  weather_status.state = board::WeatherServiceState::Ready;
  weather_status.units =
      imperial ? board::WeatherUnits::Imperial : board::WeatherUnits::Metric;
  weather_status.configured = weather_status.snapshot_available = true;
  weather_status.generation = 1;
  weather_status.updated_utc = now - 660;
  std::strcpy(weather_status.location,
              mixed ? "ירושלים / Jerusalem" : "Jerusalem");
  std::strcpy(weather_status.detail, "Weather ready");
  weather = {};
  weather.current.available = true;
  weather.current.temperature_tenths_c = imperial ? 378 : 260;
  weather.current.feels_like_tenths_c = imperial ? 378 : 270;
  weather.current.humidity_percent = 100;
  weather.current.wind_tenths_mps = 33;
  weather.current.condition_id = 801;
  std::strcpy(weather.current.description.data(),
              mixed ? "מעונן חלקית" : "Partly cloudy");
  weather.forecast_count = 7;
  const int conditions[]{800, 801, 803, 500, 601, 211, 741};
  for (int i = 0; i < 7; i++) {
    auto &day = weather.forecast[i];
    day.date = {2026, static_cast<std::uint8_t>(i < 3 ? 9 : 10),
                static_cast<std::uint8_t>(i < 3 ? 28 + i : i - 2)};
    day.minimum_tenths_c = 180 + i * 10;
    day.maximum_tenths_c = 260 + i * 10;
    day.maximum_precipitation_probability_percent = i == 3 ? 70 : 10;
    day.dominant_condition_id = conditions[i];
    std::strcpy(day.description.data(), "Fixture condition");
  }
  const std::string document =
      "X-ESP32-CALENDAR-ID:work\r\nX-ESP32-CALENDAR-NAME:" +
      std::string(mixed ? "עבודה / Work" : "Work") +
      "\r\nX-ESP32-CALENDAR-COLOR:2D7FF9\r\n"
      "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nUID:"
      "standup\r\nDTSTART:20260928T073000Z\r\nDTEND:"
      "20260928T080000Z\r\nSUMMARY:" +
      std::string(mixed ? "פגישת צוות / Team stand-up" : "Team stand-up") +
      "\r\nLOCATION:Main conference "
      "room\r\nEND:VEVENT\r\nBEGIN:VEVENT\r\nUID:design-review\r\nDTSTART:"
      "20260928T083000Z\r\nDTEND:20260928T090000Z\r\nSUMMARY:Design "
      "review\r\nEND:VEVENT\r\nBEGIN:VEVENT\r\nUID:long-title\r\nDTSTART:"
      "20260928T100000Z\r\nDTEND:20260928T110000Z\r\nSUMMARY:A deliberately "
      "long planning event with extra information that must remain readable in "
      "Event Details\r\nLOCATION:A long location including building entrance "
      "floor room number and "
      "Jerusalem\r\nEND:VEVENT\r\nBEGIN:VEVENT\r\nUID:project-"
      "checkin\r\nDTSTART:20260928T113000Z\r\nDTEND:"
      "20260928T120000Z\r\nSUMMARY:Project "
      "check-in\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n"
      "X-ESP32-CALENDAR-ID:family\r\nX-ESP32-CALENDAR-NAME:Family\r\nX-ESP32-"
      "CALENDAR-COLOR:35A854\r\n"
      "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nUID:"
      "birthday\r\nDTSTART;VALUE=DATE:20260928\r\nDTEND;VALUE=DATE:"
      "20260929\r\nSUMMARY:" +
      std::string(mixed ? "יום הולדת / Birthday" : "Maya's birthday") +
      "\r\nEND:VEVENT\r\nBEGIN:VEVENT\r\nUID:appointment\r\nDTSTART:"
      "20260928T130000Z\r\nDTEND:20260928T133000Z\r\nSUMMARY:Dentist "
      "appointment\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n";
  queue_calendar(document);
}
} // namespace preview
extern "C" std::time_t __wrap_time(std::time_t *value) {
  if (value)
    *value = preview::now;
  return preview::now;
}
namespace calendar {
AppearancePreferences load_appearance_preferences() {
  return preview::appearance;
}
bool save_appearance_preferences(const AppearancePreferences &value) {
  ++preview::appearance_save_count;
  preview::appearance = value;
  return true;
}
std::uint8_t clamp_brightness_percent(std::uint8_t value) {
  return value < 10 ? 10 : value > 100 ? 100 : value;
}
} // namespace calendar
namespace board_runtime {
std::uint16_t readAmbientLightRaw() {
  return preview::ambient_light_raw;
}
void setBacklightPercent(std::uint8_t value) {
  ++preview::backlight_update_count;
  preview::last_backlight_percent = value;
}
} // namespace board_runtime
namespace board {
CalendarDocument::~CalendarDocument() { release(); }
CalendarDocument::CalendarDocument(CalendarDocument &&other) noexcept {
  *this = std::move(other);
}
CalendarDocument &
CalendarDocument::operator=(CalendarDocument &&other) noexcept {
  if (this != &other) {
    release();
    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;
    configuration_generation_ = other.configuration_generation_;
    other.data_ = nullptr;
    other.size_ = other.capacity_ = 0;
    other.configuration_generation_ = 0;
  }
  return *this;
}
void CalendarDocument::release() {
  std::free(data_);
  data_ = nullptr;
  size_ = capacity_ = 0;
}
bool CalendarDocument::empty() const { return size_ == 0; }
std::size_t CalendarDocument::size() const { return size_; }
std::string_view CalendarDocument::view() const {
  return {data_ ? data_ : "", size_};
}
std::uint32_t CalendarDocument::configuration_generation() const {
  return configuration_generation_;
}
bool CalendarDocument::reserve(std::size_t size) {
  void *data = std::realloc(data_, size + 1);
  if (!data)
    return false;
  data_ = static_cast<char *>(data);
  capacity_ = size;
  data_[size_] = '\0';
  return true;
}
bool CalendarDocument::append(const std::uint8_t *data, std::size_t size) {
  if (size_ + size > capacity_)
    return false;
  std::memcpy(data_ + size_, data, size);
  size_ += size;
  data_[size_] = '\0';
  return true;
}
bool CalendarDocument::truncate(std::size_t size) {
  if (size > size_)
    return false;
  size_ = size;
  data_[size] = '\0';
  return true;
}
void CalendarDocument::set_configuration_generation(std::uint32_t value) {
  configuration_generation_ = value;
}
struct ConnectivityService::Impl {};
ConnectivityService::ConnectivityService() : impl_(std::make_unique<Impl>()) {}
ConnectivityService::~ConnectivityService() = default;
ConnectivityService &connectivity_service() {
  static ConnectivityService service;
  return service;
}
ConnectivityStatus ConnectivityService::status() const {
  return preview::connection;
}
bool ConnectivityService::take_downloaded_ical(CalendarDocument &document) {
  if (preview::pending_document.empty())
    return false;
  document = std::move(preview::pending_document);
  return true;
}
void ConnectivityService::report_ical_import_result(std::uint32_t generation,
                                                    bool success,
                                                    std::size_t imported,
                                                    std::size_t skipped) {
  if (generation != preview::connection.calendar_configuration_generation)
    return;
  auto &s = preview::connection;
  s.import_result_known = true;
  s.last_import_succeeded = success;
  s.last_imported_events = imported;
  s.last_skipped_events = skipped;
  if (success) {
    s.last_successful_import_utc = preview::now;
    ++s.successful_import_generation;
  }
}
bool ConnectivityService::request_sync() { return false; }
void ConnectivityService::begin_setup() {}
WifiScanRequestResult ConnectivityService::request_wifi_scan() {
  return WifiScanRequestResult::Queued;
}
WifiScanResults ConnectivityService::wifi_scan_results() const {
  return preview::wifi_scan_results;
}
bool ConnectivityService::save_display_wifi_credentials(const char *,
                                                        const char *) {
  return true;
}
struct WeatherService::Impl {};
WeatherService::WeatherService() : impl_(std::make_unique<Impl>()) {}
WeatherService::~WeatherService() = default;
WeatherService &weather_service() {
  static WeatherService service;
  return service;
}
WeatherServiceStatus WeatherService::status() const {
  return preview::weather_status;
}
bool WeatherService::snapshot(calendar::weather::WeatherSnapshot &value) const {
  value = preview::weather;
  return true;
}
bool WeatherService::request_refresh() { return true; }
WeatherLocationConfiguration WeatherService::location_configuration() const {
  WeatherLocationConfiguration c;
  c.api_key_configured = true;
  std::strcpy(c.latitude, "31.77");
  std::strcpy(c.longitude, "35.21");
  std::strcpy(c.location, "Jerusalem");
  c.units = preview::weather_status.units;
  return c;
}
bool WeatherService::save_location(const char *, const char *, const char *,
                                   WeatherUnits) {
  return true;
}
struct OtaService::Impl {};
OtaService::OtaService() : impl_(std::make_unique<Impl>()) {}
OtaService::~OtaService() = default;
OtaService &ota_service() {
  static OtaService service;
  return service;
}
OtaStatus OtaService::status() const { return preview::ota_status; }
bool OtaService::arm() {
  preview::ota_status.state = OtaState::Armed;
  std::strcpy(preview::ota_status.upload_url, "http://192.168.1.42/update");
  std::strcpy(preview::ota_status.one_time_code, "482731");
  preview::ota_status.seconds_remaining = 300;
  std::strcpy(preview::ota_status.detail, "Upload window open");
  return true;
}
bool OtaService::cancel() {
  preview::ota_status = {};
  std::strcpy(preview::ota_status.detail, "Firmware updates are disabled");
  return true;
}
bool OtaService::request_reboot() {
  return preview::ota_status.state == OtaState::ReadyToReboot;
}
} // namespace board
