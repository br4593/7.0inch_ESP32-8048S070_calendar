#include "app/ui_status.hpp"

#include <cstdlib>
#include <iostream>

namespace {

using calendar::CalendarStatusPresentation;
using calendar::ProviderState;
using calendar::UiStatusSeverity;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

CalendarStatusPresentation derive(board::ConnectivityStatus status,
                                  ProviderState provider = ProviderState::Ready,
                                  bool cached = false) {
    return calendar::derive_calendar_status(status, provider, cached);
}

void expect_labels(const CalendarStatusPresentation& value, const char* connection,
                   const char* freshness) {
    if (std::string_view(value.connection.label) != connection) {
        std::cerr << "FAILED: connection label: expected '" << connection
                  << "', got '" << value.connection.label << "'\n";
        std::exit(1);
    }
    if (std::string_view(value.freshness.label) != freshness) {
        std::cerr << "FAILED: freshness label: expected '" << freshness
                  << "', got '" << value.freshness.label << "'\n";
        std::exit(1);
    }
}

}  // namespace

int main() {
    board::ConnectivityStatus status{};
    expect_labels(derive(status), "Starting...", "Calendar starting");

    status.service_initialized = true;
    expect_labels(derive(status), "Set up Wi-Fi", "Add a calendar");

    status.setup_ap_active = true;
    expect_labels(derive(status), "Setup Wi-Fi", "Add a calendar");

    status.wifi_credentials_saved = true;
    status.setup_ap_active = false;
    expect_labels(derive(status), "Connecting...", "Add a calendar");

    status.state = board::ConnectivityState::Error;
    expect_labels(derive(status), "Offline", "Add a calendar");

    status.wifi_connected = true;
    status.setup_ap_active = true;
    expect_labels(derive(status), "Wi-Fi connected", "Add a calendar");

    status.calendar_feed_saved = true;
    status.time_synchronized = true;
    status.fetch_state = board::CalendarFetchState::Succeeded;
    status.import_result_known = true;
    status.last_import_succeeded = true;
    status.last_successful_import_utc = 1835308800;
    expect_labels(derive(status, ProviderState::Ready, true), "Wi-Fi connected", "Calendar synced");

    // A later transfer error takes precedence over the retained Ready snapshot.
    status.fetch_state = board::CalendarFetchState::Failed;
    expect_labels(derive(status, ProviderState::Ready, true), "Wi-Fi connected", "Sync failed - saved events");

    status.fetch_state = board::CalendarFetchState::Succeeded;
    status.import_result_known = true;
    status.last_import_succeeded = false;
    expect_labels(derive(status, ProviderState::Ready, true), "Wi-Fi connected", "Import failed - saved events");

    status.import_result_known = false;
    expect_labels(derive(status, ProviderState::Ready, true), "Wi-Fi connected", "Importing calendar");

    status.fetch_state = board::CalendarFetchState::WaitingForNetwork;
    status.time_synchronized = false;
    expect_labels(derive(status, ProviderState::Ready, true), "Wi-Fi connected", "Waiting for time");

    const CalendarStatusPresentation cached = derive(status, ProviderState::Stale, true);
    expect(std::string_view(cached.freshness.label) == "Waiting for time",
           "time synchronization is stated before a stale provider label");
    expect(cached.freshness.severity == UiStatusSeverity::Attention, "time severity");

    status.time_synchronized = true;
    status.wifi_connected = false;
    status.setup_ap_active = false;
    status.state = board::ConnectivityState::Error;
    expect_labels(derive(status, ProviderState::Ready, true), "Offline", "Offline copy");
    return 0;
}
