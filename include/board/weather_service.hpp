#pragma once

#include "app/weather_types.hpp"

#include <cstdint>
#include <memory>

namespace board {

enum class WeatherServiceState {
    NotConfigured,
    WaitingForNetwork,
    Loading,
    Ready,
    Stale,
    Error,
};

enum class WeatherUnits {
    Metric,
    Imperial,
};

struct WeatherServiceStatus {
    WeatherServiceState state = WeatherServiceState::NotConfigured;
    WeatherUnits units = WeatherUnits::Metric;
    bool configured = false;
    bool snapshot_available = false;
    // True when One Call 3.0 was unavailable to this API key and the service is
    // using the compatible current + five-day endpoints instead.
    bool five_day_fallback = false;
    bool refresh_pending = false;
    std::uint32_t generation = 0;
    std::int64_t updated_utc = 0;
    char location[49]{};
    char detail[96]{};
};

// Safe configuration view for the on-device editor. The OpenWeather API key
// deliberately never leaves WeatherService.
struct WeatherLocationConfiguration {
    bool api_key_configured = false;
    WeatherUnits units = WeatherUnits::Metric;
    char latitude[16]{};
    char longitude[16]{};
    char location[49]{};
};

class WeatherService {
public:
    WeatherService();
    ~WeatherService();

    WeatherService(const WeatherService&) = delete;
    WeatherService& operator=(const WeatherService&) = delete;

    // Returns false only when required local startup resources could not be
    // created. Missing configuration or network access remains a valid state.
    bool initialize();
    void tick();

    // Values come from the trusted-home-LAN setup page. An empty API key keeps
    // the existing saved key; initial setup requires one. The key is stored in
    // NVS but is never returned by status(), logged, or displayed.
    bool save_configuration(const char* api_key, const char* latitude,
                            const char* longitude, const char* location,
                            WeatherUnits units);
    // Updates only the display label and coordinates while preserving the
    // already-stored API key. Returns false if no key exists, validation fails,
    // or a weather download is currently using the old location.
    bool save_location(const char* latitude, const char* longitude,
                       const char* location, WeatherUnits units);
    bool request_refresh();

    WeatherServiceStatus status() const;
    WeatherLocationConfiguration location_configuration() const;
    bool snapshot(calendar::weather::WeatherSnapshot& destination) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

WeatherService& weather_service();

}  // namespace board
