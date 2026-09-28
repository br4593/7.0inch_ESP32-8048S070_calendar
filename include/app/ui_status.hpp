#pragma once

#include "app/calendar_provider.hpp"
#include "board/connectivity_service.hpp"

namespace calendar {

// The controller maps service facts into these compact, UI-safe presentations.
// Keep this free of Arduino and LVGL so priority rules are host-testable.
enum class UiStatusSeverity { Neutral, Normal, Attention, Error };

struct UiStatusPresentation {
    const char* label;
    UiStatusSeverity severity;
};

struct CalendarStatusPresentation {
    UiStatusPresentation connection;
    UiStatusPresentation freshness;
};

// `has_cached_calendar` means that the provider currently has a valid snapshot
// or the controller has retained a previously accepted calendar document.
CalendarStatusPresentation derive_calendar_status(const board::ConnectivityStatus& connectivity,
                                                   ProviderState provider_state,
                                                   bool has_cached_calendar);

}  // namespace calendar
