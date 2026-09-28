#include "app/calendar_navigation.hpp"
#include "app/calendar_count_format.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using calendar::CalendarEvent;
using calendar::CalendarIdentity;
using calendar::CalendarSnapshot;
using calendar::CalendarViewMode;
using calendar::CivilDate;
using calendar::DayWindow;
using calendar::FixedClock;
using calendar::NavigationScreen;
using calendar::NavigationState;
using calendar::ProviderState;
using calendar::TimeKind;

int failures = 0;
#define CHECK(...) do { if (!(__VA_ARGS__)) { ++failures; std::cerr << __FUNCTION__ << ": " #__VA_ARGS__ " failed at " << __LINE__ << '\n'; } } while (false)

CalendarEvent timed(std::string id, std::int64_t start, std::int64_t end) {
    CalendarEvent event;
    event.id = std::move(id); event.title = event.id; event.time_kind = TimeKind::Timed;
    event.start_utc = start; event.end_utc = end; event.calendar = {"c", "Calendar", 0};
    return event;
}

CalendarEvent all_day(std::string id, CivilDate start, CivilDate end) {
    CalendarEvent event;
    event.id = std::move(id); event.title = event.id; event.time_kind = TimeKind::AllDay;
    event.start_date = start; event.end_date_exclusive = end; event.calendar = {"c", "Calendar", 0};
    return event;
}

void test_dates_and_day_window() {
    CHECK(calendar::is_valid_date({2028, 2, 29}));
    CHECK(!calendar::is_valid_date({2027, 2, 29}));
    CHECK(calendar::add_days({2027, 12, 31}, 1) == CivilDate{2028, 1, 1});
    CHECK(calendar::add_days({2028, 2, 28}, 1) == CivilDate{2028, 2, 29});
    CHECK(calendar::add_days({2028, 2, 29}, 1) == CivilDate{2028, 3, 1});
    CHECK(calendar::is_valid_day_window({{2028, 3, 1}, 100, 100 + 23 * 3600}));
}

void test_week_and_month_periods() {
    CHECK(calendar::week_start_sunday({2028, 2, 29}) == CivilDate{2028, 2, 27});
    CHECK(calendar::week_period({2027, 12, 31}) ==
          calendar::CalendarPeriod{{2027, 12, 26}, {2028, 1, 2}});
    CHECK(calendar::week_period({2028, 1, 2}) ==
          calendar::CalendarPeriod{{2028, 1, 2}, {2028, 1, 9}});

    CHECK(calendar::days_in_month(2027, 2) == 28);
    CHECK(calendar::days_in_month(2028, 2) == 29);
    CHECK(calendar::days_in_month(2028, 12) == 31);
    CHECK(calendar::month_grid_period({2028, 2, 29}) ==
          calendar::CalendarPeriod{{2028, 1, 30}, {2028, 3, 12}});
    CHECK(calendar::contains(calendar::month_grid_period({2028, 2, 1}), {2028, 2, 29}));
    CHECK(!calendar::contains(calendar::month_grid_period({2028, 2, 1}), {2028, 3, 12}));

    CHECK(calendar::add_months_clamped({2028, 1, 31}, 1) == CivilDate{2028, 2, 29});
    CHECK(calendar::add_months_clamped({2027, 1, 31}, 1) == CivilDate{2027, 2, 28});
    CHECK(calendar::add_months_clamped({2028, 3, 31}, -1) == CivilDate{2028, 2, 29});
    CHECK(calendar::add_months_clamped({2028, 12, 15}, 1) == CivilDate{2029, 1, 15});
}

void test_compact_event_count_labels() {
    CHECK(std::string(calendar::format_event_count({0, false}, false).data()).empty());
    CHECK(std::string(calendar::format_event_count({0, false}, true).data()) == "0 events");
    CHECK(std::string(calendar::format_event_count({1, false}, false).data()) == "1 event");
    CHECK(std::string(calendar::format_event_count({17, false}, false).data()) == "17 events");
    CHECK(std::string(calendar::format_event_count({99, false}, false).data()) == "99 events");
    CHECK(std::string(calendar::format_event_count({100, false}, false).data()) == "99+ events");
    CHECK(std::string(calendar::format_event_count({65535, true}, false).data()) == "99+ events");

    const CivilDate date{2026, 9, 5};
    CHECK(std::string(calendar::format_day_count(date, {0, false}, false).data()) == "05/09");
    CHECK(std::string(calendar::format_day_count(date, {0, false}, true).data()) == "05/09 (0)");
    CHECK(std::string(calendar::format_day_count(date, {7, false}, true).data()) == "05/09 (7)");
    CHECK(std::string(calendar::format_day_count(date, {99, false}, true).data()) == "05/09 (99)");
    CHECK(std::string(calendar::format_day_count(date, {100, false}, true).data()) == "05/09 (99+)");
    CHECK(std::string(calendar::format_day_count(date, {65535, true}, true).data()) == "05/09 (99+)");
}

void test_end_exclusive_overlap_and_sorting() {
    const DayWindow day{{2028, 3, 1}, 1000, 2000};
    CalendarSnapshot snapshot({
        timed("after", 2000, 2100), timed("end-at-start", 900, 1000),
        timed("overnight", 900, 1100), timed("same-b", 1200, 1300),
        timed("same-a", 1200, 1300), all_day("multi", {2028, 2, 29}, {2028, 3, 2}),
        all_day("ends-today", {2028, 2, 28}, {2028, 3, 1}),
    });
    const auto agenda = calendar::filter_and_sort_agenda(snapshot, day);
    CHECK(agenda.size() == 4);
    CHECK(agenda[0].id == "multi");
    CHECK(agenda[1].id == "overnight");
    CHECK(agenda[2].id == "same-a");
    CHECK(agenda[3].id == "same-b");
    CHECK(calendar::filter_and_sort_agenda(snapshot, {{2028, 3, 3}, 3000, 4000}).empty());
}

void test_snapshot_bounds_and_fixture_coverage() {
    const auto fixtures = calendar::make_milestone_one_fixtures();
    CHECK(fixtures.size() <= calendar::kMaxSnapshotEvents);
    CalendarSnapshot valid(fixtures);
    CHECK(valid.find_by_id("equal-b") != nullptr);
    CHECK(valid.find_by_id("leap-day") != nullptr);
    const CalendarEvent* long_title = valid.find_by_id("long-title");
    CHECK(long_title != nullptr);
    CHECK(long_title->title.size() <= calendar::kMaxTitleBytes);
    const CalendarEvent* missing_location = valid.find_by_id("equal-b");
    CHECK(missing_location != nullptr);
    CHECK(!missing_location->location.has_value());
    const CalendarEvent* mixed_title = valid.find_by_id("family-trip");
    CHECK(mixed_title != nullptr);
    CHECK(mixed_title->title.find("טיול") != std::string::npos);
    CHECK(mixed_title->title.find("Family trip") != std::string::npos);
    const auto crowded = calendar::filter_and_sort_agenda(valid, {{2028, 3, 3}, 1835654400, 1835740800});
    CHECK(crowded.size() > calendar::kMaxAgendaRows);
    const auto february_28 = calendar::filter_and_sort_agenda(valid, {{2028, 2, 28}, 1835308800, 1835395200});
    CHECK(std::any_of(february_28.begin(), february_28.end(), [](const CalendarEvent& event) { return event.id == "family-trip"; }));
    CHECK(std::any_of(february_28.begin(), february_28.end(), [](const CalendarEvent& event) { return event.id == "overnight"; }));
    const auto february_29 = calendar::filter_and_sort_agenda(valid, {{2028, 2, 29}, 1835395200, 1835481600});
    CHECK(std::any_of(february_29.begin(), february_29.end(), [](const CalendarEvent& event) { return event.id == "leap-day"; }));
    CHECK(std::any_of(february_29.begin(), february_29.end(), [](const CalendarEvent& event) { return event.id == "overnight"; }));
    const auto march_1 = calendar::filter_and_sort_agenda(valid, {{2028, 3, 1}, 1835481600, 1835568000});
    CHECK(std::any_of(march_1.begin(), march_1.end(), [](const CalendarEvent& event) { return event.id == "equal-a"; }));
    CHECK(std::any_of(march_1.begin(), march_1.end(), [](const CalendarEvent& event) { return event.id == "equal-b"; }));
    const auto march_2 = calendar::filter_and_sort_agenda(valid, {{2028, 3, 2}, 1835568000, 1835654400});
    CHECK(std::any_of(march_2.begin(), march_2.end(), [](const CalendarEvent& event) { return event.id == "untitled"; }));
    const auto march_4 = calendar::filter_and_sort_agenda(valid, {{2028, 3, 4}, 1835740800, 1835827200});
    CHECK(std::any_of(march_4.begin(), march_4.end(), [](const CalendarEvent& event) { return event.id == "long-title"; }));
    CHECK(calendar::display_title(*valid.find_by_id("untitled")) == "(Untitled event)");
    calendar::MockCalendarProvider provider({timed("provider-event", 1, 2)}, ProviderState::Ready);
    CHECK(provider.current().state == ProviderState::Ready);
    provider.set_state(ProviderState::Loading);
    CHECK(provider.current().state == ProviderState::Loading);
    provider.set_state(ProviderState::Stale);
    CHECK(provider.current().state == ProviderState::Stale);
    provider.set_state(ProviderState::Error);
    CHECK(provider.current().state == ProviderState::Error);
    provider.replace({timed("refreshed-event", 3, 4)}, ProviderState::Stale);
    CHECK(provider.current().state == ProviderState::Stale);
    CHECK(provider.current().snapshot->find_by_id("provider-event") == nullptr);
    CHECK(provider.current().snapshot->find_by_id("refreshed-event") != nullptr);
    bool rejected_duplicate = false;
    try { CalendarSnapshot duplicate({timed("same", 1, 2), timed("same", 3, 4)}); } catch (const std::invalid_argument&) { rejected_duplicate = true; }
    CHECK(rejected_duplicate);
    bool rejected_overflow = false;
    try { calendar::CalendarEventList events; for (int i = 0; i < 257; ++i) events.push_back(timed("e" + std::to_string(i), i, i + 1)); CalendarSnapshot overflow(std::move(events)); } catch (const std::invalid_argument&) { rejected_overflow = true; }
    CHECK(rejected_overflow);
}

void test_provider_states_and_navigation() {
    FixedClock clock({2028, 2, 29});
    NavigationState state(clock);
    CHECK(state.current_day() == CivilDate{2028, 2, 29});
    CHECK(state.selected_day() == state.current_day());
    CHECK(state.view_mode() == CalendarViewMode::Week);
    CHECK(state.visible_period() == calendar::CalendarPeriod{{2028, 2, 27}, {2028, 3, 5}});
    calendar::MockCalendarProvider provider({timed("stable-id", 1, 2)}, ProviderState::Ready);
    const auto initial = provider.current();
    const CalendarSnapshot& snapshot = *initial.snapshot;
    CHECK(!state.open_event("missing-id", snapshot, 7));
    CHECK(state.open_event("stable-id", snapshot, 42));
    CHECK(state.screen() == NavigationScreen::EventDetails);
    CHECK(state.agenda_scroll_position() == 42);
    provider.set_state(ProviderState::Stale);
    state.apply_provider_result(provider.current());
    CHECK(state.provider_state() == ProviderState::Stale);
    CHECK(state.screen() == NavigationScreen::EventDetails);
    provider.replace({}, ProviderState::Error);
    state.apply_provider_result(provider.current());
    CHECK(state.provider_state() == ProviderState::Error);
    CHECK(state.screen() == NavigationScreen::Agenda);
    CHECK(!state.selected_event_id().has_value());
    state.select_day({2028, 3, 1});
    CHECK(state.selected_day() == CivilDate{2028, 3, 1});
    CHECK(state.agenda_scroll_position() == 0);
    state.set_current_day({2028, 3, 2});
    CHECK(state.current_day() == CivilDate{2028, 3, 2});
    CHECK(state.selected_day() == CivilDate{2028, 3, 1});
    state.select_day({2028, 3, 2});
    state.set_current_day({2028, 3, 3});
    CHECK(state.selected_day() == CivilDate{2028, 3, 3});
    state.apply_provider_result({ProviderState::Loading, nullptr});
    CHECK(state.provider_state() == ProviderState::Loading);
}

void test_period_navigation_state() {
    FixedClock clock({2028, 12, 31});
    NavigationState state(clock);

    state.next_period();
    CHECK(state.current_day() == CivilDate{2028, 12, 31});
    CHECK(state.selected_day() == CivilDate{2029, 1, 7});
    CHECK(state.visible_period() == calendar::CalendarPeriod{{2029, 1, 7}, {2029, 1, 14}});
    state.previous_period();
    CHECK(state.selected_day() == CivilDate{2028, 12, 31});

    state.select_day({2028, 1, 31});
    state.show_month_view();
    CHECK(state.view_mode() == CalendarViewMode::Month);
    CHECK(state.visible_anchor_day() == CivilDate{2028, 1, 31});
    CHECK(state.visible_period() == calendar::CalendarPeriod{{2027, 12, 26}, {2028, 2, 6}});
    state.next_period();
    CHECK(state.selected_day() == CivilDate{2028, 2, 29});
    CHECK(state.visible_period() == calendar::CalendarPeriod{{2028, 1, 30}, {2028, 3, 12}});
    state.next_period();
    CHECK(state.selected_day() == CivilDate{2028, 3, 29});
    state.previous_period();
    CHECK(state.selected_day() == CivilDate{2028, 2, 29});

    state.show_week_view();
    CHECK(state.view_mode() == CalendarViewMode::Week);
    CHECK(calendar::contains(state.visible_period(), state.selected_day()));
    state.show_today();
    CHECK(state.selected_day() == state.current_day());
    CHECK(state.visible_anchor_day() == state.current_day());
    CHECK(state.view_mode() == CalendarViewMode::Week);

    state.show_month_view();
    state.select_day({2029, 1, 15});
    state.set_current_day({2029, 1, 1});
    CHECK(state.selected_day() == CivilDate{2029, 1, 15});
    CHECK(state.visible_anchor_day() == CivilDate{2029, 1, 15});
}

void test_snapshot_change_detection() {
    calendar::CalendarEventList events{timed("first", 1, 2), timed("second", 3, 4)};
    CalendarSnapshot snapshot(events);
    CHECK(calendar::snapshot_matches_events(snapshot, events));

    std::rotate(events.begin(), events.begin() + 1, events.end());
    CHECK(calendar::snapshot_matches_events(snapshot, events));

    events[0].title += " updated";
    CHECK(!calendar::snapshot_matches_events(snapshot, events));
}

}  // namespace

int main() {
    test_dates_and_day_window();
    test_week_and_month_periods();
    test_compact_event_count_labels();
    test_end_exclusive_overlap_and_sorting();
    test_snapshot_bounds_and_fixture_coverage();
    test_provider_states_and_navigation();
    test_period_navigation_state();
    test_snapshot_change_detection();
    if (failures != 0) return 1;
    std::cout << "calendar core tests passed\n";
    return 0;
}
