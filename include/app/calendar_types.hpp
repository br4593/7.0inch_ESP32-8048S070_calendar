#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if defined(ARDUINO)
#include <esp_heap_caps.h>
#endif

namespace calendar {

// Long-lived calendar values contain user-controlled text.  On the ESP32,
// allocate them in PSRAM so they do not contend with internal RAM used by
// LVGL and Wi-Fi. Host tests retain the standard allocator.
template <typename T>
class CalendarPsramAllocator {
public:
    using value_type = T;

    CalendarPsramAllocator() noexcept = default;
    template <typename U>
    CalendarPsramAllocator(const CalendarPsramAllocator<U>&) noexcept {}

    T* allocate(std::size_t count) {
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_array_new_length();
        }
#if defined(ARDUINO)
        void* memory = heap_caps_malloc(count * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (memory == nullptr) throw std::bad_alloc();
        return static_cast<T*>(memory);
#else
        return std::allocator<T>{}.allocate(count);
#endif
    }

    void deallocate(T* memory, std::size_t count) noexcept {
#if defined(ARDUINO)
        (void)count;
        heap_caps_free(memory);
#else
        std::allocator<T>{}.deallocate(memory, count);
#endif
    }

    template <typename U>
    struct rebind { using other = CalendarPsramAllocator<U>; };
};

template <typename T, typename U>
bool operator==(const CalendarPsramAllocator<T>&, const CalendarPsramAllocator<U>&) noexcept { return true; }

template <typename T, typename U>
bool operator!=(const CalendarPsramAllocator<T>&, const CalendarPsramAllocator<U>&) noexcept { return false; }

using CalendarString = std::basic_string<char, std::char_traits<char>, CalendarPsramAllocator<char>>;

constexpr std::size_t kMaxEventIdBytes = 40;
constexpr std::size_t kMaxTitleBytes = 160;
constexpr std::size_t kMaxLocationBytes = 160;
constexpr std::size_t kMaxCalendarIdBytes = 40;
constexpr std::size_t kMaxCalendarNameBytes = 80;

// A Gregorian civil date. It deliberately has no timezone attached.
struct CivilDate {
    int year = 1970;
    unsigned month = 1;
    unsigned day = 1;
};

bool operator==(CivilDate left, CivilDate right);
bool operator!=(CivilDate left, CivilDate right);
bool operator<(CivilDate left, CivilDate right);
bool operator<=(CivilDate left, CivilDate right);
bool operator>(CivilDate left, CivilDate right);
bool operator>=(CivilDate left, CivilDate right);

// Adds whole calendar days, including across leap years and month boundaries.
CivilDate add_days(CivilDate date, int days);
bool is_valid_date(CivilDate date);

struct CalendarIdentity {
    CalendarString id;
    CalendarString display_name;
    std::uint32_t color_rgb888 = 0;
};

enum class TimeKind { Timed, AllDay };

struct CalendarEvent {
    CalendarString id;
    CalendarString title;
    TimeKind time_kind = TimeKind::Timed;

    // Used only when time_kind is Timed. end_utc is exclusive.
    std::int64_t start_utc = 0;
    std::int64_t end_utc = 0;

    // Used only when time_kind is AllDay. end_date_exclusive is exclusive.
    CivilDate start_date;
    CivilDate end_date_exclusive;

    std::optional<CalendarString> location;
    CalendarIdentity calendar;
};

// Both the event objects and their dynamically allocated strings live in
// PSRAM on the ESP32. Host tests use the standard allocator implementation.
using CalendarEventList = std::vector<CalendarEvent, CalendarPsramAllocator<CalendarEvent>>;

// A selected date paired with the timezone conversion supplied by the caller.
// The interval is [start_utc, end_utc), and can be 23 or 25 hours at DST edges.
struct DayWindow {
    CivilDate date;
    std::int64_t start_utc = 0;
    std::int64_t end_utc = 0;
};

bool is_valid_event(const CalendarEvent& event);
bool is_valid_day_window(const DayWindow& window);
bool overlaps_day(const CalendarEvent& event, const DayWindow& window);

std::string display_title(const CalendarEvent& event);

}  // namespace calendar
