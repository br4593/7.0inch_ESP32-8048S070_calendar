#include "app/ical_feed.hpp"
#include "app/calendar_provider.hpp"

#include <iostream>
#include <cstdio>
#include <string>
#include <type_traits>

namespace {

int failures = 0;
#define CHECK(...) do { if (!(__VA_ARGS__)) { ++failures; std::cerr << __FUNCTION__ << ": " #__VA_ARGS__ " failed at " << __LINE__ << '\n'; } } while (false)

void append_timed_event(std::string& feed, const std::string& uid,
                        const char* start, const char* end,
                        const std::string& title = {}) {
    feed += "BEGIN:VEVENT\nUID:" + uid + "\nSUMMARY:" + (title.empty() ? uid : title) +
            "\nDTSTART:" + start + "\nDTEND:" + end + "\nEND:VEVENT\n";
}

calendar::IcalFeedOptions week_options() {
    calendar::IcalFeedOptions options;
    options.selection_window = calendar::IcalSelectionWindow{
        {2026, 9, 12}, {2026, 9, 19}, 1789171200, 1789776000};
    return options;
}

void check_reports_equal(const calendar::IcalParseReport& expected,
                         const calendar::IcalParseReport& actual) {
    CHECK(calendar::snapshot_matches_events(calendar::CalendarSnapshot(expected.events),
                                            actual.events));
    CHECK(actual.events_seen == expected.events_seen);
    CHECK(actual.skipped_malformed == expected.skipped_malformed);
    CHECK(actual.skipped_unsupported_timezone == expected.skipped_unsupported_timezone);
    CHECK(actual.skipped_recurrence == expected.skipped_recurrence);
    CHECK(actual.skipped_duplicate == expected.skipped_duplicate);
    CHECK(actual.skipped_outside_window == expected.skipped_outside_window);
    CHECK(actual.skipped_capacity == expected.skipped_capacity);
    CHECK(actual.matched_event_count == expected.matched_event_count);
    CHECK(actual.events_truncated == expected.events_truncated);
    CHECK(actual.summary_count == expected.summary_count);
    CHECK(actual.input_too_large == expected.input_too_large);
    for (std::size_t index = 0; index < expected.summary_count; ++index) {
        CHECK(actual.summary_counts[index].event_count ==
              expected.summary_counts[index].event_count);
        CHECK(actual.summary_counts[index].overflow == expected.summary_counts[index].overflow);
    }
}

calendar::IcalParseReport parse_incrementally(std::string_view feed,
                                              const calendar::IcalFeedOptions& options,
                                              std::size_t byte_budget) {
    calendar::IcalParseSession session(feed, options);
    CHECK(byte_budget > 0);
    if (!session.done()) CHECK(!session.step(0));
    std::size_t steps = 0;
    while (!session.done()) {
        session.step(byte_budget);
        ++steps;
        CHECK(steps <= feed.size() + 1);
    }
    return session.take_report();
}

void test_all_day_utc_and_folded_text() {
    const std::string feed =
        "BEGIN:VCALENDAR\r\n"
        "BEGIN:VEVENT\r\n"
        "UID:all-day-1\r\n"
        "SUMMARY:Holiday\r\n"
        "DTSTART;VALUE=DATE:20260912\r\n"
        "DTEND;VALUE=DATE:20260914\r\n"
        "END:VEVENT\r\n"
        "BEGIN:VEVENT\r\n"
        "UID:utc-1\r\n"
        "SUMMARY:Morning \r\n"
        " meeting\\, with team\\nnext item\r\n"
        "LOCATION:Desk\\, home\r\n"
        "DTSTART:20260912T123000Z\r\n"
        "DTEND:20260912T133000Z\r\n"
        "END:VEVENT\r\n"
        "END:VCALENDAR\r\n";
    const calendar::IcalParseReport result = calendar::parse_ical_feed(feed);
    CHECK(result.events_seen == 2);
    CHECK(result.events.size() == 2);
    CHECK(result.skipped_malformed == 0);
    const calendar::CalendarEvent& all_day = result.events[0];
    CHECK(all_day.time_kind == calendar::TimeKind::AllDay);
    CHECK(all_day.start_date == calendar::CivilDate{2026, 9, 12});
    CHECK(all_day.end_date_exclusive == calendar::CivilDate{2026, 9, 14});
    const calendar::CalendarEvent& timed = result.events[1];
    CHECK(timed.time_kind == calendar::TimeKind::Timed);
    CHECK(timed.start_utc == 1789216200);
    CHECK(timed.end_utc == 1789219800);
    CHECK(timed.title == "Morning meeting, with team next item");
    CHECK(timed.location.has_value());
    CHECK(*timed.location == "Desk, home");
    CHECK(timed.id.size() == 21);
    CHECK(timed.calendar.id == "google-ical");
}

void test_timezone_and_malformed_events_are_reported() {
    const std::string feed =
        "BEGIN:VEVENT\n"
        "UID:local-time\n"
        "DTSTART;TZID=Asia/Jerusalem:20260912T120000\n"
        "DTEND;TZID=Asia/Jerusalem:20260912T130000\n"
        "END:VEVENT\n"
        "BEGIN:VEVENT\n"
        "UID:missing-end\n"
        "DTSTART:20260912T120000Z\n"
        "END:VEVENT\n"
        "BEGIN:VEVENT\n"
        "UID:floating-time\n"
        "DTSTART:20260912T120000\n"
        "DTEND:20260912T130000\n"
        "END:VEVENT\n";
    const calendar::IcalParseReport result = calendar::parse_ical_feed(feed);
    CHECK(result.events.empty());
    CHECK(result.events_seen == 3);
    CHECK(result.skipped_unsupported_timezone == 1);
    CHECK(result.skipped_malformed == 2);
}

void test_limits_duplicates_and_options() {
    static_assert(calendar::kMaxIcalFeedBytes == 1024U * 1024U);

    const std::string duplicate =
        "BEGIN:VEVENT\nUID:same\nDTSTART:20260912T120000Z\nDTEND:20260912T130000Z\nEND:VEVENT\n"
        "BEGIN:VEVENT\nUID:same\nDTSTART:20260913T120000Z\nDTEND:20260913T130000Z\nEND:VEVENT\n";
    const calendar::IcalParseReport result = calendar::parse_ical_feed(duplicate);
    CHECK(result.events.size() == 1);
    CHECK(result.skipped_duplicate == 1);

    const std::string too_large(calendar::kMaxIcalFeedBytes + 1, 'x');
    const calendar::IcalParseReport too_large_result = calendar::parse_ical_feed(too_large);
    CHECK(too_large_result.input_too_large);
    CHECK(too_large_result.events.empty());

    const std::string at_limit(calendar::kMaxIcalFeedBytes, '\n');
    const calendar::IcalParseReport at_limit_result = calendar::parse_ical_feed(at_limit);
    CHECK(!at_limit_result.input_too_large);

    std::string above_old_limit = "BEGIN:VCALENDAR\nVERSION:2.0\n";
    above_old_limit.append(300U * 1024U, '\n');
    above_old_limit +=
        "BEGIN:VEVENT\nUID:large-feed\nSUMMARY:Still accepted\n"
        "DTSTART:20260912T120000Z\nDTEND:20260912T130000Z\nEND:VEVENT\n"
        "END:VCALENDAR\n";
    const calendar::IcalParseReport above_old_limit_result =
        calendar::parse_ical_feed(above_old_limit);
    CHECK(!above_old_limit_result.input_too_large);
    CHECK(above_old_limit_result.events.size() == 1);

    calendar::IcalFeedOptions invalid_options;
    invalid_options.calendar.id.clear();
    const calendar::IcalParseReport invalid_result = calendar::parse_ical_feed(duplicate, invalid_options);
    CHECK(invalid_result.events.empty());
}

void test_recurrence_master_is_not_presented_as_one_event() {
    const std::string feed =
        "BEGIN:VEVENT\n"
        "UID:weekly\n"
        "SUMMARY:Weekly planning\n"
        "DTSTART:20260912T120000Z\n"
        "DTEND:20260912T130000Z\n"
        "RRULE:FREQ=WEEKLY;COUNT=5\n"
        "END:VEVENT\n";
    const calendar::IcalParseReport result = calendar::parse_ical_feed(feed);
    CHECK(result.events.empty());
    CHECK(result.events_seen == 1);
    CHECK(result.skipped_recurrence == 1);
    CHECK(result.skipped_malformed == 0);
}

void test_week_window_selects_relevant_events_from_the_full_feed() {
    std::string feed = "BEGIN:VCALENDAR\nVERSION:2.0\n";
    for (int index = 0; index < 40; ++index) {
        append_timed_event(feed, "old-" + std::to_string(index),
                           "20260901T100000Z", "20260901T110000Z");
    }
    static constexpr const char* kDates[]{
        "20260912", "20260913", "20260914", "20260915",
        "20260916", "20260917", "20260918"};
    for (int index = 6; index >= 0; --index) {
        const std::string start = std::string(kDates[index]) + "T120000Z";
        const std::string end = std::string(kDates[index]) + "T130000Z";
        append_timed_event(feed, "week-" + std::to_string(index),
                           start.c_str(), end.c_str());
    }
    feed += "END:VCALENDAR\n";

    const calendar::IcalParseReport result = calendar::parse_ical_feed(feed, week_options());
    CHECK(result.events_seen == 47);
    CHECK(result.events.size() == 7);
    CHECK(result.skipped_outside_window == 40);
    CHECK(result.skipped_malformed == 0);
    for (std::size_t index = 0; index < result.events.size(); ++index) {
        const std::string expected = "week-" + std::to_string(index);
        CHECK(std::string_view(result.events[index].title.data(), result.events[index].title.size()) ==
              expected);
    }
}

void test_capacity_keeps_earliest_events_independent_of_feed_order() {
    auto make_feed = [](bool near_first) {
        std::string feed = "BEGIN:VCALENDAR\nVERSION:2.0\n";
        if (near_first) {
            append_timed_event(feed, "near", "20260912T010000Z", "20260912T020000Z");
        }
        for (int index = near_first ? 259 : 0;
             near_first ? index >= 0 : index < 260;
             near_first ? --index : ++index) {
            char uid[16];
            std::snprintf(uid, sizeof(uid), "far-%03d", index);
            append_timed_event(feed, uid, "20260918T120000Z", "20260918T130000Z");
        }
        if (!near_first) {
            append_timed_event(feed, "near", "20260912T010000Z", "20260912T020000Z");
        }
        feed += "END:VCALENDAR\n";
        return feed;
    };

    const calendar::IcalParseReport forward =
        calendar::parse_ical_feed(make_feed(false), week_options());
    const calendar::IcalParseReport reverse =
        calendar::parse_ical_feed(make_feed(true), week_options());
    CHECK(forward.events_seen == 261);
    CHECK(forward.events.size() == calendar::kMaxSnapshotEvents);
    CHECK(forward.skipped_capacity == 5);
    CHECK(reverse.skipped_capacity == 5);
    CHECK(forward.events.front().title == "near");
    const calendar::CalendarSnapshot snapshot(forward.events);
    CHECK(calendar::snapshot_matches_events(snapshot, reverse.events));
}

void test_selection_window_is_end_exclusive_and_supports_months() {
    const std::string week_feed =
        "BEGIN:VEVENT\nUID:timed-ends-at-start\nDTSTART:20260911T230000Z\nDTEND:20260912T000000Z\nEND:VEVENT\n"
        "BEGIN:VEVENT\nUID:timed-spans-start\nDTSTART:20260911T235900Z\nDTEND:20260912T000100Z\nEND:VEVENT\n"
        "BEGIN:VEVENT\nUID:timed-starts-at-end\nDTSTART:20260919T000000Z\nDTEND:20260919T010000Z\nEND:VEVENT\n"
        "BEGIN:VEVENT\nUID:all-day-ends-at-start\nDTSTART;VALUE=DATE:20260911\nDTEND;VALUE=DATE:20260912\nEND:VEVENT\n"
        "BEGIN:VEVENT\nUID:all-day-spans-start\nDTSTART;VALUE=DATE:20260911\nDTEND;VALUE=DATE:20260913\nEND:VEVENT\n"
        "BEGIN:VEVENT\nUID:all-day-starts-at-end\nDTSTART;VALUE=DATE:20260919\nDTEND;VALUE=DATE:20260920\nEND:VEVENT\n";
    const calendar::IcalParseReport week = calendar::parse_ical_feed(week_feed, week_options());
    CHECK(week.events.size() == 2);
    CHECK(week.skipped_outside_window == 4);

    calendar::IcalFeedOptions month;
    month.selection_window = calendar::IcalSelectionWindow{
        {2028, 2, 1}, {2028, 3, 1}, 1, 2};
    const std::string month_feed =
        "BEGIN:VEVENT\nUID:leap-day\nDTSTART;VALUE=DATE:20280229\nDTEND;VALUE=DATE:20280301\nEND:VEVENT\n"
        "BEGIN:VEVENT\nUID:march-first\nDTSTART;VALUE=DATE:20280301\nDTEND;VALUE=DATE:20280302\nEND:VEVENT\n";
    const calendar::IcalParseReport month_result = calendar::parse_ical_feed(month_feed, month);
    CHECK(month_result.events.size() == 1);
    CHECK(month_result.events.front().title.empty());
    CHECK(month_result.skipped_outside_window == 1);
}

void test_incremental_parser_matches_synchronous_at_small_budgets() {
    static_assert(!std::is_copy_constructible_v<calendar::IcalParseSession>);
    static_assert(std::is_move_constructible_v<calendar::IcalParseSession>);

    const std::string feed =
        "BEGIN:VCALENDAR\r\n"
        "BEGIN:VEVENT\r\n"
        "UID:folded-final\r\n"
        "SUMMARY:Split across \r\n"
        " byte-budget boundaries\r\n"
        "LOCATION:First floor\r\n"
        "DTSTART:20260912T123000Z\r\n"
        "DTEND:20260912T133000Z\r\n"
        "END:VEVENT";
    const calendar::IcalParseReport synchronous = calendar::parse_ical_feed(feed);
    CHECK(synchronous.events.size() == 1);
    CHECK(synchronous.events.front().title == "Split across byte-budget boundaries");

    for (const std::size_t budget : {1U, 2U, 3U, 7U, 31U}) {
        const calendar::IcalParseReport incremental =
            parse_incrementally(feed, calendar::IcalFeedOptions{}, budget);
        check_reports_equal(synchronous, incremental);
    }
}

void test_summary_counts_are_independent_of_retained_capacity() {
    std::string feed = "BEGIN:VCALENDAR\n";
    for (int index = 0; index < 300; ++index) {
        char uid[24];
        std::snprintf(uid, sizeof(uid), "summary-%03d", index);
        if (index < 150) {
            append_timed_event(feed, uid, "20260912T120000Z", "20260912T130000Z");
        } else {
            append_timed_event(feed, uid, "20260913T120000Z", "20260913T130000Z");
        }
    }
    // Duplicate tracking spans the complete scan, including events that did
    // not fit in the retained snapshot.
    append_timed_event(feed, "summary-299", "20260913T120000Z", "20260913T130000Z");
    feed +=
        "BEGIN:VEVENT\nUID:two-day\nSUMMARY:Two days\n"
        "DTSTART;VALUE=DATE:20260912\nDTEND;VALUE=DATE:20260914\nEND:VEVENT\n"
        "END:VCALENDAR\n";

    calendar::IcalFeedOptions options;
    options.selection_window = calendar::IcalSelectionWindow{
        {2026, 9, 12}, {2026, 9, 14}, 1789171200, 1789344000};
    calendar::IcalSummaryBuckets buckets;
    buckets.count = 2;
    buckets.windows[0] = {{2026, 9, 12}, 1789171200, 1789257600};
    buckets.windows[1] = {{2026, 9, 13}, 1789257600, 1789344000};
    options.summary_buckets = buckets;

    const calendar::IcalParseReport result = calendar::parse_ical_feed(feed, options);
    CHECK(result.events_seen == 302);
    CHECK(result.matched_event_count == 301);
    CHECK(result.skipped_duplicate == 1);
    CHECK(result.events.size() == calendar::kMaxSnapshotEvents);
    CHECK(result.skipped_capacity == 45);
    CHECK(result.events_truncated);
    CHECK(result.summary_count == 2);
    CHECK(result.summary_counts[0].event_count == 151);
    CHECK(result.summary_counts[1].event_count == 151);
    CHECK(!result.summary_counts[0].overflow);
    CHECK(!result.summary_counts[1].overflow);

    const calendar::IcalParseReport incremental = parse_incrementally(feed, options, 5);
    check_reports_equal(result, incremental);
}

void test_composite_feed_preserves_calendar_identity_and_duplicate_uids() {
    const std::string feed =
        "X-ESP32-CALENDAR-ID:family\n"
        "X-ESP32-CALENDAR-NAME:Family\n"
        "X-ESP32-CALENDAR-COLOR:E85D75\n"
        "BEGIN:VCALENDAR\n"
        "BEGIN:VEVENT\nUID:shared\nSUMMARY:Family event\n"
        "DTSTART:20260912T100000Z\nDTEND:20260912T110000Z\nEND:VEVENT\n"
        "END:VCALENDAR\n"
        "X-ESP32-CALENDAR-ID:work\n"
        "X-ESP32-CALENDAR-NAME:Work calendar\n"
        "X-ESP32-CALENDAR-COLOR:4285f4\n"
        "BEGIN:VCALENDAR\n"
        "BEGIN:VEVENT\nUID:shared\nSUMMARY:Work event\n"
        "DTSTART:20260912T120000Z\nDTEND:20260912T130000Z\nEND:VEVENT\n"
        "END:VCALENDAR\n";

    const calendar::IcalParseReport result = calendar::parse_ical_feed(feed);
    CHECK(result.events_seen == 2);
    CHECK(result.matched_event_count == 2);
    CHECK(result.skipped_duplicate == 0);
    CHECK(result.events.size() == 2);
    CHECK(result.events[0].calendar.id == "family");
    CHECK(result.events[0].calendar.display_name == "Family");
    CHECK(result.events[0].calendar.color_rgb888 == 0xE85D75U);
    CHECK(result.events[1].calendar.id == "work");
    CHECK(result.events[1].calendar.display_name == "Work calendar");
    CHECK(result.events[1].calendar.color_rgb888 == 0x4285F4U);
    CHECK(result.events[0].id != result.events[1].id);
}

void test_composite_markers_are_bounded_and_malformed_values_are_ignored() {
    std::string feed =
        "X-ESP32-CALENDAR-ID:valid\n"
        "X-ESP32-CALENDAR-NAME:Valid name\n"
        "X-ESP32-CALENDAR-COLOR:123ABC\n"
        "BEGIN:VCALENDAR\n"
        "X-ESP32-CALENDAR-ID:spoofed\n"
        "X-ESP32-CALENDAR-NAME:Spoofed name\n"
        "X-ESP32-CALENDAR-COLOR:FFFFFF\n"
        "BEGIN:VEVENT\nUID:first\nSUMMARY:First\n"
        "DTSTART:20260912T080000Z\nDTEND:20260912T090000Z\nEND:VEVENT\n"
        "END:VCALENDAR\n"
        "X-ESP32-CALENDAR-ID:\n"
        "X-ESP32-CALENDAR-NAME:\n"
        "X-ESP32-CALENDAR-COLOR:12GG00\n";
    feed += "X-ESP32-CALENDAR-ID:" +
            std::string(calendar::kMaxCalendarIdBytes + 1, 'i') + "\n";
    feed += "X-ESP32-CALENDAR-NAME:" +
            std::string(calendar::kMaxCalendarNameBytes + 1, 'n') + "\n";
    feed += "X-ESP32-CALENDAR-NAME:" +
            std::string(calendar::kMaxIcalLogicalLineBytes + 1, 'x') + "\n";
    feed +=
        "BEGIN:VCALENDAR\n"
        "BEGIN:VEVENT\nUID:bounded\nSUMMARY:Still valid\n"
        "DTSTART:20260912T100000Z\nDTEND:20260912T110000Z\nEND:VEVENT\n"
        "END:VCALENDAR\n";

    const calendar::IcalParseReport result = calendar::parse_ical_feed(feed);
    CHECK(result.events_seen == 2);
    CHECK(result.matched_event_count == 2);
    CHECK(result.skipped_malformed == 0);
    CHECK(result.events.size() == 2);
    CHECK(result.events.front().calendar.id == "valid");
    CHECK(result.events.front().calendar.display_name == "Valid name");
    CHECK(result.events.front().calendar.color_rgb888 == 0x123ABCU);
    CHECK(result.events.back().calendar.id == "valid");
    CHECK(result.events.back().calendar.display_name == "Valid name");
    CHECK(result.events.back().calendar.color_rgb888 == 0x123ABCU);
}

void test_incomplete_or_interrupted_triplet_does_not_switch_identity() {
    const std::string feed =
        "X-ESP32-CALENDAR-ID:partial\n"
        "X-ESP32-CALENDAR-NAME:Partial\n"
        "BEGIN:VCALENDAR\n"
        "BEGIN:VEVENT\nUID:partial-event\nSUMMARY:Partial\n"
        "DTSTART:20260912T100000Z\nDTEND:20260912T110000Z\nEND:VEVENT\n"
        "END:VCALENDAR\n"
        "X-ESP32-CALENDAR-ID:interrupted\n"
        "COMMENT:not-immediately-before\n"
        "X-ESP32-CALENDAR-NAME:Interrupted\n"
        "X-ESP32-CALENDAR-COLOR:112233\n"
        "BEGIN:VCALENDAR\n"
        "BEGIN:VEVENT\nUID:interrupted-event\nSUMMARY:Interrupted\n"
        "DTSTART:20260912T120000Z\nDTEND:20260912T130000Z\nEND:VEVENT\n"
        "END:VCALENDAR\n";

    const auto result = calendar::parse_ical_feed(feed);
    CHECK(result.events.size() == 2);
    CHECK(result.events_seen == 2);
    CHECK(result.events[0].calendar.id == "google-ical");
    CHECK(result.events[1].calendar.id == "google-ical");
}

void test_single_feed_uses_supplied_calendar_in_stable_id() {
    const std::string feed =
        "BEGIN:VEVENT\nUID:same\nSUMMARY:One\n"
        "DTSTART:20260912T100000Z\nDTEND:20260912T110000Z\nEND:VEVENT\n";
    calendar::IcalFeedOptions first;
    first.calendar = {"first", "First", 0x111111};
    calendar::IcalFeedOptions second;
    second.calendar = {"second", "Second", 0x222222};

    const auto first_result = calendar::parse_ical_feed(feed, first);
    const auto second_result = calendar::parse_ical_feed(feed, second);
    CHECK(first_result.events.size() == 1);
    CHECK(second_result.events.size() == 1);
    CHECK(first_result.events.front().calendar.id == "first");
    CHECK(second_result.events.front().calendar.id == "second");
    CHECK(first_result.events.front().id != second_result.events.front().id);
}

}  // namespace

int main() {
    test_all_day_utc_and_folded_text();
    test_timezone_and_malformed_events_are_reported();
    test_limits_duplicates_and_options();
    test_recurrence_master_is_not_presented_as_one_event();
    test_week_window_selects_relevant_events_from_the_full_feed();
    test_capacity_keeps_earliest_events_independent_of_feed_order();
    test_selection_window_is_end_exclusive_and_supports_months();
    test_incremental_parser_matches_synchronous_at_small_budgets();
    test_summary_counts_are_independent_of_retained_capacity();
    test_composite_feed_preserves_calendar_identity_and_duplicate_uids();
    test_composite_markers_are_bounded_and_malformed_values_are_ignored();
    test_incomplete_or_interrupted_triplet_does_not_switch_identity();
    test_single_feed_uses_supplied_calendar_in_stable_id();
    if (failures != 0) return 1;
    std::cout << "iCalendar feed tests passed\n";
    return 0;
}
