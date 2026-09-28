#include "app/calendar_provider.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace calendar {
namespace {

CalendarIdentity work_calendar() { return {"work", "Work", 0x2D7FF9}; }
CalendarIdentity family_calendar() { return {"family", "Family", 0x35A854}; }

CalendarEvent timed(std::string id, std::string title, std::int64_t start, std::int64_t end,
                    std::optional<std::string> location = std::nullopt,
                    CalendarIdentity calendar = work_calendar()) {
    CalendarEvent event;
    event.id.assign(id.data(), id.size());
    event.title.assign(title.data(), title.size());
    event.time_kind = TimeKind::Timed;
    event.start_utc = start;
    event.end_utc = end;
    if (location) event.location.emplace(location->data(), location->size());
    event.calendar = std::move(calendar);
    return event;
}

CalendarEvent all_day(std::string id, std::string title, CivilDate start, CivilDate end,
                      CalendarIdentity calendar = family_calendar()) {
    CalendarEvent event;
    event.id.assign(id.data(), id.size());
    event.title.assign(title.data(), title.size());
    event.time_kind = TimeKind::AllDay;
    event.start_date = start; event.end_date_exclusive = end; event.calendar = std::move(calendar);
    return event;
}

}  // namespace

CalendarSnapshot::CalendarSnapshot(CalendarEventList events)
    : events_(std::move(events)) {
    if (events_.size() > kMaxSnapshotEvents) {
        throw std::invalid_argument("calendar snapshot exceeds 256 events");
    }
    std::unordered_set<std::string> ids;
    for (const CalendarEvent& event : events_) {
        if (!is_valid_event(event) || !ids.emplace(event.id.data(), event.id.size()).second) {
            throw std::invalid_argument("calendar snapshot has an invalid or duplicate event");
        }
    }
}

const CalendarEventList& CalendarSnapshot::events() const { return events_; }

const CalendarEvent* CalendarSnapshot::find_by_id(std::string_view id) const {
    const auto found = std::find_if(events_.begin(), events_.end(), [id](const CalendarEvent& event) {
        return std::string_view(event.id.data(), event.id.size()) == id;
    });
    return found == events_.end() ? nullptr : &*found;
}

namespace {

std::shared_ptr<const CalendarSnapshot> make_snapshot(CalendarEventList events) {
    return std::allocate_shared<CalendarSnapshot>(CalendarPsramAllocator<CalendarSnapshot>{},
                                                   std::move(events));
}

bool same_event(const CalendarEvent& left, const CalendarEvent& right) {
    return left.id == right.id && left.title == right.title && left.time_kind == right.time_kind &&
           left.start_utc == right.start_utc && left.end_utc == right.end_utc &&
           left.start_date == right.start_date && left.end_date_exclusive == right.end_date_exclusive &&
           left.location == right.location && left.calendar.id == right.calendar.id &&
           left.calendar.display_name == right.calendar.display_name &&
           left.calendar.color_rgb888 == right.calendar.color_rgb888;
}

}  // namespace

MockCalendarProvider::MockCalendarProvider() : MockCalendarProvider(make_milestone_one_fixtures()) {}

MockCalendarProvider::MockCalendarProvider(CalendarEventList events, ProviderState state)
    : state_(state), snapshot_(make_snapshot(std::move(events))) {}

ProviderResult MockCalendarProvider::current() const { return {state_, snapshot_}; }

void MockCalendarProvider::replace(CalendarEventList events, ProviderState state) {
    snapshot_ = make_snapshot(std::move(events));
    state_ = state;
}

void MockCalendarProvider::set_state(ProviderState state) { state_ = state; }

CalendarEventList filter_and_sort_agenda(const CalendarSnapshot& snapshot, const DayWindow& day) {
    CalendarEventList matching;
    for (const CalendarEvent& event : snapshot.events()) {
        if (overlaps_day(event, day)) matching.push_back(event);
    }
    std::sort(matching.begin(), matching.end(), [](const CalendarEvent& left, const CalendarEvent& right) {
        if (left.time_kind != right.time_kind) return left.time_kind == TimeKind::AllDay;
        if (left.time_kind == TimeKind::AllDay) {
            if (left.start_date != right.start_date) return left.start_date < right.start_date;
        } else {
            if (left.start_utc != right.start_utc) return left.start_utc < right.start_utc;
            if (left.end_utc != right.end_utc) return left.end_utc < right.end_utc;
        }
        return left.id < right.id;
    });
    return matching;
}

bool snapshot_matches_events(const CalendarSnapshot& snapshot,
                             const CalendarEventList& events) {
    if (snapshot.events().size() != events.size()) return false;
    for (const CalendarEvent& event : events) {
        const CalendarEvent* current = snapshot.find_by_id(
            std::string_view(event.id.data(), event.id.size()));
        if (current == nullptr || !same_event(*current, event)) return false;
    }
    return true;
}

CalendarEventList make_milestone_one_fixtures() {
    // UTC intervals are intentionally simple test data; conversion to Jerusalem
    // civil windows remains the responsibility of a future production adapter.
    CalendarEventList events{
        all_day("family-trip", "טיול משפחתי / Family trip", {2028, 2, 28}, {2028, 3, 2}),
        all_day("leap-day", "Leap day", {2028, 2, 29}, {2028, 3, 1}),
        all_day("long-title", "A deliberately long fixture title for validating truncation in the agenda and full rendering in details", {2028, 3, 4}, {2028, 3, 5}),
        timed("overnight", "Overnight maintenance", 1835393400, 1835400600, "Server room"),
        timed("equal-a", "09:00 English", 1835514000, 1835517600, "Hangar", work_calendar()),
        timed("equal-b", "09:00 פגישה 42 / meeting", 1835514000, 1835519400, std::nullopt, family_calendar()),
        timed("untitled", "", 1835601600, 1835605200),
        timed("year-boundary", "New year handover", 1767222000, 1767229200),
    };
    // A crowded day has more rows than the UI capacity but stays under snapshot capacity.
    for (int index = 0; index < 21; ++index) {
        events.push_back(timed("crowded-" + std::to_string(index), "Crowded agenda " + std::to_string(index),
                               1835683200 + index * 900, 1835683800 + index * 900));
    }
    return events;
}

}  // namespace calendar
