#include "board/weather_service.hpp"

#include "app/weather_parser.hpp"
#include "board/connectivity_service.hpp"

#include <Arduino.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <esp_heap_caps.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string_view>

namespace board {
namespace {

// ESP-IDF NVS names are limited to 15 characters, excluding the terminator.
// The previous "calendar-weather" value was 16 characters, so
// Preferences::begin() always failed and the web form could never save.
constexpr char kPreferencesNamespace[] = "calendar-wx";
static_assert(sizeof(kPreferencesNamespace) - 1 <= 15,
              "Preferences namespace exceeds the ESP32 NVS limit");
constexpr std::size_t kMaxApiKeyBytes = 96;
constexpr std::size_t kMaxCoordinateBytes = 16;
constexpr std::size_t kMaxLocationBytes = 48;
constexpr std::uint32_t kRefreshIntervalMs = 15U * 60U * 1000U;
constexpr std::uint32_t kRetryIntervalMs = 60U * 1000U;
constexpr std::time_t kPlausibleUnixTime = 1700000000;
constexpr char kMetricUnitsValue[] = "metric";
constexpr char kImperialUnitsValue[] = "imperial";

// OpenWeather's currently served chain anchors at USERTrust RSA Certification
// Authority. Keeping the root explicit preserves certificate validation and
// avoids setInsecure(). Revalidate this root during dependency/API maintenance.
constexpr char kUserTrustRsaRoot[] = R"pem(-----BEGIN CERTIFICATE-----
MIIF3jCCA8agAwIBAgIQAf1tMPyjylGoG7xkDjUDLTANBgkqhkiG9w0BAQwFADCB
iDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0pl
cnNleSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNV
BAMTJVVTRVJUcnVzdCBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAw
MjAxMDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNV
BAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVU
aGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBSU0EgQ2Vy
dGlmaWNhdGlvbiBBdXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIK
AoICAQCAEmUXNg7D2wiz0KxXDXbtzSfTTK1Qg2HiqiBNCS1kCdzOiZ/MPans9s/B
3PHTsdZ7NygRK0faOca8Ohm0X6a9fZ2jY0K2dvKpOyuR+OJv0OwWIJAJPuLodMkY
tJHUYmTbf6MG8YgYapAiPLz+E/CHFHv25B+O1ORRxhFnRghRy4YUVD+8M/5+bJz/
Fp0YvVGONaanZshyZ9shZrHUm3gDwFA66Mzw3LyeTP6vBZY1H1dat//O+T23LLb2
VN3I5xI6Ta5MirdcmrS3ID3KfyI0rn47aGYBROcBTkZTmzNg95S+UzeQc0PzMsNT
79uq/nROacdrjGCT3sTHDN/hMq7MkztReJVni+49Vv4M0GkPGw/zJSZrM233bkf6
c0Plfg6lZrEpfDKEY1WJxA3Bk1QwGROs0303p+tdOmw1XNtB1xLaqUkL39iAigmT
Yo61Zs8liM2EuLE/pDkP2QKe6xJMlXzzawWpXhaDzLhn4ugTncxbgtNMs+1b/97l
c6wjOy0AvzVVdAlJ2ElYGn+SNuZRkg7zJn0cTRe8yexDJtC/QV9AqURE9JnnV4ee
UB9XVKg+/XRjL7FQZQnmWEIuQxpMtPAlR1n6BB6T1CZGSlCBst6+eLf8ZxXhyVeE
Hg9j1uliutZfVS7qXMYoCAQlObgOK6nyTJccBz8NUvXt7y+CDwIDAQABo0IwQDAd
BgNVHQ4EFgQUU3m/WqorSs9UgOHYm8Cd8rIDZsswDgYDVR0PAQH/BAQDAgEGMA8G
A1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAFzUfA3P9wF9QZllDHPF
Up/L+M+ZBn8b2kMVn54CVVeWFPFSPCeHlCjtHzoBN6J2/FNQwISbxmtOuowhT6KO
VWKR82kV2LyI48SqC/3vqOlLVSoGIG1VeCkZ7l8wXEskEVX/JJpuXior7gtNn3/3
ATiUFJVDBwn7YKnuHKsSjKCaXqeYalltiz8I+8jRRa8YFWSQEg9zKC7F4iRO/Fjs
8PRF/iKz6y+O0tlFYQXBl2+odnKPi4w2r78NBc5xjeambx9spnFixdjQg3IM8WcR
iQycE0xyNN+81XHfqnHd4blsjDwSXWXavVcStkNr/+XeTWYRUc+ZruwXtuhxkYze
Sf7dNXGiFSeUHM9h4ya7b6NnJSFd5t0dCy5oGzuCr+yDZ4XUmFF0sbmZgIn/f3gZ
XHlKYC6SQK5MNyosycdiyA5d9zZbyuAlJQG03RoHnHcAP9Dc1ew91Pq7P8yF1m9/
qS3fuQL39ZeatTXaw2ewh0qpKJ4jjv9cJ2vhsE/zB+4ALtRZh8tSQZXq9EfX7mRB
VXyNWQKV3WKdwrnuWih0hKWbt5DHDAff9Yk2dDLWKMGwsAvgnEzDHNb842m1R0aB
L6KCq9NjRHDEjf8tM7qtj3u1cIiuPhnPQCjY/MiQu12ZIvVS5ljFH4gxQ+6IHdfG
jjxDah2nGN59PRbxYvnKkKj9
-----END CERTIFICATE-----
)pem";

template <std::size_t Size>
void copy_text(char (&destination)[Size], const char* source) {
    std::snprintf(destination, Size, "%s", source == nullptr ? "" : source);
}

bool due(std::uint32_t now, std::uint32_t deadline) {
    return deadline == 0 || static_cast<std::int32_t>(now - deadline) >= 0;
}

bool valid_coordinate(const char* text, double minimum, double maximum) {
    if (text == nullptr || *text == '\0' || std::strlen(text) >= kMaxCoordinateBytes) return false;
    errno = 0;
    char* end = nullptr;
    const double value = std::strtod(text, &end);
    return errno == 0 && end != text && *end == '\0' && std::isfinite(value) &&
           value >= minimum && value <= maximum;
}

const char* stored_units_value(WeatherUnits units) {
    return units == WeatherUnits::Imperial ? kImperialUnitsValue : kMetricUnitsValue;
}

WeatherUnits parse_stored_units(const char* value) {
    return value != nullptr && std::strcmp(value, kImperialUnitsValue) == 0
               ? WeatherUnits::Imperial
               : WeatherUnits::Metric;
}

class PsramText {
public:
    ~PsramText() { if (data_ != nullptr) heap_caps_free(data_); }
    bool initialize() {
        data_ = static_cast<char*>(heap_caps_malloc(calendar::weather::kMaxWeatherJsonBytes + 1,
                                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        return data_ != nullptr;
    }
    bool append(const std::uint8_t* bytes, std::size_t count) {
        if (data_ == nullptr || bytes == nullptr || count > calendar::weather::kMaxWeatherJsonBytes - size_) {
            overflowed_ = true;
            return false;
        }
        std::memcpy(data_ + size_, bytes, count);
        size_ += count;
        data_[size_] = '\0';
        return true;
    }
    std::string_view view() const { return {data_, size_}; }
    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    bool overflowed() const { return overflowed_; }
private:
    char* data_ = nullptr;
    std::size_t size_ = 0;
    bool overflowed_ = false;
};

class BoundedWeatherSink final : public Stream {
public:
    explicit BoundedWeatherSink(PsramText& text) : text_(text) {}
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
    size_t write(uint8_t byte) override { return write(&byte, 1); }
    size_t write(const uint8_t* bytes, size_t count) override {
        return text_.append(bytes, count) ? count : 0;
    }
private:
    PsramText& text_;
};

const char* parse_error_text(calendar::weather::WeatherParseError error) {
    using calendar::weather::WeatherParseError;
    switch (error) {
        case WeatherParseError::None: return "Weather updated";
        case WeatherParseError::InputTooLarge: return "Weather response exceeded 64 KiB";
        case WeatherParseError::MalformedJson: return "Weather server returned malformed JSON";
        case WeatherParseError::MissingRequiredField: return "Weather response is missing required data";
        case WeatherParseError::TooManyForecastEntries: return "Weather forecast contained too many entries";
    }
    return "Weather response could not be parsed";
}

enum class DownloadResult {
    Success,
    Unauthorized,
    Failure,
};

}  // namespace

struct WeatherService::Impl {
    struct Configuration {
        char api_key[kMaxApiKeyBytes + 1]{};
        char latitude[kMaxCoordinateBytes]{};
        char longitude[kMaxCoordinateBytes]{};
        char location[kMaxLocationBytes + 1]{};
        WeatherUnits units = WeatherUnits::Metric;
        bool valid() const { return api_key[0] && latitude[0] && longitude[0]; }
    };

    Preferences preferences;
    SemaphoreHandle_t lock = nullptr;
    WeatherServiceStatus status{};
    calendar::weather::WeatherSnapshot snapshot{};
    Configuration configuration{};
    std::uint32_t configuration_generation = 1;
    std::uint32_t snapshot_configuration_generation = 0;
    bool initialized = false;
    bool startup_attempted = false;
    bool startup_succeeded = false;
    bool worker_running = false;
    bool refresh_pending = false;
    std::uint32_t next_refresh_ms = 0;

    ~Impl() { if (lock != nullptr) vSemaphoreDelete(lock); }

    void set_detail(const char* detail) { copy_text(status.detail, detail); }

    void load_configuration() {
        preferences.begin(kPreferencesNamespace, true);
        if (preferences.isKey("api_key")) {
            preferences.getString("api_key", configuration.api_key,
                                  sizeof(configuration.api_key));
        }
        if (preferences.isKey("latitude")) {
            preferences.getString("latitude", configuration.latitude,
                                  sizeof(configuration.latitude));
        }
        if (preferences.isKey("longitude")) {
            preferences.getString("longitude", configuration.longitude,
                                  sizeof(configuration.longitude));
        }
        if (preferences.isKey("location")) {
            preferences.getString("location", configuration.location,
                                  sizeof(configuration.location));
        }
        char saved_units[12]{};
        if (preferences.isKey("units")) {
            preferences.getString("units", saved_units, sizeof(saved_units));
        }
        preferences.end();
        // Missing or unrecognized values deliberately migrate to metric.
        configuration.units = parse_stored_units(saved_units);
        status.units = configuration.units;
        status.configured = configuration.valid();
        copy_text(status.location, configuration.location[0] ? configuration.location : "Home");
        status.state = status.configured ? WeatherServiceState::WaitingForNetwork
                                         : WeatherServiceState::NotConfigured;
        set_detail(status.configured ? "Waiting to download weather"
                                     : "Add OpenWeather settings on the home-LAN page");
        refresh_pending = status.configured;
        status.refresh_pending = refresh_pending;
    }

    DownloadResult download(const char* url, PsramText& body, char (&detail)[96]) {
        if (!body.initialize()) {
            copy_text(detail, "No PSRAM available for weather response");
            return DownloadResult::Failure;
        }
        WiFiClientSecure client;
        client.setCACert(kUserTrustRsaRoot);
        HTTPClient http;
        http.setConnectTimeout(12000);
        http.setTimeout(12000);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        if (!http.begin(client, url)) {
            copy_text(detail, "Could not start weather HTTPS connection");
            return DownloadResult::Failure;
        }
        const int response = http.GET();
        if (response != HTTP_CODE_OK) {
            if (response > 0) std::snprintf(detail, sizeof(detail), "Weather server returned HTTP %d", response);
            else copy_text(detail, "Weather HTTPS transport failed");
            http.end();
            return response == HTTP_CODE_UNAUTHORIZED || response == HTTP_CODE_FORBIDDEN
                       ? DownloadResult::Unauthorized
                       : DownloadResult::Failure;
        }
        const int length = http.getSize();
        if (length > static_cast<int>(calendar::weather::kMaxWeatherJsonBytes)) {
            copy_text(detail, "Weather response exceeded 64 KiB");
            http.end();
            return DownloadResult::Failure;
        }
        BoundedWeatherSink sink(body);
        const int written = http.writeToStream(&sink);
        http.end();
        if (written < 0 || body.overflowed() || body.empty() ||
            (length >= 0 && body.size() != static_cast<std::size_t>(length))) {
            copy_text(detail, body.overflowed() ? "Weather response exceeded 64 KiB"
                                               : "Weather response was incomplete");
            return DownloadResult::Failure;
        }
        return DownloadResult::Success;
    }

    void run_fetch() {
        Configuration config{};
        calendar::weather::WeatherSnapshot previous{};
        std::uint32_t fetch_generation = 0;
        xSemaphoreTake(lock, portMAX_DELAY);
        config = configuration;
        previous = snapshot;
        fetch_generation = configuration_generation;
        xSemaphoreGive(lock);

        char one_call_url[352]{};
        char current_url[320]{};
        char forecast_url[320]{};
        // Keep downloaded and parsed snapshots in one canonical unit system.
        // The UI converts these metric values according to the saved preference.
        std::snprintf(one_call_url, sizeof(one_call_url),
                      "https://api.openweathermap.org/data/3.0/onecall?lat=%s&lon=%s&units=metric&lang=en&exclude=minutely,hourly,alerts&appid=%s",
                      config.latitude, config.longitude, config.api_key);
        std::snprintf(current_url, sizeof(current_url),
                      "https://api.openweathermap.org/data/2.5/weather?lat=%s&lon=%s&units=metric&lang=en&appid=%s",
                      config.latitude, config.longitude, config.api_key);
        std::snprintf(forecast_url, sizeof(forecast_url),
                      "https://api.openweathermap.org/data/2.5/forecast?lat=%s&lon=%s&units=metric&lang=en&appid=%s",
                      config.latitude, config.longitude, config.api_key);

        char detail[96]{};
        const bool changing_location = fetch_generation != snapshot_configuration_generation;
        calendar::weather::WeatherSnapshot candidate =
            changing_location ? calendar::weather::WeatherSnapshot{} : previous;
        bool current_ok = false;
        bool forecast_ok = false;
        bool used_five_day_fallback = false;
        DownloadResult one_call_result = DownloadResult::Failure;
        {
            PsramText one_call_body;
            one_call_result = download(one_call_url, one_call_body, detail);
            if (one_call_result == DownloadResult::Success) {
                const auto parsed = calendar::weather::parse_one_call_weather_json(
                    one_call_body.view(), candidate);
                if (parsed) {
                    candidate = parsed.snapshot;
                    current_ok = true;
                    forecast_ok = true;
                } else {
                    copy_text(detail, parse_error_text(parsed.error));
                }
            }
        }
        if (one_call_result == DownloadResult::Unauthorized) {
            // API keys without a One Call 3.0 subscription retain the original
            // endpoints. This intentionally provides five days; the remaining
            // two UI slots stay unavailable rather than inventing forecasts.
            used_five_day_fallback = true;
            PsramText current_body;
            if (download(current_url, current_body, detail) == DownloadResult::Success) {
                const auto parsed = calendar::weather::parse_current_weather_json(
                    current_body.view(), candidate);
                if (parsed) {
                    candidate = parsed.snapshot;
                    current_ok = true;
                } else {
                    copy_text(detail, parse_error_text(parsed.error));
                }
            }
            PsramText forecast_body;
            if (download(forecast_url, forecast_body, detail) == DownloadResult::Success) {
                const auto parsed = calendar::weather::parse_forecast_weather_json(
                    forecast_body.view(), candidate);
                if (parsed) {
                    candidate = parsed.snapshot;
                    forecast_ok = true;
                } else {
                    copy_text(detail, parse_error_text(parsed.error));
                }
            }
        }

        xSemaphoreTake(lock, portMAX_DELAY);
        if (fetch_generation != configuration_generation) {
            status.state = status.snapshot_available ? WeatherServiceState::Stale
                                                     : WeatherServiceState::WaitingForNetwork;
            set_detail("Weather settings changed; restarting download");
            next_refresh_ms = 0;
            refresh_pending = true;
            worker_running = false;
            status.refresh_pending = true;
            xSemaphoreGive(lock);
            connectivity_service().end_network_activity(NetworkActivity::WeatherDownload);
            return;
        }
        const bool any_success = current_ok || forecast_ok;
        if (changing_location && !(current_ok && forecast_ok)) {
            status.state = status.snapshot_available ? WeatherServiceState::Stale
                                                     : WeatherServiceState::Error;
            set_detail(status.snapshot_available
                           ? "New location update incomplete; previous location retained"
                           : (detail[0] ? detail : "Weather download failed"));
            next_refresh_ms = millis() + kRetryIntervalMs;
        } else if (any_success) {
            if (candidate != snapshot) {
                snapshot = candidate;
                ++status.generation;
            }
            if (changing_location) {
                snapshot_configuration_generation = fetch_generation;
                copy_text(status.location, config.location[0] ? config.location : "Home");
            }
            if (!used_five_day_fallback || forecast_ok) {
                status.five_day_fallback = used_five_day_fallback;
            }
            status.snapshot_available = snapshot.current.available || snapshot.forecast_count != 0;
            status.updated_utc = static_cast<std::int64_t>(std::time(nullptr));
            status.state = current_ok && forecast_ok ? WeatherServiceState::Ready
                                                      : WeatherServiceState::Stale;
            if (current_ok && forecast_ok && used_five_day_fallback) {
                set_detail("Using 5-day forecast; One Call access unavailable");
            } else {
                set_detail(current_ok && forecast_ok ? "Weather updated"
                                                      : "Weather partly updated; cached data retained");
            }
            next_refresh_ms = millis() + kRefreshIntervalMs;
        } else {
            status.state = status.snapshot_available ? WeatherServiceState::Stale
                                                     : WeatherServiceState::Error;
            set_detail(detail[0] ? detail : "Weather download failed");
            next_refresh_ms = millis() + kRetryIntervalMs;
        }
        worker_running = false;
        status.refresh_pending = false;
        xSemaphoreGive(lock);
        connectivity_service().end_network_activity(NetworkActivity::WeatherDownload);
    }

    static void fetch_task(void* argument) {
        static_cast<Impl*>(argument)->run_fetch();
        vTaskDelete(nullptr);
    }
};

WeatherService::WeatherService() : impl_(new Impl()) {}
WeatherService::~WeatherService() = default;

bool WeatherService::initialize() {
    if (impl_->startup_attempted) return impl_->startup_succeeded;
    impl_->startup_attempted = true;
    impl_->lock = xSemaphoreCreateMutex();
    if (impl_->lock == nullptr) {
        impl_->status.state = WeatherServiceState::Error;
        impl_->set_detail("No memory for weather service");
        return false;
    }
    impl_->load_configuration();
    impl_->initialized = true;
    impl_->startup_succeeded = true;
    return true;
}

void WeatherService::tick() {
    if (!impl_->initialized || impl_->lock == nullptr) return;
    const std::uint32_t now = millis();
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    const bool should_start = impl_->status.configured && !impl_->worker_running &&
                              (impl_->refresh_pending || due(now, impl_->next_refresh_ms));
    xSemaphoreGive(impl_->lock);
    if (!should_start) return;
    if (WiFi.status() != WL_CONNECTED || std::time(nullptr) < kPlausibleUnixTime) {
        xSemaphoreTake(impl_->lock, portMAX_DELAY);
        impl_->status.state = impl_->status.snapshot_available ? WeatherServiceState::Stale
                                                               : WeatherServiceState::WaitingForNetwork;
        impl_->set_detail("Waiting for Wi-Fi and network time");
        xSemaphoreGive(impl_->lock);
        return;
    }
    if (!connectivity_service().try_begin_network_activity(NetworkActivity::WeatherDownload)) return;
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    if (impl_->worker_running) {
        xSemaphoreGive(impl_->lock);
        connectivity_service().end_network_activity(NetworkActivity::WeatherDownload);
        return;
    }
    impl_->worker_running = true;
    impl_->refresh_pending = false;
    impl_->status.refresh_pending = false;
    impl_->status.state = WeatherServiceState::Loading;
    impl_->set_detail(impl_->status.snapshot_available ? "Updating weather"
                                                       : "Downloading weather");
    xSemaphoreGive(impl_->lock);
    TaskHandle_t task = nullptr;
    if (xTaskCreatePinnedToCore(Impl::fetch_task, "weather-fetch", 9216, impl_.get(), 1, &task, 0) != pdPASS) {
        xSemaphoreTake(impl_->lock, portMAX_DELAY);
        impl_->worker_running = false;
        impl_->status.state = WeatherServiceState::Error;
        impl_->set_detail("Could not start weather task");
        xSemaphoreGive(impl_->lock);
        connectivity_service().end_network_activity(NetworkActivity::WeatherDownload);
    }
}

bool WeatherService::save_configuration(const char* api_key, const char* latitude,
                                        const char* longitude, const char* location,
                                        WeatherUnits units) {
    if (!impl_->initialized || impl_->lock == nullptr) return false;
    const auto reject = [this](const char* detail) {
        xSemaphoreTake(impl_->lock, portMAX_DELAY);
        impl_->set_detail(detail);
        xSemaphoreGive(impl_->lock);
        return false;
    };
    const bool new_api_key_supplied = api_key != nullptr && *api_key != '\0';
    if (new_api_key_supplied &&
        (std::strlen(api_key) < 8 || std::strlen(api_key) > kMaxApiKeyBytes)) {
        return reject("API key must contain 8 to 96 characters");
    }
    if (!valid_coordinate(latitude, -90.0, 90.0)) {
        return reject("Latitude must be a number from -90 to 90");
    }
    if (!valid_coordinate(longitude, -180.0, 180.0)) {
        return reject("Longitude must be a number from -180 to 180");
    }
    if (location != nullptr && std::strlen(location) > kMaxLocationBytes) {
        return reject("Location label must be 48 characters or fewer");
    }
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    Impl::Configuration candidate = impl_->configuration;
    if (!new_api_key_supplied && candidate.api_key[0] == '\0') {
        impl_->set_detail("Enter an OpenWeather API key");
        xSemaphoreGive(impl_->lock);
        return false;
    }
    if (new_api_key_supplied) copy_text(candidate.api_key, api_key);
    copy_text(candidate.latitude, latitude);
    copy_text(candidate.longitude, longitude);
    copy_text(candidate.location, (location != nullptr && *location) ? location : "Home");
    candidate.units = units;
    const bool source_changed =
        std::strcmp(candidate.api_key, impl_->configuration.api_key) != 0 ||
        std::strcmp(candidate.latitude, impl_->configuration.latitude) != 0 ||
        std::strcmp(candidate.longitude, impl_->configuration.longitude) != 0 ||
        std::strcmp(candidate.location, impl_->configuration.location) != 0;
    const bool units_changed = candidate.units != impl_->configuration.units;
    if (!impl_->preferences.begin(kPreferencesNamespace, false)) {
        impl_->set_detail("Could not open weather settings storage");
        xSemaphoreGive(impl_->lock);
        return false;
    }
    const bool saved = impl_->preferences.putString("api_key", candidate.api_key) > 0 &&
                       impl_->preferences.putString("latitude", candidate.latitude) > 0 &&
                       impl_->preferences.putString("longitude", candidate.longitude) > 0 &&
                       impl_->preferences.putString("location", candidate.location) > 0 &&
                       impl_->preferences.putString("units", stored_units_value(candidate.units)) > 0;
    impl_->preferences.end();
    if (!saved) {
        impl_->set_detail("Could not save weather settings");
        xSemaphoreGive(impl_->lock);
        return false;
    }
    impl_->configuration = candidate;
    impl_->status.configured = true;
    impl_->status.units = candidate.units;
    if (!source_changed) {
        impl_->set_detail(units_changed ? "Weather units saved" : "Weather settings unchanged");
        xSemaphoreGive(impl_->lock);
        return true;
    }
    ++impl_->configuration_generation;
    if (!impl_->status.snapshot_available) copy_text(impl_->status.location, candidate.location);
    impl_->status.state = impl_->status.snapshot_available ? WeatherServiceState::Stale
                                                           : WeatherServiceState::WaitingForNetwork;
    impl_->refresh_pending = true;
    impl_->status.refresh_pending = true;
    impl_->next_refresh_ms = 0;
    impl_->set_detail(impl_->status.snapshot_available
                          ? "Weather settings saved; previous location retained until refresh"
                          : "Weather settings saved; refresh queued");
    xSemaphoreGive(impl_->lock);
    return true;
}

bool WeatherService::save_location(const char* latitude, const char* longitude,
                                   const char* location, WeatherUnits units) {
    if (!impl_->initialized || impl_->lock == nullptr ||
        !valid_coordinate(latitude, -90.0, 90.0) ||
        !valid_coordinate(longitude, -180.0, 180.0) ||
        location == nullptr || *location == '\0' ||
        std::strlen(location) > kMaxLocationBytes) {
        return false;
    }
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    if (impl_->configuration.api_key[0] == '\0') {
        xSemaphoreGive(impl_->lock);
        return false;
    }
    Impl::Configuration candidate = impl_->configuration;
    copy_text(candidate.latitude, latitude);
    copy_text(candidate.longitude, longitude);
    copy_text(candidate.location, location);
    candidate.units = units;
    const bool source_changed =
        std::strcmp(candidate.latitude, impl_->configuration.latitude) != 0 ||
        std::strcmp(candidate.longitude, impl_->configuration.longitude) != 0 ||
        std::strcmp(candidate.location, impl_->configuration.location) != 0;
    const bool units_changed = candidate.units != impl_->configuration.units;
    if (impl_->worker_running && source_changed) {
        xSemaphoreGive(impl_->lock);
        return false;
    }
    if (!impl_->preferences.begin(kPreferencesNamespace, false)) {
        impl_->status.state = WeatherServiceState::Error;
        impl_->set_detail("Could not open weather location storage");
        xSemaphoreGive(impl_->lock);
        return false;
    }
    const bool saved = impl_->preferences.putString("latitude", candidate.latitude) > 0 &&
                       impl_->preferences.putString("longitude", candidate.longitude) > 0 &&
                       impl_->preferences.putString("location", candidate.location) > 0 &&
                       impl_->preferences.putString("units", stored_units_value(candidate.units)) > 0;
    impl_->preferences.end();
    if (saved) {
        impl_->configuration = candidate;
        if (source_changed) ++impl_->configuration_generation;
    }
    const bool configured = saved && impl_->configuration.valid();
    impl_->status.configured = configured;
    if (saved) impl_->status.units = candidate.units;
    if (configured && !source_changed) {
        impl_->set_detail(units_changed ? "Weather units saved" : "Weather settings unchanged");
        xSemaphoreGive(impl_->lock);
        return true;
    }
    if (configured && !impl_->status.snapshot_available) {
        copy_text(impl_->status.location, impl_->configuration.location);
    }
    impl_->status.state = configured
        ? (impl_->status.snapshot_available ? WeatherServiceState::Stale
                                            : WeatherServiceState::WaitingForNetwork)
        : WeatherServiceState::Error;
    impl_->refresh_pending = impl_->status.configured;
    impl_->status.refresh_pending = impl_->refresh_pending;
    impl_->next_refresh_ms = 0;
    impl_->set_detail(impl_->status.configured
                          ? (impl_->status.snapshot_available
                                 ? "Location saved; previous weather shown until refresh"
                                 : "Weather location saved; refresh queued")
                          : "Could not save weather location");
    xSemaphoreGive(impl_->lock);
    return configured;
}

bool WeatherService::request_refresh() {
    if (!impl_->initialized || impl_->lock == nullptr) return false;
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    const bool accepted = impl_->status.configured && !impl_->worker_running;
    if (accepted) {
        impl_->refresh_pending = true;
        impl_->status.refresh_pending = true;
        impl_->next_refresh_ms = 0;
    }
    xSemaphoreGive(impl_->lock);
    return accepted;
}

WeatherServiceStatus WeatherService::status() const {
    WeatherServiceStatus result{};
    if (impl_->lock == nullptr) return impl_->status;
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    result = impl_->status;
    xSemaphoreGive(impl_->lock);
    return result;
}

WeatherLocationConfiguration WeatherService::location_configuration() const {
    WeatherLocationConfiguration result{};
    if (impl_->lock == nullptr) return result;
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    result.api_key_configured = impl_->configuration.api_key[0] != '\0';
    result.units = impl_->configuration.units;
    copy_text(result.latitude, impl_->configuration.latitude);
    copy_text(result.longitude, impl_->configuration.longitude);
    copy_text(result.location, impl_->configuration.location);
    xSemaphoreGive(impl_->lock);
    return result;
}

bool WeatherService::snapshot(calendar::weather::WeatherSnapshot& destination) const {
    if (impl_->lock == nullptr) return false;
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    const bool available = impl_->status.snapshot_available;
    if (available) destination = impl_->snapshot;
    xSemaphoreGive(impl_->lock);
    return available;
}

WeatherService& weather_service() {
    static WeatherService service;
    return service;
}

}  // namespace board
