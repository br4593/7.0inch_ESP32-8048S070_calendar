#include "app/calendar_types.hpp"

#include <tuple>

namespace calendar {
namespace {

// Howard Hinnant's civil-date algorithms, expressed locally to avoid a C++20
// chrono dependency on the embedded build.
std::int64_t days_from_civil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
    const unsigned month_prime = month + (month > 2 ? static_cast<unsigned>(-3) : 9);
    const unsigned day_of_year = (153 * month_prime + 2) / 5 + day - 1;
    const unsigned day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    return era * 146097 + static_cast<int>(day_of_era) - 719468;
}

CivilDate civil_from_days(std::int64_t days) {
    days += 719468;
    const int era = (days >= 0 ? days : days - 146096) / 146097;
    const unsigned day_of_era = static_cast<unsigned>(days - era * 146097);
    const unsigned year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
    int year = static_cast<int>(year_of_era) + era * 400;
    const unsigned day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    const unsigned month_prime = (5 * day_of_year + 2) / 153;
    const unsigned day = day_of_year - (153 * month_prime + 2) / 5 + 1;
    const unsigned month = month_prime + (month_prime < 10 ? 3 : static_cast<unsigned>(-9));
    year += month <= 2;
    return {year, month, day};
}

bool bytes_at_most(std::string_view value, std::size_t maximum) {
    return value.size() <= maximum;
}

}  // namespace

bool operator==(CivilDate left, CivilDate right) { return std::tie(left.year, left.month, left.day) == std::tie(right.year, right.month, right.day); }
bool operator!=(CivilDate left, CivilDate right) { return !(left == right); }
bool operator<(CivilDate left, CivilDate right) { return std::tie(left.year, left.month, left.day) < std::tie(right.year, right.month, right.day); }
bool operator<=(CivilDate left, CivilDate right) { return !(right < left); }
bool operator>(CivilDate left, CivilDate right) { return right < left; }
bool operator>=(CivilDate left, CivilDate right) { return !(left < right); }

bool is_valid_date(CivilDate date) {
    if (date.month < 1 || date.month > 12 || date.day < 1) {
        return false;
    }
    const auto next_month = date.month == 12 ? CivilDate{date.year + 1, 1, 1}
                                             : CivilDate{date.year, date.month + 1, 1};
    return date < next_month && date.day <= static_cast<unsigned>(days_from_civil(next_month.year, next_month.month, next_month.day) - days_from_civil(date.year, date.month, 1));
}

CivilDate add_days(CivilDate date, int days) {
    return civil_from_days(days_from_civil(date.year, date.month, date.day) + days);
}

bool is_valid_event(const CalendarEvent& event) {
    if (event.id.empty() || !bytes_at_most(event.id, kMaxEventIdBytes) ||
        !bytes_at_most(event.title, kMaxTitleBytes) ||
        event.calendar.id.empty() || !bytes_at_most(event.calendar.id, kMaxCalendarIdBytes) ||
        !bytes_at_most(event.calendar.display_name, kMaxCalendarNameBytes)) {
        return false;
    }
    if (event.location && !bytes_at_most(*event.location, kMaxLocationBytes)) {
        return false;
    }
    if (event.time_kind == TimeKind::Timed) {
        return event.end_utc > event.start_utc;
    }
    return is_valid_date(event.start_date) && is_valid_date(event.end_date_exclusive) &&
           event.end_date_exclusive > event.start_date;
}

bool is_valid_day_window(const DayWindow& window) {
    return is_valid_date(window.date) && window.end_utc > window.start_utc;
}

bool overlaps_day(const CalendarEvent& event, const DayWindow& window) {
    if (!is_valid_event(event) || !is_valid_day_window(window)) {
        return false;
    }
    if (event.time_kind == TimeKind::Timed) {
        return event.start_utc < window.end_utc && event.end_utc > window.start_utc;
    }
    const CivilDate next_day = add_days(window.date, 1);
    return event.start_date < next_day && event.end_date_exclusive > window.date;
}

std::string display_title(const CalendarEvent& event) {
    return event.title.empty() ? "(Untitled event)"
                               : std::string(event.title.data(), event.title.size());
}

}  // namespace calendar
