#pragma once

#include "app/calendar_period.hpp"
#include "app/calendar_provider.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace calendar {

class Clock {
public:
    virtual ~Clock() = default;
    virtual CivilDate today() const = 0;
};

class FixedClock final : public Clock {
public:
    explicit FixedClock(CivilDate date);
    CivilDate today() const override;

private:
    CivilDate date_;
};

enum class NavigationScreen { Agenda, EventDetails };

// UI-independent interaction state. A future LVGL controller may map this
// state onto EEZ widgets, but this class never creates or touches LVGL objects.
class NavigationState {
public:
    explicit NavigationState(const Clock& clock);

    CivilDate current_day() const;
    CivilDate selected_day() const;
    CivilDate visible_anchor_day() const;
    CalendarViewMode view_mode() const;
    CalendarPeriod visible_period() const;
    NavigationScreen screen() const;
    const std::optional<std::string>& selected_event_id() const;
    std::size_t agenda_scroll_position() const;
    ProviderState provider_state() const;

    void select_day(CivilDate day);
    // Updates the live local date without discarding a deliberately selected
    // different day. When the user was looking at "today", follow midnight.
    void set_current_day(CivilDate day);
    void previous_period();
    void next_period();
    void show_today();
    void show_week_view();
    void show_month_view();
    bool open_event(std::string_view event_id, const CalendarSnapshot& snapshot,
                    std::size_t agenda_scroll_position);
    void back();
    void apply_provider_result(const ProviderResult& result);

private:
    CivilDate current_day_;
    CivilDate selected_day_;
    CivilDate visible_anchor_day_;
    CalendarViewMode view_mode_ = CalendarViewMode::Week;
    NavigationScreen screen_ = NavigationScreen::Agenda;
    std::optional<std::string> selected_event_id_;
    std::size_t agenda_scroll_position_ = 0;
    ProviderState provider_state_ = ProviderState::Loading;

    void move_to(CivilDate day);
    void set_view_mode(CalendarViewMode mode);
};

}  // namespace calendar
