#pragma once

#include "app/calendar_types.hpp"

namespace calendar {

enum class CalendarViewMode { Week, Month };

// A date-only, end-exclusive interval. Month view deliberately uses a fixed
// six-week range so the UI can reuse exactly 42 cells without changing shape.
struct CalendarPeriod {
    CivilDate start;
    CivilDate end_exclusive;
};

bool operator==(CalendarPeriod left, CalendarPeriod right);
bool operator!=(CalendarPeriod left, CalendarPeriod right);
bool contains(CalendarPeriod period, CivilDate date);

unsigned days_in_month(int year, unsigned month);
CivilDate week_start_sunday(CivilDate date);
CalendarPeriod week_period(CivilDate date);
CalendarPeriod month_grid_period(CivilDate date_in_month);

// Moves by whole calendar months and clamps the day to the destination month.
// For example, 31 January + one month becomes 28 or 29 February.
CivilDate add_months_clamped(CivilDate date, int months);

}  // namespace calendar
