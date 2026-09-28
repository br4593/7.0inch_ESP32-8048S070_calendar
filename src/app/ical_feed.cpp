#include "app/ical_feed.hpp"

#include "app/calendar_provider.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

namespace calendar {
namespace {

enum class TimestampKind { Missing, Date, UtcDateTime, UnsupportedTimezone, Malformed };

struct Timestamp {
    TimestampKind kind = TimestampKind::Missing;
    CivilDate date;
    std::int64_t utc = 0;
};

struct EventBuilder {
    bool active = false;
    bool bad = false;
    bool unsupported_timezone = false;
    bool oversized_required_field = false;
    bool recurring_master = false;
    std::string uid;
    std::string summary;
    std::optional<std::string> location;
    Timestamp start;
    Timestamp end;
};

constexpr std::string_view kCalendarIdMarker = "X-ESP32-CALENDAR-ID:";
constexpr std::string_view kCalendarNameMarker = "X-ESP32-CALENDAR-NAME:";
constexpr std::string_view kCalendarColorMarker = "X-ESP32-CALENDAR-COLOR:";

struct PendingCalendarIdentity {
    CalendarIdentity calendar;
    unsigned stage = 0;

    void reset() {
        calendar = {};
        stage = 0;
    }
};

bool ascii_iequals(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        const auto a = static_cast<unsigned char>(left[index]);
        const auto b = static_cast<unsigned char>(right[index]);
        if (std::toupper(a) != std::toupper(b)) return false;
    }
    return true;
}

bool has_parameter(std::string_view parameters, std::string_view wanted_name,
                   std::string_view wanted_value) {
    while (!parameters.empty()) {
        const std::size_t separator = parameters.find(';');
        const std::string_view parameter = parameters.substr(0, separator);
        const std::size_t equals = parameter.find('=');
        if (equals != std::string_view::npos &&
            ascii_iequals(parameter.substr(0, equals), wanted_name) &&
            ascii_iequals(parameter.substr(equals + 1), wanted_value)) {
            return true;
        }
        if (separator == std::string_view::npos) break;
        parameters.remove_prefix(separator + 1);
    }
    return false;
}

bool has_parameter_name(std::string_view parameters, std::string_view wanted_name) {
    while (!parameters.empty()) {
        const std::size_t separator = parameters.find(';');
        const std::string_view parameter = parameters.substr(0, separator);
        const std::size_t equals = parameter.find('=');
        if (equals != std::string_view::npos && ascii_iequals(parameter.substr(0, equals), wanted_name)) {
            return true;
        }
        if (separator == std::string_view::npos) break;
        parameters.remove_prefix(separator + 1);
    }
    return false;
}

bool decimal(std::string_view value, int& out) {
    if (value.empty()) return false;
    int parsed = 0;
    for (const char character : value) {
        if (character < '0' || character > '9') return false;
        parsed = parsed * 10 + (character - '0');
    }
    out = parsed;
    return true;
}

bool parse_date(std::string_view value, CivilDate& result) {
    if (value.size() != 8) return false;
    int year = 0;
    int month = 0;
    int day = 0;
    if (!decimal(value.substr(0, 4), year) || !decimal(value.substr(4, 2), month) ||
        !decimal(value.substr(6, 2), day)) {
        return false;
    }
    result = {year, static_cast<unsigned>(month), static_cast<unsigned>(day)};
    return is_valid_date(result);
}

std::int64_t days_from_civil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
    const unsigned month_prime = month + (month > 2 ? static_cast<unsigned>(-3) : 9);
    const unsigned day_of_year = (153 * month_prime + 2) / 5 + day - 1;
    const unsigned day_of_era =
        year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    return era * 146097 + static_cast<int>(day_of_era) - 719468;
}

Timestamp parse_timestamp(std::string_view parameters, std::string_view value) {
    Timestamp result;
    const bool value_is_date = has_parameter(parameters, "VALUE", "DATE");
    const bool has_tzid = has_parameter_name(parameters, "TZID");
    if (value_is_date) {
        if (has_tzid || !parse_date(value, result.date)) {
            result.kind = has_tzid ? TimestampKind::UnsupportedTimezone : TimestampKind::Malformed;
        } else {
            result.kind = TimestampKind::Date;
        }
        return result;
    }

    if (has_tzid) {
        result.kind = TimestampKind::UnsupportedTimezone;
        return result;
    }
    if (value.size() != 16 || value[8] != 'T' || value[15] != 'Z') {
        result.kind = TimestampKind::Malformed;
        return result;
    }

    CivilDate date;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (!parse_date(value.substr(0, 8), date) || !decimal(value.substr(9, 2), hour) ||
        !decimal(value.substr(11, 2), minute) || !decimal(value.substr(13, 2), second) ||
        hour > 23 || minute > 59 || second > 59) {
        result.kind = TimestampKind::Malformed;
        return result;
    }
    result.kind = TimestampKind::UtcDateTime;
    result.utc = days_from_civil(date.year, date.month, date.day) * 86400 + hour * 3600 +
                 minute * 60 + second;
    return result;
}

std::string unescape_text(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '\\' || index + 1 == value.size()) {
            result.push_back(value[index]);
            continue;
        }
        const char escaped = value[++index];
        if (escaped == 'n' || escaped == 'N') {
            result.push_back(' ');
        } else {
            result.push_back(escaped);
        }
    }
    return result;
}

std::uint64_t fnv1a_append(std::uint64_t hash, std::string_view value) {
    for (const unsigned char byte : value) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string event_id_for_uid(std::string_view calendar_id, std::string_view uid) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string id("ical-");
    std::uint64_t hash = fnv1a_append(14695981039346656037ULL, calendar_id);
    // A delimiter prevents pairs such as ("ab", "c") and ("a", "bc")
    // from sharing the same byte sequence.
    hash ^= 0xffU;
    hash *= 1099511628211ULL;
    hash = fnv1a_append(hash, uid);
    for (int shift = 60; shift >= 0; shift -= 4) {
        id.push_back(kHex[(hash >> shift) & 0x0fU]);
    }
    return id;
}

bool valid_calendar(const CalendarIdentity& calendar) {
    return !calendar.id.empty() && calendar.id.size() <= kMaxCalendarIdBytes &&
           !calendar.display_name.empty() && calendar.display_name.size() <= kMaxCalendarNameBytes;
}

bool valid_selection_window(const std::optional<IcalSelectionWindow>& window) {
    return !window ||
           (is_valid_date(window->start_date) && is_valid_date(window->end_date_exclusive) &&
            window->start_date < window->end_date_exclusive && window->start_utc < window->end_utc);
}

bool valid_summary_buckets(const std::optional<IcalSummaryBuckets>& buckets) {
    if (!buckets) return true;
    if (buckets->count > kMaxIcalSummaryBuckets) return false;
    for (std::size_t index = 0; index < buckets->count; ++index) {
        if (!is_valid_day_window(buckets->windows[index])) return false;
    }
    return true;
}

bool overlaps_selection_window(const CalendarEvent& event, const IcalSelectionWindow& window) {
    if (event.time_kind == TimeKind::AllDay) {
        return event.start_date < window.end_date_exclusive &&
               window.start_date < event.end_date_exclusive;
    }
    return event.start_utc < window.end_utc && window.start_utc < event.end_utc;
}

std::int64_t utc_day(std::int64_t seconds) {
    constexpr std::int64_t kSecondsPerDay = 24 * 60 * 60;
    std::int64_t day = seconds / kSecondsPerDay;
    if (seconds < 0 && seconds % kSecondsPerDay != 0) --day;
    return day;
}

std::int64_t event_start_day(const CalendarEvent& event) {
    return event.time_kind == TimeKind::AllDay
               ? days_from_civil(event.start_date.year, event.start_date.month, event.start_date.day)
               : utc_day(event.start_utc);
}

// Deterministic chronological order makes the retained set independent of the
// order used by the calendar server. All-day events sort before timed events
// that begin on the same UTC/civil day, matching the agenda presentation.
bool event_selection_less(const CalendarEvent& left, const CalendarEvent& right) {
    const std::int64_t left_day = event_start_day(left);
    const std::int64_t right_day = event_start_day(right);
    if (left_day != right_day) return left_day < right_day;
    if (left.time_kind != right.time_kind) return left.time_kind == TimeKind::AllDay;
    if (left.time_kind == TimeKind::AllDay) {
        if (left.start_date != right.start_date) return left.start_date < right.start_date;
        if (left.end_date_exclusive != right.end_date_exclusive) {
            return left.end_date_exclusive < right.end_date_exclusive;
        }
    } else {
        if (left.start_utc != right.start_utc) return left.start_utc < right.start_utc;
        if (left.end_utc != right.end_utc) return left.end_utc < right.end_utc;
    }
    return left.id < right.id;
}

struct CalendarStringHash {
    std::size_t operator()(const CalendarString& value) const noexcept {
        // FNV-1a avoids relying on a standard-library specialization for a
        // basic_string with a custom allocator.
        std::size_t hash = sizeof(std::size_t) == 8
            ? static_cast<std::size_t>(1469598103934665603ULL)
            : static_cast<std::size_t>(2166136261U);
        const std::size_t prime = sizeof(std::size_t) == 8
            ? static_cast<std::size_t>(1099511628211ULL)
            : static_cast<std::size_t>(16777619U);
        for (const unsigned char byte : value) {
            hash ^= byte;
            hash *= prime;
        }
        return hash;
    }
};

using SeenEventIds = std::unordered_set<
    CalendarString, CalendarStringHash, std::equal_to<CalendarString>,
    CalendarPsramAllocator<CalendarString>>;

void retain_event(CalendarEvent event, const IcalFeedOptions& options,
                  SeenEventIds& seen_event_ids, IcalParseReport& report) {
    if (options.selection_window && !overlaps_selection_window(event, *options.selection_window)) {
        ++report.skipped_outside_window;
        return;
    }
    if (!seen_event_ids.insert(event.id).second) {
        ++report.skipped_duplicate;
        return;
    }

    ++report.matched_event_count;
    if (options.summary_buckets) {
        for (std::size_t index = 0; index < options.summary_buckets->count; ++index) {
            if (!overlaps_day(event, options.summary_buckets->windows[index])) continue;
            IcalSummaryCount& count = report.summary_counts[index];
            if (count.event_count == std::numeric_limits<std::uint16_t>::max()) {
                count.overflow = true;
            } else {
                ++count.event_count;
            }
        }
    }

    const auto position = std::lower_bound(report.events.begin(), report.events.end(), event,
                                           event_selection_less);
    if (report.events.size() < kMaxSnapshotEvents) {
        report.events.insert(position, std::move(event));
        return;
    }
    if (position == report.events.end()) {
        ++report.skipped_capacity;
        report.events_truncated = true;
        return;
    }

    const std::size_t insertion_index =
        static_cast<std::size_t>(std::distance(report.events.begin(), position));
    report.events.pop_back();
    report.events.insert(report.events.begin() + insertion_index, std::move(event));
    ++report.skipped_capacity;
    report.events_truncated = true;
}

void finish_event(EventBuilder& builder, const IcalFeedOptions& options,
                  const CalendarIdentity& active_calendar,
                  SeenEventIds& seen_event_ids, IcalParseReport& report) {
    if (!builder.active) return;
    ++report.events_seen;
    if (builder.recurring_master) {
        ++report.skipped_recurrence;
    } else if (builder.unsupported_timezone || builder.start.kind == TimestampKind::UnsupportedTimezone ||
        builder.end.kind == TimestampKind::UnsupportedTimezone) {
        ++report.skipped_unsupported_timezone;
    } else if (builder.bad || builder.oversized_required_field || builder.uid.empty() ||
               builder.start.kind == TimestampKind::Missing || builder.end.kind == TimestampKind::Missing ||
               builder.start.kind == TimestampKind::Malformed || builder.end.kind == TimestampKind::Malformed ||
               builder.start.kind != builder.end.kind) {
        ++report.skipped_malformed;
    } else {
        CalendarEvent event;
        event.id = event_id_for_uid(
            std::string_view(active_calendar.id.data(), active_calendar.id.size()), builder.uid);
        event.title = builder.summary;
        event.location = std::move(builder.location);
        event.calendar = active_calendar;
        event.time_kind = builder.start.kind == TimestampKind::Date ? TimeKind::AllDay : TimeKind::Timed;
        if (event.time_kind == TimeKind::AllDay) {
            event.start_date = builder.start.date;
            event.end_date_exclusive = builder.end.date;
        } else {
            event.start_utc = builder.start.utc;
            event.end_utc = builder.end.utc;
        }
        if (!is_valid_event(event)) {
            ++report.skipped_malformed;
        } else {
            retain_event(std::move(event), options, seen_event_ids, report);
        }
    }
    builder = {};
}

bool parse_rgb888(std::string_view value, std::uint32_t& result) {
    if (value.size() != 6) return false;
    std::uint32_t parsed = 0;
    for (const char character : value) {
        unsigned nibble = 0;
        if (character >= '0' && character <= '9') {
            nibble = static_cast<unsigned>(character - '0');
        } else if (character >= 'a' && character <= 'f') {
            nibble = static_cast<unsigned>(character - 'a' + 10);
        } else if (character >= 'A' && character <= 'F') {
            nibble = static_cast<unsigned>(character - 'A' + 10);
        } else {
            return false;
        }
        parsed = (parsed << 4U) | nibble;
    }
    result = parsed;
    return true;
}

bool marker_value(std::string_view line, std::string_view marker, std::string_view& value) {
    if (line.size() < marker.size() || line.substr(0, marker.size()) != marker) return false;
    value = line.substr(marker.size());
    return true;
}

bool consume_calendar_marker(std::string_view line, PendingCalendarIdentity& pending) {
    std::string_view value;
    if (marker_value(line, kCalendarIdMarker, value)) {
        pending.reset();
        if (!value.empty() && value.size() <= kMaxCalendarIdBytes) {
            pending.calendar.id.assign(value.data(), value.size());
            pending.stage = 1;
        }
        return true;
    }
    if (marker_value(line, kCalendarNameMarker, value)) {
        if (pending.stage == 1 && !value.empty() && value.size() <= kMaxCalendarNameBytes) {
            pending.calendar.display_name.assign(value.data(), value.size());
            pending.stage = 2;
        } else {
            pending.reset();
        }
        return true;
    }
    if (marker_value(line, kCalendarColorMarker, value)) {
        std::uint32_t color = 0;
        if (pending.stage == 2 && parse_rgb888(value, color)) {
            pending.calendar.color_rgb888 = color;
            pending.stage = 3;
        } else {
            pending.reset();
        }
        return true;
    }
    return false;
}

std::string_view property_name(std::string_view line) {
    const std::size_t separator = line.find_first_of(";:");
    return line.substr(0, separator);
}

bool is_required_property(std::string_view name) {
    return ascii_iequals(name, "UID") || ascii_iequals(name, "SUMMARY") ||
           ascii_iequals(name, "DTSTART") || ascii_iequals(name, "DTEND");
}

void consume_property(std::string_view line, EventBuilder& builder) {
    const std::size_t colon = line.find(':');
    if (colon == std::string_view::npos) return;
    const std::string_view name_and_parameters = line.substr(0, colon);
    const std::string_view value = line.substr(colon + 1);
    const std::size_t semicolon = name_and_parameters.find(';');
    const std::string_view name = name_and_parameters.substr(0, semicolon);
    const std::string_view parameters = semicolon == std::string_view::npos
                                           ? std::string_view{}
                                           : name_and_parameters.substr(semicolon + 1);

    if (ascii_iequals(name, "UID")) {
        if (value.empty() || value.size() > kMaxIcalLogicalLineBytes) {
            builder.oversized_required_field = true;
        } else {
            builder.uid.assign(value.data(), value.size());
        }
    } else if (ascii_iequals(name, "SUMMARY")) {
        builder.summary = unescape_text(value);
        if (builder.summary.size() > kMaxTitleBytes) builder.oversized_required_field = true;
    } else if (ascii_iequals(name, "LOCATION")) {
        std::string location = unescape_text(value);
        if (!location.empty() && location.size() <= kMaxLocationBytes) {
            builder.location = std::move(location);
        }
    } else if (ascii_iequals(name, "DTSTART")) {
        builder.start = parse_timestamp(parameters, value);
    } else if (ascii_iequals(name, "DTEND")) {
        builder.end = parse_timestamp(parameters, value);
    } else if (ascii_iequals(name, "RRULE")) {
        builder.recurring_master = true;
    }
}

void consume_logical_line(std::string_view line, bool line_too_long, EventBuilder& builder,
                          const IcalFeedOptions& options, CalendarIdentity& active_calendar,
                          PendingCalendarIdentity& pending_calendar, bool& inside_vcalendar,
                          SeenEventIds& seen_event_ids,
                          IcalParseReport& report) {
    if (ascii_iequals(line, "BEGIN:VEVENT")) {
        if (builder.active) {
            builder.bad = true;
            finish_event(builder, options, active_calendar, seen_event_ids, report);
        }
        builder = {};
        builder.active = true;
        return;
    }
    if (ascii_iequals(line, "END:VEVENT")) {
        if (builder.active) {
            finish_event(builder, options, active_calendar, seen_event_ids, report);
        }
        return;
    }
    if (!builder.active && ascii_iequals(line, "BEGIN:VCALENDAR")) {
        if (!inside_vcalendar && pending_calendar.stage == 3 &&
            valid_calendar(pending_calendar.calendar)) {
            active_calendar = pending_calendar.calendar;
        }
        pending_calendar.reset();
        inside_vcalendar = true;
        return;
    }
    if (!builder.active && ascii_iequals(line, "END:VCALENDAR")) {
        pending_calendar.reset();
        inside_vcalendar = false;
        return;
    }
    if (!builder.active) {
        // Source identity is accepted only as one atomic ID/name/color triplet
        // directly before BEGIN:VCALENDAR. Calendar contents therefore cannot
        // spoof a source switch. Invalid, partial, interrupted or oversized
        // triplets leave the active identity untouched.
        if (!inside_vcalendar && !line_too_long &&
            consume_calendar_marker(line, pending_calendar)) {
            return;
        }
        pending_calendar.reset();
        return;
    }
    if (line_too_long) {
        const std::string_view name = property_name(line);
        if (is_required_property(name)) builder.oversized_required_field = true;
        if (ascii_iequals(name, "RRULE")) builder.recurring_master = true;
        return;
    }
    consume_property(line, builder);
}

}  // namespace

struct IcalParseSession::Impl {
    static constexpr std::size_t kPhysicalPrefixBytes = kMaxIcalLogicalLineBytes + 1;

    Impl(std::string_view supplied_feed, const IcalFeedOptions& supplied_options)
        : feed(supplied_feed), options(supplied_options), active_calendar(options.calendar) {
        logical_line.reserve(128);
        physical_line.reserve(128);
        if (options.summary_buckets &&
            options.summary_buckets->count <= kMaxIcalSummaryBuckets) {
            report.summary_count = options.summary_buckets->count;
        }
        if (feed.size() > kMaxIcalFeedBytes || !valid_calendar(options.calendar) ||
            !valid_selection_window(options.selection_window) ||
            !valid_summary_buckets(options.summary_buckets)) {
            report.input_too_large = feed.size() > kMaxIcalFeedBytes;
            complete = true;
            return;
        }
        report.events.reserve(kMaxSnapshotEvents);
        seen_event_ids.reserve(kMaxSnapshotEvents);
        if (feed.empty()) finish();
    }

    void consume_physical_line() {
        std::string_view physical(physical_line);
        if (!physical.empty() && physical.back() == '\r') physical.remove_suffix(1);
        const bool continuation = !physical.empty() &&
                                  (physical.front() == ' ' || physical.front() == '\t');

        if (!continuation && have_logical_line) flush_logical_line();

        const std::string_view part = continuation ? physical.substr(1) : physical;
        have_logical_line = true;
        if (!logical_line_too_long) {
            const std::size_t available = kMaxIcalLogicalLineBytes - logical_line.size();
            const std::size_t copied = std::min(available, part.size());
            logical_line.append(part.data(), copied);
            if (physical_line_too_long || copied != part.size()) {
                logical_line_too_long = true;
            }
        }

        physical_line.clear();
        physical_line_too_long = false;
    }

    void flush_logical_line() {
        consume_logical_line(logical_line, logical_line_too_long, builder, options,
                             active_calendar, pending_calendar, inside_vcalendar,
                             seen_event_ids, report);
        logical_line.clear();
        logical_line_too_long = false;
        have_logical_line = false;
    }

    void finish() {
        if (complete) return;
        if (!physical_line.empty() || physical_line_too_long) consume_physical_line();
        if (have_logical_line) flush_logical_line();
        if (builder.active) {
            builder.bad = true;
            finish_event(builder, options, active_calendar, seen_event_ids, report);
        }
        complete = true;
    }

    bool parse_step(std::size_t byte_budget) {
        if (complete) return true;
        std::size_t consumed = 0;
        while (cursor < feed.size() && consumed < byte_budget) {
            const char character = feed[cursor++];
            ++consumed;
            if (character == '\n') {
                consume_physical_line();
                continue;
            }
            if (physical_line.size() < kPhysicalPrefixBytes) {
                physical_line.push_back(character);
            } else {
                physical_line_too_long = true;
            }
        }
        if (cursor == feed.size()) finish();
        return complete;
    }

    std::string_view feed;
    IcalFeedOptions options;
    IcalParseReport report;
    SeenEventIds seen_event_ids;
    EventBuilder builder;
    CalendarIdentity active_calendar;
    PendingCalendarIdentity pending_calendar;
    std::string logical_line;
    std::string physical_line;
    std::size_t cursor = 0;
    bool logical_line_too_long = false;
    bool physical_line_too_long = false;
    bool have_logical_line = false;
    bool inside_vcalendar = false;
    bool complete = false;
    bool report_taken = false;
};

IcalParseSession::IcalParseSession(std::string_view feed, const IcalFeedOptions& options)
    : impl_(std::make_unique<Impl>(feed, options)) {}

IcalParseSession::~IcalParseSession() = default;
IcalParseSession::IcalParseSession(IcalParseSession&&) noexcept = default;
IcalParseSession& IcalParseSession::operator=(IcalParseSession&&) noexcept = default;

bool IcalParseSession::step(std::size_t byte_budget) {
    return !impl_ || impl_->parse_step(byte_budget);
}

bool IcalParseSession::done() const {
    return !impl_ || impl_->complete;
}

IcalParseReport IcalParseSession::take_report() {
    if (!impl_ || !impl_->complete || impl_->report_taken) return {};
    impl_->report_taken = true;
    return std::move(impl_->report);
}

IcalParseReport parse_ical_feed(std::string_view feed, const IcalFeedOptions& options) {
    IcalParseSession session(feed, options);
    while (!session.done()) session.step(kMaxIcalFeedBytes);
    return session.take_report();
}

}  // namespace calendar
