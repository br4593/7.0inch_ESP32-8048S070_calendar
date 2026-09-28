#pragma once

#include "app/weather_types.hpp"

#include <string_view>

namespace calendar::weather {

enum class WeatherParseError {
    None,
    InputTooLarge,
    MalformedJson,
    MissingRequiredField,
    TooManyForecastEntries,
};

struct WeatherParseResult {
    WeatherParseError error = WeatherParseError::None;
    WeatherSnapshot snapshot{};

    explicit operator bool() const { return error == WeatherParseError::None; }
};

// On success, the parsed component replaces the corresponding component in
// base. On failure, result.snapshot remains equal to base.
WeatherParseResult parse_current_weather_json(
    std::string_view json, const WeatherSnapshot& base = {});
WeatherParseResult parse_forecast_weather_json(
    std::string_view json, const WeatherSnapshot& base = {});
// Parses OpenWeather One Call 3.0 current conditions and daily forecast as one
// atomic snapshot. Up to seven daily entries are retained; additional entries
// are intentionally ignored.
WeatherParseResult parse_one_call_weather_json(
    std::string_view json, const WeatherSnapshot& base = {});

}  // namespace calendar::weather
