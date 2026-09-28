#pragma once

#include "app/calendar_types.hpp"

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

namespace calendar {

constexpr std::size_t kMaxSnapshotEvents = 256;
constexpr std::size_t kMaxAgendaRows = 20;

enum class ProviderState { Ready, Loading, Stale, Error };

// A read-only, bounded event collection. Construction rejects invalid events,
// duplicate IDs, and snapshots exceeding the bounded PSRAM limit.
class CalendarSnapshot {
public:
    explicit CalendarSnapshot(CalendarEventList events);

    const CalendarEventList& events() const;
    const CalendarEvent* find_by_id(std::string_view id) const;

private:
    CalendarEventList events_;
};

struct ProviderResult {
    ProviderState state = ProviderState::Loading;
    std::shared_ptr<const CalendarSnapshot> snapshot;
};

class CalendarProvider {
public:
    virtual ~CalendarProvider() = default;
    virtual ProviderResult current() const = 0;
};

// Replaceable fixture provider. It does no I/O, so callers can use it from
// deterministic unit tests and later swap it for a production provider.
class MockCalendarProvider final : public CalendarProvider {
public:
    MockCalendarProvider();
    explicit MockCalendarProvider(CalendarEventList events,
                                  ProviderState state = ProviderState::Ready);

    ProviderResult current() const override;
    void replace(CalendarEventList events, ProviderState state = ProviderState::Ready);
    void set_state(ProviderState state);

private:
    ProviderState state_;
    std::shared_ptr<const CalendarSnapshot> snapshot_;
};

CalendarEventList filter_and_sort_agenda(const CalendarSnapshot& snapshot,
                                         const DayWindow& day);

// Stable IDs and values match, independent of server ordering.
bool snapshot_matches_events(const CalendarSnapshot& snapshot,
                             const CalendarEventList& events);

// The canonical deterministic Milestone 1 data set. It includes Hebrew,
// English and mixed-direction text plus empty, crowded and edge-day cases.
CalendarEventList make_milestone_one_fixtures();

}  // namespace calendar
