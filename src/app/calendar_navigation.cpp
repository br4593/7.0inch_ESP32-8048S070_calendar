#include "app/calendar_navigation.hpp"

#include <stdexcept>

namespace calendar {

FixedClock::FixedClock(CivilDate date) : date_(date) {
    if (!is_valid_date(date_)) throw std::invalid_argument("fixed clock needs a valid date");
}

CivilDate FixedClock::today() const { return date_; }

NavigationState::NavigationState(const Clock& clock)
    : current_day_(clock.today()), selected_day_(current_day_), visible_anchor_day_(current_day_) {}

CivilDate NavigationState::current_day() const { return current_day_; }
CivilDate NavigationState::selected_day() const { return selected_day_; }
CivilDate NavigationState::visible_anchor_day() const { return visible_anchor_day_; }
CalendarViewMode NavigationState::view_mode() const { return view_mode_; }
CalendarPeriod NavigationState::visible_period() const {
    return view_mode_ == CalendarViewMode::Week ? week_period(visible_anchor_day_)
                                                : month_grid_period(visible_anchor_day_);
}
NavigationScreen NavigationState::screen() const { return screen_; }
const std::optional<std::string>& NavigationState::selected_event_id() const { return selected_event_id_; }
std::size_t NavigationState::agenda_scroll_position() const { return agenda_scroll_position_; }
ProviderState NavigationState::provider_state() const { return provider_state_; }

void NavigationState::select_day(CivilDate day) {
    if (!is_valid_date(day)) throw std::invalid_argument("selected day must be valid");
    move_to(day);
}

void NavigationState::move_to(CivilDate day) {
    selected_day_ = day;
    visible_anchor_day_ = day;
    selected_event_id_.reset();
    screen_ = NavigationScreen::Agenda;
    agenda_scroll_position_ = 0;
}

void NavigationState::set_current_day(CivilDate day) {
    if (!is_valid_date(day)) throw std::invalid_argument("current day must be valid");
    const bool following_today = selected_day_ == current_day_;
    const bool visible_follows_today = visible_anchor_day_ == current_day_;
    current_day_ = day;
    if (following_today) selected_day_ = day;
    if (visible_follows_today) visible_anchor_day_ = day;
}

void NavigationState::previous_period() {
    move_to(view_mode_ == CalendarViewMode::Week ? add_days(selected_day_, -7)
                                                 : add_months_clamped(selected_day_, -1));
}

void NavigationState::next_period() {
    move_to(view_mode_ == CalendarViewMode::Week ? add_days(selected_day_, 7)
                                                 : add_months_clamped(selected_day_, 1));
}

void NavigationState::show_today() { move_to(current_day_); }

void NavigationState::set_view_mode(CalendarViewMode mode) {
    view_mode_ = mode;
    visible_anchor_day_ = selected_day_;
    selected_event_id_.reset();
    screen_ = NavigationScreen::Agenda;
    agenda_scroll_position_ = 0;
}

void NavigationState::show_week_view() { set_view_mode(CalendarViewMode::Week); }

void NavigationState::show_month_view() { set_view_mode(CalendarViewMode::Month); }

bool NavigationState::open_event(std::string_view event_id, const CalendarSnapshot& snapshot,
                                 std::size_t agenda_scroll_position) {
    if (event_id.empty() || snapshot.find_by_id(event_id) == nullptr) return false;
    selected_event_id_ = std::string(event_id);
    agenda_scroll_position_ = agenda_scroll_position;
    screen_ = NavigationScreen::EventDetails;
    return true;
}

void NavigationState::back() {
    selected_event_id_.reset();
    screen_ = NavigationScreen::Agenda;
}

void NavigationState::apply_provider_result(const ProviderResult& result) {
    provider_state_ = result.state;
    if (screen_ == NavigationScreen::EventDetails &&
        (!result.snapshot || !selected_event_id_ || result.snapshot->find_by_id(*selected_event_id_) == nullptr)) {
        back();
    }
}

}  // namespace calendar
