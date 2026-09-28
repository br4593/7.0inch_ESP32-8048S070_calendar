#include "app/calendar_period.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace calendar {
namespace {

// Returns days relative to 1970-01-01. This is the same proleptic Gregorian
// calculation used by calendar_types.cpp, kept private to both translation
// units so the public date type does not expose an epoch representation.
std::int64_t days_from_civil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
    const unsigned month_prime = month + (month > 2 ? static_cast<unsigned>(-3) : 9);
    const unsigned day_of_year = (153 * month_prime + 2) / 5 + day - 1;
    const unsigned day_of_era =
        year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    return static_cast<std::int64_t>(era) * 146097 + day_of_era - 719468;
}

void require_valid(CivilDate date) {
    if (!is_valid_date(date)) throw std::invalid_argument("calendar period needs a valid date");
}

}  // namespace

bool operator==(CalendarPeriod left, CalendarPeriod right) {
    return left.start == right.start && left.end_exclusive == right.end_exclusive;
}

bool operator!=(CalendarPeriod left, CalendarPeriod right) { return !(left == right); }

bool contains(CalendarPeriod period, CivilDate date) {
    return period.start <= date && date < period.end_exclusive;
}

unsigned days_in_month(int year, unsigned month) {
    if (month < 1 || month > 12) throw std::invalid_argument("month must be in 1..12");
    const CivilDate first{year, month, 1};
    const CivilDate next = month == 12 ? CivilDate{year + 1, 1, 1}
                                       : CivilDate{year, month + 1, 1};
    return static_cast<unsigned>(days_from_civil(next.year, next.month, next.day) -
                                 days_from_civil(first.year, first.month, first.day));
}

CivilDate week_start_sunday(CivilDate date) {
    require_valid(date);
    // 1970-01-01 was Thursday. Normalize the remainder for dates before 1970.
    const std::int64_t days = days_from_civil(date.year, date.month, date.day);
    const int sunday_based_weekday = static_cast<int>(((days + 4) % 7 + 7) % 7);
    return add_days(date, -sunday_based_weekday);
}

CalendarPeriod week_period(CivilDate date) {
    const CivilDate start = week_start_sunday(date);
    return {start, add_days(start, 7)};
}

CalendarPeriod month_grid_period(CivilDate date_in_month) {
    require_valid(date_in_month);
    const CivilDate start = week_start_sunday({date_in_month.year, date_in_month.month, 1});
    return {start, add_days(start, 42)};
}

CivilDate add_months_clamped(CivilDate date, int months) {
    require_valid(date);
    const std::int64_t source_month = static_cast<std::int64_t>(date.year) * 12 +
                                      static_cast<std::int64_t>(date.month) - 1;
    const std::int64_t destination = source_month + months;
    std::int64_t year = destination / 12;
    std::int64_t month_zero_based = destination % 12;
    if (month_zero_based < 0) {
        month_zero_based += 12;
        --year;
    }
    if (year < std::numeric_limits<int>::min() || year > std::numeric_limits<int>::max()) {
        throw std::out_of_range("calendar month navigation exceeds supported years");
    }
    const unsigned month = static_cast<unsigned>(month_zero_based + 1);
    const unsigned day = date.day < days_in_month(static_cast<int>(year), month)
                             ? date.day
                             : days_in_month(static_cast<int>(year), month);
    return {static_cast<int>(year), month, day};
}

}  // namespace calendar
