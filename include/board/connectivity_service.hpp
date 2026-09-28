#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace board {

// This is deliberately a small board service rather than UI code.  It owns
// Wi-Fi credentials and private iCalendar addresses, but never exposes the
// latter through Status or Serial output.
enum class ConnectivityState {
    NotConfigured,
    SetupPortal,
    Connecting,
    SynchronizingTime,
    Ready,
    Error,
};

// This describes the HTTPS transfer only.  The calendar parser runs later on
// the UI/application context, so a successful transfer means a bounded iCal
// document was received, not that every event in it was understood.
enum class CalendarFetchState {
    NotRequested,
    WaitingForNetwork,
    Downloading,
    Succeeded,
    Failed,
};

// Memory-heavy network/flash operations are serialized. Calendar downloads
// remain owned by ConnectivityService; Weather and OTA borrow the same gate so
// two TLS stacks or a flash write cannot compete with the display runtime.
enum class NetworkActivity {
    Idle,
    WeatherDownload,
    OtaUpdate,
};

constexpr std::size_t kMaxWifiScanNetworks = 16;

// Safe, bounded scan metadata for the local display. This deliberately
// excludes credentials and any BSSID/device identifiers.
struct WifiScanNetwork {
    char ssid[33]{};
    std::int16_t rssi = 0;
    bool secured = false;
};

struct WifiScanResults {
    bool in_progress = false;
    bool completed = false;
    bool failed = false;
    std::uint32_t generation = 0;
    std::size_t count = 0;
    WifiScanNetwork networks[kMaxWifiScanNetworks]{};
};

enum class WifiScanRequestResult {
    Queued,
    AlreadyPending,
    ServiceStarting,
    CalendarBusy,
    WeatherBusy,
    OtaBusy,
};

struct ConnectivityStatus {
    // Remains false during the deliberate pre-service startup delay. This
    // separates normal startup from an initialized but unconfigured service.
    bool service_initialized = false;
    ConnectivityState state = ConnectivityState::NotConfigured;
    bool setup_ap_active = false;
    bool wifi_connected = false;
    bool time_synchronized = false;
    // credentials_saved remains the "ready to fetch" compatibility flag: it
    // is true only when both Wi-Fi and at least one private feed are saved.
    bool credentials_saved = false;
    bool wifi_credentials_saved = false;
    bool calendar_feed_saved = false;
    // Number of configured private feeds. Safe to expose because it contains
    // no calendar address or credential data.
    std::size_t configured_feed_count = 0;
    // Changes whenever the saved calendar slot configuration changes.
    std::uint32_t calendar_configuration_generation = 0;
    bool calendar_document_available = false;
    CalendarFetchState fetch_state = CalendarFetchState::NotRequested;
    std::size_t last_fetch_bytes = 0;
    bool import_result_known = false;
    bool last_import_succeeded = false;
    std::size_t last_imported_events = 0;
    std::size_t last_skipped_events = 0;
    // Last accepted download/import, retained across failed refreshes and
    // cleared when the feed configuration changes. No private data is exposed.
    std::int64_t last_successful_import_utc = 0;
    std::uint32_t successful_import_generation = 0;

    // These fields are intentionally safe to put on the Settings screen.
    // setup_ssid/password are only populated while the temporary setup AP is
    // running.  No stored home-network password or calendar URL is exposed.
    // station_address is the safe local URL for the second-stage iCal page.
    char summary[96]{};
    // A short, URL-free reason such as "HTTP 404" or "HTTPS read timeout".
    // This is safe for the local page and Settings screen.
    char fetch_detail[96]{};
    // Last station disconnect reason reported by the ESP32 Wi-Fi driver. This
    // contains only a reason code/name, never the SSID or password.
    char wifi_detail[96]{};
    char setup_ssid[33]{};
    char setup_password[16]{};
    char setup_address[24]{};
    char station_address[20]{};
};

// A bounded iCalendar transfer buffer. Its storage is allocated from PSRAM by
// the board service and transferred, without copying, to the UI context for
// parsing. It is intentionally not a general-purpose string.
class CalendarDocument {
public:
    CalendarDocument() = default;
    ~CalendarDocument();
    CalendarDocument(const CalendarDocument&) = delete;
    CalendarDocument& operator=(const CalendarDocument&) = delete;
    CalendarDocument(CalendarDocument&& other) noexcept;
    CalendarDocument& operator=(CalendarDocument&& other) noexcept;

    bool empty() const;
    std::size_t size() const;
    std::string_view view() const;
    std::uint32_t configuration_generation() const;

    // Board-service implementation detail, public only so its local Stream
    // sink can populate this move-only value.
    bool reserve(std::size_t capacity);
    bool append(const std::uint8_t* bytes, std::size_t count);
    // Rolls back a partially received feed before one bounded retry.
    bool truncate(std::size_t size);
    void set_configuration_generation(std::uint32_t generation);

private:
    void release();
    char* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
    std::uint32_t configuration_generation_ = 0;
};

class ConnectivityService {
public:
    ConnectivityService();
    ~ConnectivityService();

    ConnectivityService(const ConnectivityService&) = delete;
    ConnectivityService& operator=(const ConnectivityService&) = delete;

    // Call once after Arduino is initialized, then call tick() from the main
    // loop. Neither method makes LVGL calls.
    // Returns false only when required local startup resources could not be
    // created. Wi-Fi, NTP and remote feed availability are not startup gates.
    bool initialize();
    void tick();

    // Starts a WPA2-protected access point and first-stage local setup page at
    // the safe address reported in status(). That page scans/selects Wi-Fi
    // only; the private iCal address is entered later over the home LAN.
    void begin_setup();

    // Saves an SSID/password entered on the display, then starts the normal
    // asynchronous station connection. It never exposes the password through
    // status, the display, or serial output. Returns false for invalid input,
    // an unavailable service, or while a calendar transfer is in progress.
    bool save_display_wifi_credentials(const char* ssid, const char* password);

    // Starts one asynchronous nearby-network scan for the display setup form.
    // It performs no waiting; call wifi_scan_results() later to observe the
    // bounded safe results. Network work takes precedence, and repeated taps
    // coalesce into the already queued/running scan.
    WifiScanRequestResult request_wifi_scan();
    WifiScanResults wifi_scan_results() const;

    // Starts one background, certificate-validated synchronization when
    // Wi-Fi, NTP, and at least one saved feed are available. Configured feeds
    // are downloaded sequentially and published as one bounded document only
    // when every download succeeds.
    bool request_sync();

    // Board services that perform their own background work use this shared
    // gate. A successful begin must be paired with end_network_activity().
    // Wi-Fi scanning and calendar downloads are included in the busy check.
    bool try_begin_network_activity(NetworkActivity activity);
    void end_network_activity(NetworkActivity activity);
    NetworkActivity network_activity() const;

    // Called after the application parses a document returned by
    // take_downloaded_ical(). This lets the local web page distinguish a
    // successful HTTPS transfer from a feed that could not be imported. A
    // valid calendar containing zero events is successful.
    void report_ical_import_result(std::uint32_t configuration_generation, bool success,
                                   std::size_t imported_events, std::size_t skipped_events);

    ConnectivityStatus status() const;

    // Moves the latest successful, bounded PSRAM-backed iCalendar document to
    // the caller.
    // Intended to be consumed on the normal UI/application context and parsed
    // there; it returns false when no new download is waiting.
    bool take_downloaded_ical(CalendarDocument& document);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// The firmware can use this single long-lived instance without exposing any
// Arduino headers to UI/application code.
ConnectivityService& connectivity_service();

}  // namespace board
