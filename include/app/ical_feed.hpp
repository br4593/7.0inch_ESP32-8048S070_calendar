#pragma once

#include "app/calendar_types.hpp"
#include "calendar_limits.hpp"

#include <cstddef>
#include <cstdint>
#include <array>
#include <memory>
#include <optional>
#include <string_view>

namespace calendar {

// Logical lines remain independently bounded even though the complete feed may
// occupy the larger PSRAM-backed transfer buffer.
constexpr std::size_t kMaxIcalLogicalLineBytes = 512;
constexpr std::size_t kMaxIcalSummaryBuckets = 42;

// End-exclusive local civil and UTC boundaries for selecting a display
// period. All-day events use the civil dates; timed events use UTC.
struct IcalSelectionWindow {
    CivilDate start_date;
    CivilDate end_date_exclusive;
    std::int64_t start_utc = 0;
    std::int64_t end_utc = 0;
};

// Optional day buckets used by month-style views. A valid event may overlap
// more than one bucket. Counts are gathered while scanning the complete feed,
// so they are independent of the bounded retained event snapshot.
struct IcalSummaryBuckets {
    std::array<DayWindow, kMaxIcalSummaryBuckets> windows{};
    std::size_t count = 0;
};

struct IcalSummaryCount {
    std::uint16_t event_count = 0;
    bool overflow = false;
};

struct IcalFeedOptions {
    CalendarIdentity calendar{"google-ical", "Google Calendar", 0x4285F4};
    std::optional<IcalSelectionWindow> selection_window;
    std::optional<IcalSummaryBuckets> summary_buckets;
};

// A parse report is deliberately numeric rather than retaining feed text: the
// feed URL and event contents must not be copied into diagnostics or logs.
struct IcalParseReport {
    CalendarEventList events;
    std::size_t events_seen = 0;
    std::size_t skipped_malformed = 0;
    std::size_t skipped_unsupported_timezone = 0;
    // VEVENTs with RRULE are masters, not individual occurrences. They are
    // skipped until recurrence expansion is deliberately implemented.
    std::size_t skipped_recurrence = 0;
    std::size_t skipped_duplicate = 0;
    std::size_t skipped_outside_window = 0;
    std::size_t skipped_capacity = 0;
    // Valid, selected, non-duplicate events encountered before applying the
    // retained snapshot capacity.
    std::size_t matched_event_count = 0;
    bool events_truncated = false;
    std::array<IcalSummaryCount, kMaxIcalSummaryBuckets> summary_counts{};
    std::size_t summary_count = 0;
    bool input_too_large = false;
};

// Incrementally parses a borrowed feed view. The feed storage must remain
// valid for the lifetime of the session. Each step consumes at most
// byte_budget feed bytes and returns true once parsing is complete. The
// retained event collection uses CalendarEventList's PSRAM allocator on the
// ESP32. take_report() returns an empty report if called before completion.
class IcalParseSession {
public:
    explicit IcalParseSession(std::string_view feed,
                              const IcalFeedOptions& options = {});
    ~IcalParseSession();

    IcalParseSession(IcalParseSession&&) noexcept;
    IcalParseSession& operator=(IcalParseSession&&) noexcept;
    IcalParseSession(const IcalParseSession&) = delete;
    IcalParseSession& operator=(const IcalParseSession&) = delete;

    bool step(std::size_t byte_budget);
    bool done() const;
    IcalParseReport take_report();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Parses a bounded iCalendar (RFC 5545) feed into the calendar model. It
// supports all-day VALUE=DATE values and UTC DATE-TIME values ending in Z.
// A bounded composite document may select the identity for one VCALENDAR with
// an atomic X-ESP32-CALENDAR-ID, X-ESP32-CALENDAR-NAME and
// X-ESP32-CALENDAR-COLOR triplet immediately before BEGIN:VCALENDAR.
// Recurrence expansion and named/floating time zones are intentionally left to
// a later adapter; those VEVENTs are skipped and counted in the report.
IcalParseReport parse_ical_feed(std::string_view feed,
                                const IcalFeedOptions& options = {});

}  // namespace calendar
