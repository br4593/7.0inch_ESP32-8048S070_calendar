#include "app/weather_parser.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace calendar::weather {
namespace {

constexpr std::size_t kMaxForecastSamples = 48;
constexpr std::size_t kMaxCompatibilityForecastDays = 5;
constexpr unsigned kMaxJsonDepth = 24;

class JsonCursor {
public:
    explicit JsonCursor(std::string_view input) : input_(input) {}

    void whitespace() {
        while (position_ < input_.size()) {
            const char value = input_[position_];
            if (value != ' ' && value != '\t' && value != '\r' && value != '\n') break;
            ++position_;
        }
    }

    bool consume(char expected) {
        whitespace();
        if (position_ >= input_.size() || input_[position_] != expected) return false;
        ++position_;
        return true;
    }

    bool finished() {
        whitespace();
        return position_ == input_.size();
    }

    bool string(char* output, std::size_t capacity) {
        whitespace();
        if (position_ >= input_.size() || input_[position_++] != '"' || capacity == 0) return false;
        std::size_t written = 0;
        while (position_ < input_.size()) {
            unsigned char value = static_cast<unsigned char>(input_[position_++]);
            if (value == '"') {
                output[written] = '\0';
                return true;
            }
            if (value < 0x20U) return false;
            if (value == '\\') {
                if (position_ >= input_.size()) return false;
                const char escaped = input_[position_++];
                switch (escaped) {
                    case '"': value = '"'; break;
                    case '\\': value = '\\'; break;
                    case '/': value = '/'; break;
                    case 'b': value = '\b'; break;
                    case 'f': value = '\f'; break;
                    case 'n': value = '\n'; break;
                    case 'r': value = '\r'; break;
                    case 't': value = '\t'; break;
                    case 'u': {
                        if (position_ + 4 > input_.size()) return false;
                        for (int index = 0; index < 4; ++index) {
                            const char digit = input_[position_++];
                            if (!((digit >= '0' && digit <= '9') ||
                                  (digit >= 'a' && digit <= 'f') ||
                                  (digit >= 'A' && digit <= 'F'))) return false;
                        }
                        value = '?';
                        break;
                    }
                    default: return false;
                }
            }
            if (written + 1 < capacity) output[written++] = static_cast<char>(value);
        }
        return false;
    }

    bool number(double& output) {
        whitespace();
        const std::size_t start = position_;
        if (position_ < input_.size() && input_[position_] == '-') ++position_;
        if (position_ >= input_.size()) return false;
        if (input_[position_] == '0') {
            ++position_;
        } else {
            if (input_[position_] < '1' || input_[position_] > '9') return false;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
        }
        if (position_ < input_.size() && input_[position_] == '.') {
            ++position_;
            const std::size_t fraction = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
            if (fraction == position_) return false;
        }
        if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) ++position_;
            const std::size_t exponent = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
            if (exponent == position_) return false;
        }
        const std::size_t length = position_ - start;
        if (length == 0 || length >= 64) return false;
        char buffer[64]{};
        std::memcpy(buffer, input_.data() + start, length);
        char* end = nullptr;
        output = std::strtod(buffer, &end);
        return end == buffer + length && std::isfinite(output);
    }

    bool skip_value(unsigned depth = 0) {
        if (depth > kMaxJsonDepth) return false;
        whitespace();
        if (position_ >= input_.size()) return false;
        if (input_[position_] == '"') {
            char ignored[1];
            return string(ignored, sizeof(ignored));
        }
        if (input_[position_] == '{') {
            ++position_;
            whitespace();
            if (position_ < input_.size() && input_[position_] == '}') { ++position_; return true; }
            while (true) {
                char key[1];
                if (!string(key, sizeof(key)) || !consume(':') || !skip_value(depth + 1)) return false;
                whitespace();
                if (position_ < input_.size() && input_[position_] == '}') { ++position_; return true; }
                if (!consume(',')) return false;
            }
        }
        if (input_[position_] == '[') {
            ++position_;
            whitespace();
            if (position_ < input_.size() && input_[position_] == ']') { ++position_; return true; }
            while (true) {
                if (!skip_value(depth + 1)) return false;
                whitespace();
                if (position_ < input_.size() && input_[position_] == ']') { ++position_; return true; }
                if (!consume(',')) return false;
            }
        }
        double ignored = 0;
        if (input_[position_] == '-' || (input_[position_] >= '0' && input_[position_] <= '9')) return number(ignored);
        return literal("true") || literal("false") || literal("null");
    }

private:
    bool literal(std::string_view value) {
        whitespace();
        if (input_.substr(position_, value.size()) != value) return false;
        position_ += value.size();
        return true;
    }

    std::string_view input_;
    std::size_t position_ = 0;
};

template <typename Handler>
bool parse_object(JsonCursor& cursor, Handler&& handler) {
    if (!cursor.consume('{')) return false;
    cursor.whitespace();
    if (cursor.consume('}')) return true;
    while (true) {
        char key[40]{};
        if (!cursor.string(key, sizeof(key)) || !cursor.consume(':') || !handler(key, cursor)) return false;
        cursor.whitespace();
        if (cursor.consume('}')) return true;
        if (!cursor.consume(',')) return false;
    }
}

bool rounded_tenths(double value, std::int16_t& output) {
    const long rounded = std::lround(value * 10.0);
    if (rounded < std::numeric_limits<std::int16_t>::min() ||
        rounded > std::numeric_limits<std::int16_t>::max()) return false;
    output = static_cast<std::int16_t>(rounded);
    return true;
}

bool integer_value(JsonCursor& cursor, std::int64_t& output) {
    double value = 0;
    if (!cursor.number(value) || std::floor(value) != value ||
        value < static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
        value > static_cast<double>(std::numeric_limits<std::int64_t>::max())) return false;
    output = static_cast<std::int64_t>(value);
    return true;
}

struct Condition {
    bool id_present = false;
    std::int16_t id = 0;
    std::array<char, kMaxWeatherDescriptionBytes + 1> description{};
};

bool parse_condition_object(JsonCursor& cursor, Condition& condition) {
    return parse_object(cursor, [&](const char* key, JsonCursor& value) {
        if (std::strcmp(key, "id") == 0) {
            std::int64_t parsed = 0;
            if (!integer_value(value, parsed) || parsed < 0 || parsed > std::numeric_limits<std::int16_t>::max()) return false;
            condition.id = static_cast<std::int16_t>(parsed);
            condition.id_present = true;
            return true;
        }
        if (std::strcmp(key, "description") == 0) {
            return value.string(condition.description.data(), condition.description.size());
        }
        return value.skip_value();
    });
}

bool parse_first_condition(JsonCursor& cursor, Condition& condition) {
    if (!cursor.consume('[')) return false;
    cursor.whitespace();
    if (cursor.consume(']')) return true;
    if (!parse_condition_object(cursor, condition)) return false;
    while (true) {
        cursor.whitespace();
        if (cursor.consume(']')) return true;
        if (!cursor.consume(',') || !cursor.skip_value()) return false;
    }
}

struct ForecastSample {
    std::int64_t utc = 0;
    std::int16_t minimum_tenths_c = 0;
    std::int16_t maximum_tenths_c = 0;
    std::uint8_t precipitation_probability_percent = 0;
    Condition condition{};
    bool utc_present = false;
    bool minimum_present = false;
    bool maximum_present = false;
};

bool parse_one_call_current(JsonCursor& cursor, CurrentConditions& current,
                            bool& missing_required_field) {
    bool temperature_present = false;
    bool feels_like_present = false;
    bool humidity_present = false;
    bool wind_present = false;
    bool observed_present = false;
    Condition condition{};
    const bool parsed = parse_object(cursor, [&](const char* key, JsonCursor& value) {
        double number = 0;
        if (std::strcmp(key, "temp") == 0) {
            if (!value.number(number) || !rounded_tenths(number, current.temperature_tenths_c)) return false;
            temperature_present = true;
            return true;
        }
        if (std::strcmp(key, "feels_like") == 0) {
            if (!value.number(number) || !rounded_tenths(number, current.feels_like_tenths_c)) return false;
            feels_like_present = true;
            return true;
        }
        if (std::strcmp(key, "humidity") == 0) {
            std::int64_t integer = 0;
            if (!integer_value(value, integer) || integer < 0 || integer > 100) return false;
            current.humidity_percent = static_cast<std::uint8_t>(integer);
            humidity_present = true;
            return true;
        }
        if (std::strcmp(key, "wind_speed") == 0) {
            if (!value.number(number) || number < 0 || !rounded_tenths(number, current.wind_tenths_mps)) return false;
            wind_present = true;
            return true;
        }
        if (std::strcmp(key, "weather") == 0) return parse_first_condition(value, condition);
        if (std::strcmp(key, "dt") == 0) {
            observed_present = integer_value(value, current.observed_utc);
            return observed_present;
        }
        return value.skip_value();
    });
    if (!parsed) return false;
    missing_required_field =
        !temperature_present || !feels_like_present || !humidity_present ||
        !wind_present || !observed_present || current.observed_utc <= 0 ||
        !condition.id_present;
    if (missing_required_field) return true;
    current.available = true;
    current.condition_id = condition.id;
    current.description = condition.description;
    return true;
}

bool parse_one_call_temperature(JsonCursor& cursor, ForecastSample& sample) {
    return parse_object(cursor, [&](const char* key, JsonCursor& value) {
        double parsed = 0;
        if (std::strcmp(key, "min") == 0) {
            if (!value.number(parsed) || !rounded_tenths(parsed, sample.minimum_tenths_c)) return false;
            sample.minimum_present = true;
            return true;
        }
        if (std::strcmp(key, "max") == 0) {
            if (!value.number(parsed) || !rounded_tenths(parsed, sample.maximum_tenths_c)) return false;
            sample.maximum_present = true;
            return true;
        }
        return value.skip_value();
    });
}

bool parse_one_call_day(JsonCursor& cursor, ForecastSample& sample) {
    return parse_object(cursor, [&](const char* key, JsonCursor& value) {
        if (std::strcmp(key, "dt") == 0) {
            sample.utc_present = integer_value(value, sample.utc);
            return sample.utc_present;
        }
        if (std::strcmp(key, "temp") == 0) return parse_one_call_temperature(value, sample);
        if (std::strcmp(key, "weather") == 0) return parse_first_condition(value, sample.condition);
        if (std::strcmp(key, "pop") == 0) {
            double parsed = 0;
            if (!value.number(parsed)) return false;
            parsed = std::max(0.0, std::min(1.0, parsed));
            sample.precipitation_probability_percent =
                static_cast<std::uint8_t>(std::lround(parsed * 100.0));
            return true;
        }
        return value.skip_value();
    });
}

bool parse_forecast_main(JsonCursor& cursor, ForecastSample& sample) {
    return parse_object(cursor, [&](const char* key, JsonCursor& value) {
        double parsed = 0;
        if (std::strcmp(key, "temp_min") == 0) {
            if (!value.number(parsed) || !rounded_tenths(parsed, sample.minimum_tenths_c)) return false;
            sample.minimum_present = true;
            return true;
        }
        if (std::strcmp(key, "temp_max") == 0) {
            if (!value.number(parsed) || !rounded_tenths(parsed, sample.maximum_tenths_c)) return false;
            sample.maximum_present = true;
            return true;
        }
        return value.skip_value();
    });
}

bool parse_forecast_sample(JsonCursor& cursor, ForecastSample& sample) {
    return parse_object(cursor, [&](const char* key, JsonCursor& value) {
        if (std::strcmp(key, "dt") == 0) {
            sample.utc_present = integer_value(value, sample.utc);
            return sample.utc_present;
        }
        if (std::strcmp(key, "main") == 0) return parse_forecast_main(value, sample);
        if (std::strcmp(key, "weather") == 0) return parse_first_condition(value, sample.condition);
        if (std::strcmp(key, "pop") == 0) {
            double parsed = 0;
            if (!value.number(parsed)) return false;
            parsed = std::max(0.0, std::min(1.0, parsed));
            sample.precipitation_probability_percent = static_cast<std::uint8_t>(std::lround(parsed * 100.0));
            return true;
        }
        return value.skip_value();
    });
}

std::int64_t floor_divide(std::int64_t numerator, std::int64_t denominator) {
    std::int64_t quotient = numerator / denominator;
    const std::int64_t remainder = numerator % denominator;
    if (remainder != 0 && numerator < 0) --quotient;
    return quotient;
}

LocalDate civil_from_days(std::int64_t days_since_epoch) {
    // Howard Hinnant's civil-from-days algorithm, with 1970-01-01 as day zero.
    const std::int64_t z = days_since_epoch + 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    std::int64_t year = static_cast<std::int64_t>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned day = doy - (153 * mp + 2) / 5 + 1;
    const int month = static_cast<int>(mp) + (mp < 10 ? 3 : -9);
    year += month <= 2;
    return {static_cast<std::int16_t>(year), static_cast<std::uint8_t>(month),
            static_cast<std::uint8_t>(day)};
}

void copy_description(std::array<char, kMaxWeatherDescriptionBytes + 1>& destination,
                      const std::array<char, kMaxWeatherDescriptionBytes + 1>& source) {
    destination = source;
}

WeatherParseResult failure(WeatherParseError error, const WeatherSnapshot& base) {
    return {error, base};
}

}  // namespace

bool operator==(const LocalDate& lhs, const LocalDate& rhs) {
    return lhs.year == rhs.year && lhs.month == rhs.month && lhs.day == rhs.day;
}

bool operator==(const CurrentConditions& lhs, const CurrentConditions& rhs) {
    return lhs.available == rhs.available &&
           lhs.temperature_tenths_c == rhs.temperature_tenths_c &&
           lhs.feels_like_tenths_c == rhs.feels_like_tenths_c &&
           lhs.humidity_percent == rhs.humidity_percent &&
           lhs.wind_tenths_mps == rhs.wind_tenths_mps &&
           lhs.condition_id == rhs.condition_id && lhs.description == rhs.description &&
           lhs.observed_utc == rhs.observed_utc;
}

bool operator==(const ForecastDay& lhs, const ForecastDay& rhs) {
    return lhs.date == rhs.date && lhs.minimum_tenths_c == rhs.minimum_tenths_c &&
           lhs.maximum_tenths_c == rhs.maximum_tenths_c &&
           lhs.maximum_precipitation_probability_percent ==
               rhs.maximum_precipitation_probability_percent &&
           lhs.dominant_condition_id == rhs.dominant_condition_id &&
           lhs.description == rhs.description;
}

bool operator==(const WeatherSnapshot& lhs, const WeatherSnapshot& rhs) {
    if (!(lhs.current == rhs.current) || lhs.forecast_count != rhs.forecast_count ||
        lhs.timezone_offset_seconds != rhs.timezone_offset_seconds) return false;
    for (std::size_t index = 0; index < lhs.forecast_count; ++index) {
        if (!(lhs.forecast[index] == rhs.forecast[index])) return false;
    }
    return true;
}

WeatherParseResult parse_current_weather_json(std::string_view json,
                                              const WeatherSnapshot& base) {
    if (json.size() > kMaxWeatherJsonBytes) return failure(WeatherParseError::InputTooLarge, base);
    JsonCursor cursor(json);
    CurrentConditions current{};
    bool temperature_present = false;
    bool feels_like_present = false;
    bool humidity_present = false;
    bool wind_present = false;
    Condition condition{};

    const bool parsed = parse_object(cursor, [&](const char* key, JsonCursor& value) {
        if (std::strcmp(key, "main") == 0) {
            return parse_object(value, [&](const char* main_key, JsonCursor& field) {
                double number = 0;
                if (std::strcmp(main_key, "temp") == 0) {
                    if (!field.number(number) || !rounded_tenths(number, current.temperature_tenths_c)) return false;
                    temperature_present = true;
                    return true;
                }
                if (std::strcmp(main_key, "feels_like") == 0) {
                    if (!field.number(number) || !rounded_tenths(number, current.feels_like_tenths_c)) return false;
                    feels_like_present = true;
                    return true;
                }
                if (std::strcmp(main_key, "humidity") == 0) {
                    std::int64_t integer = 0;
                    if (!integer_value(field, integer) || integer < 0 || integer > 100) return false;
                    current.humidity_percent = static_cast<std::uint8_t>(integer);
                    humidity_present = true;
                    return true;
                }
                return field.skip_value();
            });
        }
        if (std::strcmp(key, "wind") == 0) {
            return parse_object(value, [&](const char* wind_key, JsonCursor& field) {
                if (std::strcmp(wind_key, "speed") != 0) return field.skip_value();
                double speed = 0;
                if (!field.number(speed) || speed < 0 || !rounded_tenths(speed, current.wind_tenths_mps)) return false;
                wind_present = true;
                return true;
            });
        }
        if (std::strcmp(key, "weather") == 0) return parse_first_condition(value, condition);
        if (std::strcmp(key, "dt") == 0) return integer_value(value, current.observed_utc);
        return value.skip_value();
    });

    if (!parsed || !cursor.finished()) return failure(WeatherParseError::MalformedJson, base);
    if (!temperature_present || !feels_like_present || !humidity_present || !wind_present ||
        !condition.id_present || current.observed_utc <= 0) {
        return failure(WeatherParseError::MissingRequiredField, base);
    }
    current.available = true;
    current.condition_id = condition.id;
    copy_description(current.description, condition.description);
    WeatherSnapshot result = base;
    result.current = current;
    return {WeatherParseError::None, result};
}

WeatherParseResult parse_forecast_weather_json(std::string_view json,
                                               const WeatherSnapshot& base) {
    if (json.size() > kMaxWeatherJsonBytes) return failure(WeatherParseError::InputTooLarge, base);
    JsonCursor cursor(json);
    std::array<ForecastSample, kMaxForecastSamples> samples{};
    std::size_t sample_count = 0;
    bool list_present = false;
    bool timezone_present = false;
    std::int32_t timezone_offset = 0;
    bool too_many_samples = false;

    const bool parsed = parse_object(cursor, [&](const char* key, JsonCursor& value) {
        if (std::strcmp(key, "list") == 0) {
            list_present = true;
            if (!value.consume('[')) return false;
            value.whitespace();
            if (value.consume(']')) return true;
            while (true) {
                if (sample_count >= samples.size()) {
                    too_many_samples = true;
                    if (!value.skip_value()) return false;
                } else if (!parse_forecast_sample(value, samples[sample_count++])) {
                    return false;
                }
                value.whitespace();
                if (value.consume(']')) return true;
                if (!value.consume(',')) return false;
            }
        }
        if (std::strcmp(key, "city") == 0) {
            return parse_object(value, [&](const char* city_key, JsonCursor& field) {
                if (std::strcmp(city_key, "timezone") != 0) return field.skip_value();
                std::int64_t offset = 0;
                if (!integer_value(field, offset) || offset < -86400 || offset > 86400) return false;
                timezone_offset = static_cast<std::int32_t>(offset);
                timezone_present = true;
                return true;
            });
        }
        return value.skip_value();
    });

    if (!parsed || !cursor.finished()) return failure(WeatherParseError::MalformedJson, base);
    if (too_many_samples) return failure(WeatherParseError::TooManyForecastEntries, base);
    if (!list_present || !timezone_present || sample_count == 0) {
        return failure(WeatherParseError::MissingRequiredField, base);
    }
    for (std::size_t index = 0; index < sample_count; ++index) {
        const ForecastSample& sample = samples[index];
        if (!sample.utc_present || !sample.minimum_present || !sample.maximum_present ||
            !sample.condition.id_present) {
            return failure(WeatherParseError::MissingRequiredField, base);
        }
    }

    std::sort(samples.begin(), samples.begin() + static_cast<std::ptrdiff_t>(sample_count),
              [](const ForecastSample& lhs, const ForecastSample& rhs) { return lhs.utc < rhs.utc; });

    WeatherSnapshot result = base;
    result.timezone_offset_seconds = timezone_offset;
    result.forecast = {};
    result.forecast_count = 0;
    std::size_t index = 0;
    while (index < sample_count &&
           result.forecast_count < kMaxCompatibilityForecastDays) {
        const std::int64_t local_day = floor_divide(samples[index].utc + timezone_offset, 86400);
        ForecastDay& day = result.forecast[result.forecast_count];
        day.date = civil_from_days(local_day);
        day.minimum_tenths_c = samples[index].minimum_tenths_c;
        day.maximum_tenths_c = samples[index].maximum_tenths_c;
        day.maximum_precipitation_probability_percent = samples[index].precipitation_probability_percent;

        std::array<std::int16_t, kMaxForecastSamples> condition_ids{};
        std::array<std::uint8_t, kMaxForecastSamples> condition_counts{};
        std::array<std::array<char, kMaxWeatherDescriptionBytes + 1>, kMaxForecastSamples> descriptions{};
        std::size_t unique_conditions = 0;
        while (index < sample_count &&
               floor_divide(samples[index].utc + timezone_offset, 86400) == local_day) {
            day.minimum_tenths_c = std::min(day.minimum_tenths_c, samples[index].minimum_tenths_c);
            day.maximum_tenths_c = std::max(day.maximum_tenths_c, samples[index].maximum_tenths_c);
            day.maximum_precipitation_probability_percent = std::max(
                day.maximum_precipitation_probability_percent,
                samples[index].precipitation_probability_percent);
            std::size_t condition_index = 0;
            while (condition_index < unique_conditions &&
                   condition_ids[condition_index] != samples[index].condition.id) ++condition_index;
            if (condition_index == unique_conditions) {
                condition_ids[condition_index] = samples[index].condition.id;
                descriptions[condition_index] = samples[index].condition.description;
                ++unique_conditions;
            }
            if (condition_counts[condition_index] < std::numeric_limits<std::uint8_t>::max()) {
                ++condition_counts[condition_index];
            }
            ++index;
        }
        std::size_t dominant = 0;
        for (std::size_t condition_index = 1; condition_index < unique_conditions; ++condition_index) {
            if (condition_counts[condition_index] > condition_counts[dominant]) dominant = condition_index;
        }
        day.dominant_condition_id = condition_ids[dominant];
        day.description = descriptions[dominant];
        ++result.forecast_count;
    }
    return {WeatherParseError::None, result};
}

WeatherParseResult parse_one_call_weather_json(std::string_view json,
                                               const WeatherSnapshot& base) {
    if (json.size() > kMaxWeatherJsonBytes) {
        return failure(WeatherParseError::InputTooLarge, base);
    }
    JsonCursor cursor(json);
    CurrentConditions current{};
    std::array<ForecastSample, kMaxForecastDays> days{};
    std::size_t day_count = 0;
    bool current_present = false;
    bool missing_current_field = false;
    bool daily_present = false;
    bool timezone_present = false;
    bool missing_daily_field = false;
    std::int32_t timezone_offset = 0;

    const bool parsed = parse_object(cursor, [&](const char* key, JsonCursor& value) {
        if (std::strcmp(key, "timezone_offset") == 0) {
            std::int64_t offset = 0;
            if (!integer_value(value, offset) || offset < -86400 || offset > 86400) return false;
            timezone_offset = static_cast<std::int32_t>(offset);
            timezone_present = true;
            return true;
        }
        if (std::strcmp(key, "current") == 0) {
            current_present = true;
            return parse_one_call_current(value, current, missing_current_field);
        }
        if (std::strcmp(key, "daily") == 0) {
            daily_present = true;
            if (!value.consume('[')) return false;
            value.whitespace();
            if (value.consume(']')) return true;
            while (true) {
                if (day_count < days.size()) {
                    ForecastSample sample{};
                    if (!parse_one_call_day(value, sample)) return false;
                    if (!sample.utc_present || !sample.minimum_present ||
                        !sample.maximum_present || !sample.condition.id_present) {
                        missing_daily_field = true;
                    }
                    days[day_count++] = sample;
                } else if (!value.skip_value()) {
                    return false;
                }
                value.whitespace();
                if (value.consume(']')) return true;
                if (!value.consume(',')) return false;
            }
        }
        return value.skip_value();
    });

    if (!parsed || !cursor.finished()) return failure(WeatherParseError::MalformedJson, base);
    if (!current_present || !daily_present || !timezone_present || day_count == 0 ||
        missing_current_field || missing_daily_field) {
        return failure(WeatherParseError::MissingRequiredField, base);
    }
    std::sort(days.begin(), days.begin() + static_cast<std::ptrdiff_t>(day_count),
              [](const ForecastSample& lhs, const ForecastSample& rhs) {
                  return lhs.utc < rhs.utc;
              });

    WeatherSnapshot result = base;
    result.current = current;
    result.forecast = {};
    result.forecast_count = static_cast<std::uint8_t>(day_count);
    result.timezone_offset_seconds = timezone_offset;
    for (std::size_t index = 0; index < day_count; ++index) {
        ForecastDay& destination = result.forecast[index];
        const ForecastSample& source = days[index];
        destination.date = civil_from_days(
            floor_divide(source.utc + timezone_offset, 86400));
        destination.minimum_tenths_c = source.minimum_tenths_c;
        destination.maximum_tenths_c = source.maximum_tenths_c;
        destination.maximum_precipitation_probability_percent =
            source.precipitation_probability_percent;
        destination.dominant_condition_id = source.condition.id;
        destination.description = source.condition.description;
    }
    return {WeatherParseError::None, result};
}

}  // namespace calendar::weather
