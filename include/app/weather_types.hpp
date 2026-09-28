#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace calendar::weather {

constexpr std::size_t kMaxForecastDays = 7;
constexpr std::size_t kMaxWeatherDescriptionBytes = 47;
constexpr std::size_t kMaxWeatherJsonBytes = 64 * 1024;

struct LocalDate {
    std::int16_t year = 0;
    std::uint8_t month = 0;
    std::uint8_t day = 0;
};

struct CurrentConditions {
    bool available = false;
    std::int16_t temperature_tenths_c = 0;
    std::int16_t feels_like_tenths_c = 0;
    std::uint8_t humidity_percent = 0;
    std::int16_t wind_tenths_mps = 0;
    std::int16_t condition_id = 0;
    std::array<char, kMaxWeatherDescriptionBytes + 1> description{};
    std::int64_t observed_utc = 0;
};

struct ForecastDay {
    LocalDate date{};
    std::int16_t minimum_tenths_c = 0;
    std::int16_t maximum_tenths_c = 0;
    std::uint8_t maximum_precipitation_probability_percent = 0;
    std::int16_t dominant_condition_id = 0;
    std::array<char, kMaxWeatherDescriptionBytes + 1> description{};
};

struct WeatherSnapshot {
    CurrentConditions current{};
    std::array<ForecastDay, kMaxForecastDays> forecast{};
    std::uint8_t forecast_count = 0;
    std::int32_t timezone_offset_seconds = 0;
};

bool operator==(const LocalDate& lhs, const LocalDate& rhs);
bool operator==(const CurrentConditions& lhs, const CurrentConditions& rhs);
bool operator==(const ForecastDay& lhs, const ForecastDay& rhs);
bool operator==(const WeatherSnapshot& lhs, const WeatherSnapshot& rhs);

inline bool operator!=(const WeatherSnapshot& lhs, const WeatherSnapshot& rhs) {
    return !(lhs == rhs);
}

}  // namespace calendar::weather
