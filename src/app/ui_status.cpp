#include "app/ui_status.hpp"

namespace calendar {
namespace {

constexpr UiStatusPresentation kStarting{"Starting...", UiStatusSeverity::Neutral};
constexpr UiStatusPresentation kSetUpWifi{"Set up Wi-Fi", UiStatusSeverity::Attention};
constexpr UiStatusPresentation kSetupWifi{"Setup Wi-Fi", UiStatusSeverity::Attention};
constexpr UiStatusPresentation kConnecting{"Connecting...", UiStatusSeverity::Attention};
constexpr UiStatusPresentation kConnected{"Wi-Fi connected", UiStatusSeverity::Normal};
constexpr UiStatusPresentation kOffline{"Offline", UiStatusSeverity::Attention};

constexpr UiStatusPresentation kRefreshFailed{"Calendar refresh failed", UiStatusSeverity::Error};
constexpr UiStatusPresentation kImportFailed{"Calendar import failed", UiStatusSeverity::Error};
constexpr UiStatusPresentation kRefreshFailedCached{"Sync failed - saved events", UiStatusSeverity::Error};
constexpr UiStatusPresentation kImportFailedCached{"Import failed - saved events", UiStatusSeverity::Error};
constexpr UiStatusPresentation kCalendarStarting{"Calendar starting", UiStatusSeverity::Neutral};
constexpr UiStatusPresentation kDownloading{"Downloading calendar", UiStatusSeverity::Attention};
constexpr UiStatusPresentation kImporting{"Importing calendar", UiStatusSeverity::Attention};
constexpr UiStatusPresentation kWaitingForTime{"Waiting for time", UiStatusSeverity::Attention};
constexpr UiStatusPresentation kSynced{"Calendar synced", UiStatusSeverity::Normal};
constexpr UiStatusPresentation kCached{"Offline copy", UiStatusSeverity::Attention};
constexpr UiStatusPresentation kUnavailable{"Calendar unavailable", UiStatusSeverity::Error};
constexpr UiStatusPresentation kAddCalendar{"Add a calendar", UiStatusSeverity::Attention};

UiStatusPresentation derive_connection(const board::ConnectivityStatus& status) {
    if (!status.service_initialized) return kStarting;
    if (status.wifi_connected) return kConnected;
    if (status.setup_ap_active) return kSetupWifi;
    if (!status.wifi_credentials_saved) return kSetUpWifi;
    if (status.state == board::ConnectivityState::Error) return kOffline;
    return kConnecting;
}

UiStatusPresentation derive_freshness(const board::ConnectivityStatus& status,
                                      ProviderState provider_state,
                                      bool has_cached_calendar) {
    // A current transfer/import failure must remain visible even while the
    // retained provider snapshot is Ready.
    if (!status.service_initialized) return kCalendarStarting;
    if (status.fetch_state == board::CalendarFetchState::Failed) {
        return has_cached_calendar ? kRefreshFailedCached : kRefreshFailed;
    }
    if (status.import_result_known && !status.last_import_succeeded) {
        return has_cached_calendar ? kImportFailedCached : kImportFailed;
    }
    if (status.fetch_state == board::CalendarFetchState::Downloading) return kDownloading;
    if (status.fetch_state == board::CalendarFetchState::Succeeded &&
        !status.import_result_known) return kImporting;
    if (!status.time_synchronized && status.calendar_feed_saved) return kWaitingForTime;
    if (!status.calendar_feed_saved) return kAddCalendar;
    if (!status.wifi_connected && has_cached_calendar) return kCached;

    if (provider_state == ProviderState::Error) {
        return has_cached_calendar ? kCached : kUnavailable;
    }
    if (provider_state == ProviderState::Stale) return kCached;
    if (provider_state == ProviderState::Loading) return has_cached_calendar ? kCached : kImporting;

    if (status.import_result_known && status.last_import_succeeded &&
        status.last_successful_import_utc > 0) return kSynced;
    return has_cached_calendar ? kCached : kUnavailable;
}

}  // namespace

CalendarStatusPresentation derive_calendar_status(const board::ConnectivityStatus& connectivity,
                                                   ProviderState provider_state,
                                                   bool has_cached_calendar) {
    return {derive_connection(connectivity),
            derive_freshness(connectivity, provider_state, has_cached_calendar)};
}

}  // namespace calendar
