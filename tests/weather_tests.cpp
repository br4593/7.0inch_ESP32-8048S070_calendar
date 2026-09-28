#include "app/weather_parser.hpp"

#include <iostream>
#include <string>

namespace {
using namespace calendar::weather;

int failures = 0;
#define CHECK(...) do { if (!(__VA_ARGS__)) { ++failures; std::cerr << __FUNCTION__ << ": " #__VA_ARGS__ " failed at " << __LINE__ << '\n'; } } while (false)

void test_current_valid_and_negative_temperature() {
    const auto result = parse_current_weather_json(R"json({
        "weather":[{"id":501,"description":"moderate rain","icon":"10d"}],
        "main":{"temp":-1.25,"feels_like":-3.04,"humidity":87,"pressure":1004},
        "wind":{"speed":4.26,"deg":120},"dt":1700000000,
        "rain":{"1h":2.1},"name":"Home"})json");
    CHECK(result);
    CHECK(result.snapshot.current.available);
    CHECK(result.snapshot.current.temperature_tenths_c == -13);
    CHECK(result.snapshot.current.feels_like_tenths_c == -30);
    CHECK(result.snapshot.current.humidity_percent == 87);
    CHECK(result.snapshot.current.wind_tenths_mps == 43);
    CHECK(result.snapshot.current.condition_id == 501);
    CHECK(std::string(result.snapshot.current.description.data()) == "moderate rain");
    CHECK(result.snapshot.current.observed_utc == 1700000000);
}

void test_missing_optional_fields() {
    const auto current = parse_current_weather_json(
        R"json({"weather":[{"id":800}],"main":{"temp":20,"feels_like":20,"humidity":40},"wind":{"speed":0},"dt":1700000000})json");
    CHECK(current);
    CHECK(current.snapshot.current.description[0] == '\0');

    const auto forecast = parse_forecast_weather_json(
        R"json({"list":[{"dt":1700000000,"main":{"temp_min":10,"temp_max":12},"weather":[{"id":800}]}],"city":{"timezone":7200}})json");
    CHECK(forecast);
    CHECK(forecast.snapshot.forecast_count == 1);
    CHECK(forecast.snapshot.forecast[0].maximum_precipitation_probability_percent == 0);
    CHECK(forecast.snapshot.forecast[0].description[0] == '\0');
}

void test_forecast_grouping_and_dominant_condition() {
    const auto result = parse_forecast_weather_json(R"json({
      "list":[
        {"dt":1704060000,"main":{"temp_min":8.4,"temp_max":11.1},"weather":[{"id":800,"description":"clear"}],"pop":0.1},
        {"dt":1704070800,"main":{"temp_min":7.0,"temp_max":12.6},"weather":[{"id":500,"description":"light rain"}],"pop":0.7},
        {"dt":1704081600,"main":{"temp_min":6.2,"temp_max":10.0},"weather":[{"id":500,"description":"light rain"}],"pop":0.4},
        {"dt":1704153600,"main":{"temp_min":-2.2,"temp_max":4.4},"weather":[{"id":600,"description":"snow"}],"pop":1.0}
      ],
      "city":{"timezone":7200,"name":"Test"}
    })json");
    CHECK(result);
    CHECK(result.snapshot.timezone_offset_seconds == 7200);
    CHECK(result.snapshot.forecast_count == 2);
    const auto& first = result.snapshot.forecast[0];
    CHECK(first.date == LocalDate{2024, 1, 1});
    CHECK(first.minimum_tenths_c == 62);
    CHECK(first.maximum_tenths_c == 126);
    CHECK(first.maximum_precipitation_probability_percent == 70);
    CHECK(first.dominant_condition_id == 500);
    CHECK(std::string(first.description.data()) == "light rain");
    const auto& second = result.snapshot.forecast[1];
    CHECK(second.date == LocalDate{2024, 1, 2});
    CHECK(second.minimum_tenths_c == -22);
    CHECK(second.maximum_precipitation_probability_percent == 100);
}

void test_five_day_bound_and_semantic_equality() {
    std::string json = "{\"list\":[";
    for (int day = 0; day < 7; ++day) {
        if (day != 0) json += ',';
        json += "{\"dt\":" + std::to_string(1704067200 + day * 86400) +
                ",\"main\":{\"temp_min\":1,\"temp_max\":2},\"weather\":[{\"id\":800,\"description\":\"clear\"}]}";
    }
    json += "],\"city\":{\"timezone\":0}}";
    const auto result = parse_forecast_weather_json(json);
    CHECK(result);
    CHECK(result.snapshot.forecast_count == 5);
    WeatherSnapshot copy = result.snapshot;
    CHECK(copy == result.snapshot);
    copy.forecast[4].maximum_tenths_c++;
    CHECK(copy != result.snapshot);
}

std::string one_call_fixture(int days, int timezone_offset = 7200) {
    std::string json =
        "{\"timezone_offset\":" + std::to_string(timezone_offset) +
        ",\"current\":{\"dt\":1704060000,\"temp\":21.25,\"feels_like\":20.75,"
        "\"humidity\":54,\"wind_speed\":3.26,\"weather\":[{\"id\":801,"
        "\"description\":\"few clouds\"}]},\"daily\":[";
    for (int day = 0; day < days; ++day) {
        if (day != 0) json += ',';
        json += "{\"dt\":" + std::to_string(1704060000 + day * 86400) +
                ",\"temp\":{\"min\":" + std::to_string(10 + day) +
                ",\"max\":" + std::to_string(20 + day) +
                "},\"weather\":[{\"id\":800,\"description\":\"clear\"}],"
                "\"pop\":0.25}";
    }
    return json + "]}";
}

void test_one_call_seven_days_and_truncation() {
    const auto result = parse_one_call_weather_json(one_call_fixture(8));
    CHECK(result);
    CHECK(result.snapshot.current.available);
    CHECK(result.snapshot.current.temperature_tenths_c == 213);
    CHECK(result.snapshot.current.feels_like_tenths_c == 208);
    CHECK(result.snapshot.current.wind_tenths_mps == 33);
    CHECK(result.snapshot.current.condition_id == 801);
    CHECK(result.snapshot.timezone_offset_seconds == 7200);
    CHECK(result.snapshot.forecast_count == 7);
    CHECK(result.snapshot.forecast[0].date == LocalDate{2024, 1, 1});
    CHECK(result.snapshot.forecast[6].date == LocalDate{2024, 1, 7});
    CHECK(result.snapshot.forecast[6].minimum_tenths_c == 160);
    CHECK(result.snapshot.forecast[6].maximum_tenths_c == 260);
    CHECK(result.snapshot.forecast[6].maximum_precipitation_probability_percent == 25);
}

void test_one_call_timezone_and_partial_days() {
    const auto west = parse_one_call_weather_json(one_call_fixture(2, -10800));
    CHECK(west);
    CHECK(west.snapshot.forecast_count == 2);
    CHECK(west.snapshot.forecast[0].date == LocalDate{2023, 12, 31});
    CHECK(west.snapshot.forecast[1].date == LocalDate{2024, 1, 1});
}

void test_one_call_errors_preserve_base() {
    WeatherSnapshot base{};
    base.current.available = true;
    base.current.temperature_tenths_c = 99;
    const auto missing = parse_one_call_weather_json(R"json({
      "timezone_offset":0,
      "current":{"dt":1704060000,"temp":1,"feels_like":1,"humidity":50,
                 "wind_speed":1,"weather":[{"id":800}]},
      "daily":[{"dt":1704060000,"temp":{"min":1},"weather":[{"id":800}]}]
    })json", base);
    CHECK(!missing);
    CHECK(missing.error == WeatherParseError::MissingRequiredField);
    CHECK(missing.snapshot == base);

    const auto missing_current = parse_one_call_weather_json(R"json({
      "timezone_offset":0,
      "current":{"dt":1704060000,"temp":1,"humidity":50,"wind_speed":1,
                 "weather":[{"id":800}]},
      "daily":[{"dt":1704060000,"temp":{"min":1,"max":2},
                 "weather":[{"id":800}]}]
    })json", base);
    CHECK(!missing_current);
    CHECK(missing_current.error == WeatherParseError::MissingRequiredField);
    CHECK(missing_current.snapshot == base);

    const auto malformed = parse_one_call_weather_json("{\"timezone_offset\":0", base);
    CHECK(!malformed);
    CHECK(malformed.error == WeatherParseError::MalformedJson);
    CHECK(malformed.snapshot == base);

    std::string oversized(kMaxWeatherJsonBytes + 1, ' ');
    const auto too_large = parse_one_call_weather_json(oversized, base);
    CHECK(!too_large);
    CHECK(too_large.error == WeatherParseError::InputTooLarge);
    CHECK(too_large.snapshot == base);
}

void test_errors_preserve_base() {
    WeatherSnapshot base{};
    base.current.available = true;
    base.current.temperature_tenths_c = 215;
    auto malformed = parse_current_weather_json("{bad", base);
    CHECK(!malformed);
    CHECK(malformed.error == WeatherParseError::MalformedJson);
    CHECK(malformed.snapshot == base);

    auto missing = parse_current_weather_json(
        R"json({"weather":[{"id":800}],"main":{"temp":20},"wind":{"speed":1},"dt":1700000000})json", base);
    CHECK(!missing);
    CHECK(missing.error == WeatherParseError::MissingRequiredField);
    CHECK(missing.snapshot == base);

    std::string oversized(kMaxWeatherJsonBytes + 1, ' ');
    auto too_large = parse_forecast_weather_json(oversized, base);
    CHECK(!too_large);
    CHECK(too_large.error == WeatherParseError::InputTooLarge);
    CHECK(too_large.snapshot == base);
}

}  // namespace

int main() {
    test_current_valid_and_negative_temperature();
    test_missing_optional_fields();
    test_forecast_grouping_and_dominant_condition();
    test_five_day_bound_and_semantic_equality();
    test_one_call_seven_days_and_truncation();
    test_one_call_timezone_and_partial_days();
    test_one_call_errors_preserve_base();
    test_errors_preserve_base();
    if (failures != 0) {
        std::cerr << failures << " weather test(s) failed\n";
        return 1;
    }
    std::cout << "weather tests passed\n";
    return 0;
}
