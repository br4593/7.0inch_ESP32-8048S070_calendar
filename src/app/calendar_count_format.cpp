#include "app/calendar_count_format.hpp"

#include <cstdio>

namespace calendar {

EventCountText format_event_count(IcalSummaryCount count, bool include_zero) {
    EventCountText text{};
    if (count.overflow || count.event_count > 99) {
        std::snprintf(text.data(), text.size(), "99+ events");
    } else if (count.event_count == 0) {
        if (include_zero) std::snprintf(text.data(), text.size(), "0 events");
    } else if (count.event_count == 1) {
        std::snprintf(text.data(), text.size(), "1 event");
    } else {
        std::snprintf(text.data(), text.size(), "%u events",
                      static_cast<unsigned>(count.event_count));
    }
    return text;
}

DayCountText format_day_count(CivilDate date, IcalSummaryCount count, bool count_known) {
    DayCountText text{};
    if (!count_known) {
        std::snprintf(text.data(), text.size(), "%02u/%02u", date.day, date.month);
    } else if (count.overflow || count.event_count > 99) {
        std::snprintf(text.data(), text.size(), "%02u/%02u (99+)", date.day, date.month);
    } else {
        std::snprintf(text.data(), text.size(), "%02u/%02u (%u)", date.day, date.month,
                      static_cast<unsigned>(count.event_count));
    }
    return text;
}

}  // namespace calendar
