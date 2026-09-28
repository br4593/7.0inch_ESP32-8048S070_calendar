#pragma once

#include "app/calendar_period.hpp"
#include "app/ical_feed.hpp"

#include <array>

namespace calendar {

using EventCountText = std::array<char, 16>;
using DayCountText = std::array<char, 24>;

// Compact, allocation-free labels for the dense 800 x 480 calendar controls.
// An empty label means that no count should be drawn.
EventCountText format_event_count(IcalSummaryCount count, bool include_zero);
DayCountText format_day_count(CivilDate date, IcalSummaryCount count, bool count_known);

}  // namespace calendar
