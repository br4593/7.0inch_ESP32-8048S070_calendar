#include "board/connectivity_service.hpp"
#include "board/weather_service.hpp"
#include "calendar_limits.hpp"

#include <Arduino.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <esp_heap_caps.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <utility>

namespace board {

CalendarDocument::~CalendarDocument() { release(); }

CalendarDocument::CalendarDocument(CalendarDocument&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_),
      configuration_generation_(other.configuration_generation_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
    other.configuration_generation_ = 0;
}

CalendarDocument& CalendarDocument::operator=(CalendarDocument&& other) noexcept {
    if (this == &other) return *this;
    release();
    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;
    configuration_generation_ = other.configuration_generation_;
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
    other.configuration_generation_ = 0;
    return *this;
}

bool CalendarDocument::empty() const { return size_ == 0; }
std::size_t CalendarDocument::size() const { return size_; }
std::string_view CalendarDocument::view() const { return {data_, size_}; }
std::uint32_t CalendarDocument::configuration_generation() const {
    return configuration_generation_;
}
void CalendarDocument::set_configuration_generation(std::uint32_t generation) {
    configuration_generation_ = generation;
}

bool CalendarDocument::reserve(std::size_t capacity) {
    if (capacity <= capacity_) return true;
    char* replacement = static_cast<char*>(
        heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (replacement == nullptr) return false;
    if (data_ != nullptr && size_ != 0) std::memcpy(replacement, data_, size_);
    if (data_ != nullptr) heap_caps_free(data_);
    data_ = replacement;
    capacity_ = capacity;
    return true;
}

bool CalendarDocument::append(const std::uint8_t* bytes, std::size_t count) {
    if (bytes == nullptr || count == 0) return count == 0;
    if (data_ == nullptr || count > capacity_ - size_) return false;
    std::memcpy(data_ + size_, bytes, count);
    size_ += count;
    return true;
}

bool CalendarDocument::truncate(std::size_t size) {
    if (size > size_) return false;
    size_ = size;
    return true;
}

void CalendarDocument::release() {
    if (data_ != nullptr) heap_caps_free(data_);
    data_ = nullptr;
    size_ = 0;
    capacity_ = 0;
    configuration_generation_ = 0;
}

namespace {

constexpr char kPreferencesNamespace[] = "calendar-net";
constexpr char kTzJerusalem[] = "IST-2IDT,M3.4.4/26,M10.5.0";
constexpr char kNtpPrimary[] = "time.google.com";
constexpr char kNtpSecondary[] = "pool.ntp.org";
constexpr std::size_t kMaxSsidBytes = 32;
constexpr std::size_t kMaxPasswordBytes = 63;
constexpr std::size_t kMaxFeedUrlBytes = 512;
constexpr std::size_t kMaxCalendarFeeds = 4;
constexpr std::size_t kMaxCalendarLabelBytes = 48;
constexpr std::uint32_t kReconnectIntervalMs = 15U * 1000U;
constexpr std::uint8_t kMaxStationConnectAttemptsBeforePortal = 3;
constexpr std::uint32_t kCalendarRefreshIntervalMs = 5U * 60U * 1000U;
constexpr std::uint32_t kWifiScanRetryDelayMs = 1500U;
constexpr std::uint32_t kWifiScanRadioSettleMs = 500U;
constexpr std::uint8_t kMaxWifiScanStartAttempts = 4;
constexpr std::uint8_t kCalendarBodyAttempts = 2;
constexpr std::uint32_t kCalendarBodyRetryDelayMs = 250U;
constexpr std::time_t kPlausibleUnixTime = 1700000000;

// Google Calendar's private iCal URLs are HTTPS endpoints currently served
// from the Google Trust Services chain. GTS Root R1 expires in 2036. Keeping a
// root CA here validates the server without relying on setInsecure().
constexpr char kGoogleTrustServicesRootR1[] = R"pem(-----BEGIN CERTIFICATE-----
MIIFVzCCAz+gAwIBAgINAgPlk28xsBNJiGuiFzANBgkqhkiG9w0BAQwFADBHMQsw
CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU
MBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAw
MDAwWjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZp
Y2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwggIiMA0GCSqGSIb3DQEBAQUA
A4ICDwAwggIKAoICAQC2EQKLHuOhd5s73L+UPreVp0A8of2C+X0yBoJx9vaMf/vo
27xqLpeXo4xL+Sv2sfnOhB2x+cWX3u+58qPpvBKJXqeqUqv4IyfLpLGcY9vXmX7w
Cl7raKb0xlpHDU0QM+NOsROjyBhsS+z8CZDfnWQpJSMHobTSPS5g4M/SCYe7zUjw
TcLCeoiKu7rPWRnWr4+wB7CeMfGCwcDfLqZtbBkOtdh+JhpFAz2weaSUKK0Pfybl
qAj+lug8aJRT7oM6iCsVlgmy4HqMLnXWnOunVmSPlk9orj2XwoSPwLxAwAtcvfaH
szVsrBhQf4TgTM2S0yDpM7xSma8ytSmzJSq0SPly4cpk9+aCEI3oncKKiPo4Zor8
Y/kB+Xj9e1x3+naH+uzfsQ55lVe0vSbv1gHR6xYKu44LtcXFilWr06zqkUspzBmk
MiVOKvFlRNACzqrOSbTqn3yDsEB750Orp2yjj32JgfpMpf/VjsPOS+C12LOORc92
wO1AK/1TD7Cn1TsNsYqiA94xrcx36m97PtbfkSIS5r762DL8EGMUUXLeXdYWk70p
aDPvOmbsB4om3xPXV2V4J95eSRQAogB/mqghtqmxlbCluQ0WEdrHbEg8QOB+DVrN
VjzRlwW5y0vtOUucxD/SVRNuJLDWcfr0wbrM7Rv1/oFB2ACYPTrIrnqYNxgFlQID
AQABo0IwQDAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4E
FgQU5K8rJnEaK0gnhS9SZizv8IkTcT4wDQYJKoZIhvcNAQEMBQADggIBAJ+qQibb
C5u+/x6Wki4+omVKapi6Ist9wTrYggoGxval3sBOh2Z5ofmmWJyq+bXmYOfg6LEe
QkEzCzc9zolwFcq1JKjPa7XSQCGYzyI0zzvFIoTgxQ6KfF2I5DUkzps+GlQebtuy
h6f88/qBVRRiClmpIgUxPoLW7ttXNLwzldMXG+gnoot7TiYaelpkttGsN/H9oPM4
7HLwEXWdyzRSjeZ2axfG34arJ45JK3VmgRAhpuo+9K4l/3wV3s6MJT/KYnAK9y8J
ZgfIPxz88NtFMN9iiMG1D53Dn0reWVlHxYciNuaCp+0KueIHoI17eko8cdLiA6Ef
MgfdG+RCzgwARWGAtQsgWSl4vflVy2PFPEz0tv/bal8xa5meLMFrUKTX5hgUvYU/
Z6tGn6D/Qqc6f1zLXbBwHSs09dR2CQzreExZBfMzQsNhFRAbd03OIozUhfJFfbdT
6u9AWpQKXCBfTkBdYiJ23//OYb2MI3jSNwLgjt7RETeJ9r/tSQdirpLsQBqvFAnZ
0E6yove+7u7Y/9waLd64NnHi/Hm3lCXRSHNboTXns5lndcEZOitHTtNCjv0xyBZm
2tIMPNuzjsmhDYAPexZ3FL//2wmUspO8IFgV6dtxQ/PeEMMA3KgqlbbC1j+Qa3bb
bP6MvPJwNQzcmRk13NfIRmPVNnGuV/u3gm3c
-----END CERTIFICATE-----
)pem";

template <std::size_t Size>
void copy_text(char (&destination)[Size], const char* source) {
    if (source == nullptr) {
        destination[0] = '\0';
        return;
    }
    std::snprintf(destination, Size, "%s", source);
}

bool starts_with_https(const String& value) {
    return value.startsWith("https://") && value.length() < kMaxFeedUrlBytes;
}

bool has_control_character(const String& value) {
    for (std::size_t index = 0; index < value.length(); ++index) {
        if (static_cast<unsigned char>(value[index]) < 0x20U) {
            return true;
        }
    }
    return false;
}

bool valid_calendar_label(const String& value) {
    return value.length() > 0 && value.length() <= kMaxCalendarLabelBytes &&
           !has_control_character(value);
}

constexpr const char* kCalendarColors[kMaxCalendarFeeds] = {
    "4285F4", "EA4335", "FBBC04", "34A853",
};

void append_html_escaped(String& destination, const char* source) {
    if (source == nullptr) return;
    for (const char* character = source; *character != '\0'; ++character) {
        switch (*character) {
            case '&': destination += "&amp;"; break;
            case '<': destination += "&lt;"; break;
            case '>': destination += "&gt;"; break;
            case '\"': destination += "&quot;"; break;
            case '\'': destination += "&#39;"; break;
            default: destination += *character; break;
        }
    }
}

void append_json_escaped(String& destination, const char* source) {
    if (source == nullptr) return;
    for (const char* character = source; *character != '\0'; ++character) {
        switch (*character) {
            case '\\': destination += "\\\\"; break;
            case '"': destination += "\\\""; break;
            case '\n': destination += "\\n"; break;
            case '\r': destination += "\\r"; break;
            case '\t': destination += "\\t"; break;
            default: destination += *character; break;
        }
    }
}

// HTTPClient::writeToStream handles both Content-Length and chunked responses.
// This sink enforces the project feed cap without exposing calendar text.
class BoundedDocumentSink final : public Stream {
public:
    BoundedDocumentSink(CalendarDocument& document, std::size_t limit)
        : document_(document), limit_(limit) {}

    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}

    size_t write(uint8_t byte) override { return write(&byte, 1); }

    size_t write(const uint8_t* bytes, size_t count) override {
        if (bytes == nullptr || count == 0) return 0;
        if (document_.size() > limit_ || count > limit_ - document_.size() ||
            !document_.append(bytes, count)) {
            overflowed_ = true;
            return 0;
        }
        return count;
    }

    bool overflowed() const { return overflowed_; }

private:
    CalendarDocument& document_;
    std::size_t limit_;
    bool overflowed_ = false;
};

const char* fetch_state_text(CalendarFetchState state) {
    switch (state) {
        case CalendarFetchState::NotRequested: return "No calendar download requested yet";
        case CalendarFetchState::WaitingForNetwork: return "Waiting for Wi-Fi and network time";
        case CalendarFetchState::Downloading: return "Downloading calendar";
        case CalendarFetchState::Succeeded: return "Calendar download succeeded";
        case CalendarFetchState::Failed: return "Calendar download failed";
    }
    return "Calendar download status unavailable";
}

bool retryable_calendar_body_error(int error) {
    // In this pinned HTTPClient, STREAM_WRITE also means that a response with
    // Content-Length closed before all declared bytes arrived. The bounded
    // sink's own overflow flag is checked first, so retrying this code cannot
    // hide a size-cap failure.
    return error == HTTPC_ERROR_STREAM_WRITE || error == HTTPC_ERROR_CONNECTION_LOST ||
           error == HTTPC_ERROR_READ_TIMEOUT;
}

}  // namespace

struct ConnectivityService::Impl {
    struct CalendarFeed {
        char url[kMaxFeedUrlBytes]{};
        char label[kMaxCalendarLabelBytes + 1]{};

        bool configured() const { return url[0] != '\0'; }
    };

    struct SavedConfiguration {
        char ssid[kMaxSsidBytes + 1]{};
        char password[kMaxPasswordBytes + 1]{};
        CalendarFeed feeds[kMaxCalendarFeeds]{};

        bool wifi_valid() const { return ssid[0] != '\0'; }
        std::size_t feed_count() const {
            std::size_t count = 0;
            for (const CalendarFeed& feed : feeds) {
                if (feed.configured()) ++count;
            }
            return count;
        }
        bool valid() const { return wifi_valid() && feed_count() != 0; }
    };

    Preferences preferences;
    WebServer server{80};
    SemaphoreHandle_t lock = nullptr;
    ConnectivityStatus status{};
    SavedConfiguration configuration{};
    CalendarDocument downloaded_ical;
    bool initialized = false;
    bool startup_attempted = false;
    bool startup_succeeded = false;
    bool portal_running = false;
    bool fallback_portal_running = false;
    bool lan_server_running = false;
    bool routes_configured = false;
    bool scan_in_progress = false;
    bool scan_requested = false;
    bool scan_radio_prepared = false;
    bool scan_completed = false;
    bool scan_failed = false;
    bool connect_after_response = false;
    bool sync_after_response = false;
    bool ntp_started = false;
    bool worker_running = false;
    NetworkActivity external_activity = NetworkActivity::Idle;
    std::uint32_t last_connect_attempt_ms = 0;
    std::uint8_t station_connect_attempts = 0;
    std::uint32_t next_scan_start_ms = 0;
    std::uint32_t scan_generation = 0;
    std::uint8_t scan_start_attempts = 0;
    std::uint32_t next_calendar_refresh_ms = 0;
    std::uint32_t calendar_configuration_generation = 1;
    WiFiEventId_t wifi_disconnect_event_id = 0;
    bool wifi_disconnect_event_registered = false;
    std::atomic<bool> pending_wifi_disconnect{false};
    std::atomic<std::uint8_t> pending_wifi_disconnect_reason{0};
    std::size_t scanned_network_count = 0;
    WifiScanNetwork scanned_networks[kMaxWifiScanNetworks]{};

    Impl() = default;

    ~Impl() {
        if (wifi_disconnect_event_registered) {
            WiFi.removeEvent(wifi_disconnect_event_id);
        }
        if (lock != nullptr) {
            vSemaphoreDelete(lock);
        }
    }

    void set_summary(const char* summary) {
        copy_text(status.summary, summary);
    }

    void set_fetch_detail(const char* detail) {
        copy_text(status.fetch_detail, detail);
    }

    void note_wifi_disconnect(std::uint8_t reason) {
        // Arduino dispatches Wi-Fi callbacks from its event task. Store only
        // byte-sized values here; the normal service context formats status.
        pending_wifi_disconnect_reason.store(reason, std::memory_order_relaxed);
        pending_wifi_disconnect.store(true, std::memory_order_release);
    }

    void consume_wifi_disconnect() {
        if (!pending_wifi_disconnect.exchange(false, std::memory_order_acquire)) return;
        const std::uint8_t reason =
            pending_wifi_disconnect_reason.load(std::memory_order_relaxed);
        const char* name = WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(reason));
        if (name != nullptr && name[0] != '\0') {
            std::snprintf(status.wifi_detail, sizeof(status.wifi_detail),
                          "Wi-Fi disconnect %u (%s)", static_cast<unsigned>(reason), name);
        } else {
            std::snprintf(status.wifi_detail, sizeof(status.wifi_detail),
                          "Wi-Fi disconnect reason %u", static_cast<unsigned>(reason));
        }
    }

    bool load_configuration() {
        // Open read-write so a factory-new device creates the application-owned
        // namespace. The ESP-IDF Wi-Fi store is deliberately not the credential
        // authority for this application.
        if (!preferences.begin(kPreferencesNamespace, false)) return false;
        if (preferences.isKey("ssid")) {
            preferences.getString("ssid", configuration.ssid, sizeof(configuration.ssid));
        }
        if (preferences.isKey("password")) {
            preferences.getString("password", configuration.password,
                                  sizeof(configuration.password));
        }
        const bool multi_feed_migration_complete = preferences.getBool("feeds_v2", false);
        for (std::size_t index = 0; index < kMaxCalendarFeeds; ++index) {
            char url_key[12]{};
            char label_key[12]{};
            std::snprintf(url_key, sizeof(url_key), "cal%u_url", static_cast<unsigned>(index));
            std::snprintf(label_key, sizeof(label_key), "cal%u_name", static_cast<unsigned>(index));
            if (preferences.isKey(url_key)) {
                preferences.getString(url_key, configuration.feeds[index].url,
                                      sizeof(configuration.feeds[index].url));
            }
            if (preferences.isKey(label_key)) {
                preferences.getString(label_key, configuration.feeds[index].label,
                                      sizeof(configuration.feeds[index].label));
            }
        }

        // One-time, privacy-preserving migration from the original single-feed
        // key. The URL is copied directly into slot zero and is never rendered,
        // logged, or returned by the service.
        bool migrated_legacy_feed = false;
        const bool legacy_feed_exists = preferences.isKey("ical_url");
        if (!multi_feed_migration_complete && !configuration.feeds[0].configured() &&
            legacy_feed_exists) {
            preferences.getString("ical_url", configuration.feeds[0].url,
                                  sizeof(configuration.feeds[0].url));
            if (configuration.feeds[0].configured()) {
                copy_text(configuration.feeds[0].label, "Calendar 1");
                migrated_legacy_feed = true;
            }
        }
        preferences.end();

        if (!multi_feed_migration_complete) {
            if (preferences.begin(kPreferencesNamespace, false)) {
                bool migration_saved = true;
                if (migrated_legacy_feed) {
                    preferences.putString("cal0_url", configuration.feeds[0].url);
                    preferences.putString("cal0_name", configuration.feeds[0].label);
                    migration_saved = preferences.isKey("cal0_url") && preferences.isKey("cal0_name") &&
                                      preferences.getString("cal0_url").length() ==
                                          std::strlen(configuration.feeds[0].url);
                }
                if (migration_saved) {
                    // The completion marker prevents a later intentional removal
                    // of slot zero from resurrecting the legacy address.
                    preferences.putBool("feeds_v2", true);
                    preferences.remove("ical_url");
                }
                preferences.end();
            }
        }

        for (std::size_t index = 0; index < kMaxCalendarFeeds; ++index) {
            CalendarFeed& feed = configuration.feeds[index];
            if (feed.configured() &&
                (!starts_with_https(String(feed.url)) || has_control_character(String(feed.url)))) {
                feed = {};
                continue;
            }
            if (feed.configured() && !valid_calendar_label(String(feed.label))) {
                std::snprintf(configuration.feeds[index].label,
                              sizeof(configuration.feeds[index].label), "Calendar %u",
                              static_cast<unsigned>(index + 1));
            }
        }
        update_saved_status();
        return true;
    }

    void update_saved_status() {
        status.wifi_credentials_saved = configuration.wifi_valid();
        status.configured_feed_count = configuration.feed_count();
        status.calendar_configuration_generation = calendar_configuration_generation;
        status.calendar_feed_saved = status.configured_feed_count != 0;
        status.credentials_saved = configuration.valid();
    }

    bool save_wifi_configuration(const String& ssid, const String& password) {
        if (ssid.length() == 0 || ssid.length() > kMaxSsidBytes ||
            password.length() > kMaxPasswordBytes || has_control_character(ssid) ||
            has_control_character(password)) {
            return false;
        }

        if (!preferences.begin(kPreferencesNamespace, false)) return false;
        // Preferences reports zero bytes for a successfully saved empty
        // string, which is a valid password for an intentionally open home
        // network. Check the keys after writing instead of rejecting it.
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        const bool saved = preferences.isKey("ssid") && preferences.isKey("password") &&
                           preferences.getString("ssid") == ssid &&
                           preferences.getString("password") == password;
        preferences.end();
        if (saved) {
            copy_text(configuration.ssid, ssid.c_str());
            copy_text(configuration.password, password.c_str());
            update_saved_status();
        }
        return saved;
    }

    bool save_calendar_feeds(const CalendarFeed (&feeds)[kMaxCalendarFeeds]) {
        std::size_t configured_count = 0;
        for (const CalendarFeed& feed : feeds) {
            if (!feed.configured()) continue;
            const String url(feed.url);
            const String label(feed.label);
            if (!starts_with_https(url) || has_control_character(url) ||
                !valid_calendar_label(label)) {
                return false;
            }
            ++configured_count;
        }
        if (configured_count == 0) return false;
        for (std::size_t left = 0; left < kMaxCalendarFeeds; ++left) {
            if (!feeds[left].configured()) continue;
            for (std::size_t right = left + 1; right < kMaxCalendarFeeds; ++right) {
                if (feeds[right].configured() &&
                    std::strcmp(feeds[left].url, feeds[right].url) == 0) {
                    return false;
                }
            }
        }

        preferences.begin(kPreferencesNamespace, false);
        bool saved = true;
        for (std::size_t index = 0; index < kMaxCalendarFeeds; ++index) {
            char url_key[12]{};
            char label_key[12]{};
            std::snprintf(url_key, sizeof(url_key), "cal%u_url", static_cast<unsigned>(index));
            std::snprintf(label_key, sizeof(label_key), "cal%u_name", static_cast<unsigned>(index));
            if (feeds[index].configured()) {
                preferences.putString(url_key, feeds[index].url);
                preferences.putString(label_key, feeds[index].label);
                saved = saved && preferences.isKey(url_key) && preferences.isKey(label_key) &&
                        preferences.getString(url_key).length() == std::strlen(feeds[index].url) &&
                        preferences.getString(label_key).length() == std::strlen(feeds[index].label);
            } else {
                preferences.remove(url_key);
                preferences.remove(label_key);
                saved = saved && !preferences.isKey(url_key) && !preferences.isKey(label_key);
            }
        }
        preferences.end();
        if (saved) {
            for (std::size_t index = 0; index < kMaxCalendarFeeds; ++index) {
                configuration.feeds[index] = feeds[index];
            }
            ++calendar_configuration_generation;
            status.last_successful_import_utc = 0;
            status.successful_import_generation = 0;
            update_saved_status();
        }
        return saved;
    }

    void prepare_setup_credentials() {
        const std::uint64_t mac = ESP.getEfuseMac();
        std::snprintf(status.setup_ssid, sizeof(status.setup_ssid), "Calendar-Setup-%04llX",
                      static_cast<unsigned long long>(mac & 0xFFFFULL));
        std::snprintf(status.setup_password, sizeof(status.setup_password), "CAL%06llX",
                      static_cast<unsigned long long>(mac & 0xFFFFFFULL));
        copy_text(status.setup_address, "192.168.4.1");
    }

    void add_private_headers(bool allow_map_referrer = false) {
        server.sendHeader("Cache-Control", "no-store, max-age=0");
        server.sendHeader("Referrer-Policy", allow_map_referrer ? "origin" : "no-referrer");
        server.sendHeader("X-Content-Type-Options", "nosniff");
    }

    void append_page_head(String& page, const char* title, bool include_map = false) const {
        page += "<!doctype html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
        page += include_map
            ? "<meta name=\"referrer\" content=\"origin\">"
            : "<meta name=\"referrer\" content=\"no-referrer\">";
        if (include_map) {
            page += "<link rel=\"stylesheet\" href=\"https://unpkg.com/leaflet@1.9.4/dist/leaflet.css\" integrity=\"sha256-p4NxAoJBhIIN+hmNHrzRCf9tD/miZyoHS5obTRR9BMY=\" crossorigin=\"\">";
        }
        page += "<title>";
        append_html_escaped(page, title);
        page += "</title><style>body{font-family:system-ui,sans-serif;max-width:38rem;margin:2rem auto;padding:0 1rem;line-height:1.45}input,button,select{box-sizing:border-box;font:inherit;padding:.7rem}input,select{width:100%;margin:.35rem 0 1rem}button{margin:.5rem 0;background:#1769aa;color:#fff;border:0;border-radius:.25rem}small{color:#555}.card{border:1px solid #ccc;border-radius:.5rem;padding:1rem;margin:1rem 0}.ok{color:#087830}.bad{color:#b00020}#weather-map{height:20rem;margin:.5rem 0 1rem;border:1px solid #999;border-radius:.5rem;background:#eee}.map-hidden{display:none}.map-help{display:block;margin:.1rem 0 1rem}</style></head><body>";
    }

    void request_wifi_scan() {
        if (scan_requested || scan_in_progress) return;
        scanned_network_count = 0;
        scan_requested = true;
        scan_radio_prepared = false;
        scan_completed = false;
        scan_failed = false;
        scan_start_attempts = 0;
        next_scan_start_ms = millis();
        ++scan_generation;
    }

    void retry_or_finish_failed_wifi_scan() {
        scan_in_progress = false;
        if (scan_start_attempts >= kMaxWifiScanStartAttempts) {
            scan_requested = false;
            scan_completed = true;
            scan_failed = true;
            if (scan_radio_prepared && configuration.wifi_valid()) {
                WiFi.setAutoReconnect(true);
                last_connect_attempt_ms = millis() - kReconnectIntervalMs;
            }
            scan_radio_prepared = false;
            ++scan_generation;
            return;
        }
        scan_requested = true;
        next_scan_start_ms = millis() + kWifiScanRetryDelayMs;
    }

    void start_requested_wifi_scan() {
        if (!scan_requested || scan_in_progress ||
            static_cast<std::int32_t>(millis() - next_scan_start_ms) < 0) {
            return;
        }
        if (!scan_radio_prepared) {
            if (configuration.wifi_valid()) WiFi.setAutoReconnect(false);
            scan_radio_prepared = true;
            if (configuration.wifi_valid() && WiFi.status() != WL_CONNECTED) {
                // ESP-IDF rejects a scan while STA association is in progress.
                // Disconnect only the STA side; WIFI_AP_STA mode and the
                // protected fallback AP remain active.
                WiFi.disconnect(false, false);
                next_scan_start_ms = millis() + kWifiScanRadioSettleMs;
                return;
            }
        }
        if (scan_start_attempts < UINT8_MAX) ++scan_start_attempts;
        const int started = WiFi.scanNetworks(true, true);
        if (started == WIFI_SCAN_RUNNING) {
            scan_requested = false;
            scan_in_progress = true;
            return;
        }
        // esp_wifi_scan_start can be briefly busy while AP/STA mode changes.
        // Retry from later board ticks instead of blocking the LVGL loop.
        retry_or_finish_failed_wifi_scan();
    }

    void refresh_scan_results() {
        if (!scan_in_progress) return;
        const int complete = WiFi.scanComplete();
        if (complete >= 0) {
            scanned_network_count = 0;
            const int limit = std::min(complete, static_cast<int>(kMaxWifiScanNetworks));
            for (int index = 0; index < limit; ++index) {
                const String ssid = WiFi.SSID(index);
                if (ssid.length() == 0 || ssid.length() > kMaxSsidBytes || has_control_character(ssid)) continue;
                WifiScanNetwork& result = scanned_networks[scanned_network_count++];
                copy_text(result.ssid, ssid.c_str());
                result.rssi = static_cast<std::int16_t>(WiFi.RSSI(index));
                result.secured = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
            }
            WiFi.scanDelete();
            scan_requested = false;
            scan_in_progress = false;
            scan_completed = true;
            scan_failed = false;
            if (scan_radio_prepared && configuration.wifi_valid()) {
                WiFi.setAutoReconnect(true);
                if (WiFi.status() != WL_CONNECTED) {
                    last_connect_attempt_ms = millis() - kReconnectIntervalMs;
                }
            }
            scan_radio_prepared = false;
            ++scan_generation;
        } else if (complete == WIFI_SCAN_FAILED) {
            WiFi.scanDelete();
            scanned_network_count = 0;
            retry_or_finish_failed_wifi_scan();
        }
    }

    void service_wifi_scan() {
        refresh_scan_results();
        start_requested_wifi_scan();
    }

    void handle_setup_page() {
        String page;
        page.reserve(4096);
        append_page_head(page, "Calendar Wi-Fi setup");
        page += "<h1>1. Connect the calendar to Wi-Fi</h1><p>Select a nearby home network or type a hidden network name. The next page, on your home network, is where you add the private calendar address.</p>";
        if (status.wifi_detail[0] != '\0') {
            page += "<div class=\"card bad\"><strong>Last connection result</strong><p>";
            append_html_escaped(page, status.wifi_detail);
            page += "</p></div>";
        }
        page += "<form method=\"post\" action=\"/wifi\"><label>Wi-Fi name (SSID)</label><input list=\"networks\" name=\"ssid\" maxlength=\"32\" required autocomplete=\"off\"><datalist id=\"networks\">";
        for (std::size_t index = 0; index < scanned_network_count; ++index) {
            page += "<option value=\"";
            append_html_escaped(page, scanned_networks[index].ssid);
            page += "\" label=\"";
            append_html_escaped(page, scanned_networks[index].ssid);
            page += scanned_networks[index].secured ? " (secured, " : " (open, ";
            page += String(scanned_networks[index].rssi);
            page += " dBm)\"></option>";
        }
        page += "</datalist><small>Hidden networks can be typed above. Use Refresh before entering a password if the network is missing.</small><p><button type=\"button\" onclick=\"fetch('/scan',{method:'POST',cache:'no-store'}).then(()=>setTimeout(()=>location.reload(),1800))\">Refresh nearby networks</button></p><label>Wi-Fi password</label><input name=\"password\" type=\"password\" maxlength=\"63\" autocomplete=\"current-password\"><button type=\"submit\">Save Wi-Fi and connect</button></form>";
        page += "</body></html>";
        add_private_headers();
        server.send(200, "text/html", page);
    }

    void handle_wifi_save() {
        if (!save_wifi_configuration(server.arg("ssid"), server.arg("password"))) {
            add_private_headers();
            server.send(400, "text/html", "<p>Use a Wi-Fi name up to 32 characters and a password up to 63 characters.</p><p><a href='/'>Back</a></p>");
            return;
        }
        add_private_headers();
        server.send(200, "text/html", "<p>Wi-Fi saved. The calendar will now join your home network and synchronize time. Look at the display for its local web address.</p>");
        connect_after_response = true;
    }

    void handle_lan_page() {
        String page;
        page.reserve(11000);
        append_page_head(page, "Calendar settings", true);
        page += "<h1>2. Add private calendars</h1><p>This page is available only on your home network. Add up to four Google Calendar <em>Secret addresses in iCal format</em>. Addresses are stored on this display and are never shown again. Leave an address blank to keep the saved one.</p>";
        page += "<form method=\"post\" action=\"/calendar\">";
        for (std::size_t index = 0; index < kMaxCalendarFeeds; ++index) {
            const CalendarFeed& feed = configuration.feeds[index];
            page += "<div class=\"card\"><h2>Calendar ";
            page += String(index + 1);
            page += feed.configured() ? " <small class=\"ok\">Configured</small></h2>" :
                                        " <small>Not configured</small></h2>";
            page += "<label>Display name</label><input name=\"name";
            page += String(index);
            page += "\" maxlength=\"48\" autocomplete=\"off\" value=\"";
            append_html_escaped(page, feed.label);
            page += "\" placeholder=\"Calendar ";
            page += String(index + 1);
            page += "\"><label>Private iCal address</label><input name=\"feed";
            page += String(index);
            page += "\" type=\"url\" maxlength=\"511\" placeholder=\"";
            page += feed.configured() ? "Leave blank to keep saved address" : "https://...";
            page += "\" autocomplete=\"off\">";
            if (feed.configured()) {
                page += "<label><input style=\"width:auto;margin-right:.5rem\" name=\"remove";
                page += String(index);
                page += "\" type=\"checkbox\" value=\"1\">Remove this calendar</label>";
            }
            page += "</div>";
        }
        page += "<button type=\"submit\">Save and fetch calendars</button></form>";
        const WeatherServiceStatus weather = weather_service().status();
        const WeatherLocationConfiguration weather_config =
            weather_service().location_configuration();
        page += "<div class=\"card\"><h2>OpenWeather</h2><p>Add one location for the Weather screen. The API key is stored locally and is never shown again.</p>";
        page += "<form id=\"weather-form\" method=\"post\" action=\"/weather\"><label>OpenWeather API key</label><input name=\"api_key\" type=\"password\" maxlength=\"96\" ";
        if (!weather_config.api_key_configured) page += "required ";
        page += "autocomplete=\"off\" placeholder=\"";
        page += weather_config.api_key_configured
            ? "Leave blank to keep the saved API key"
            : "Required for initial setup";
        page += "\"><label>Weather units</label><select name=\"units\"><option value=\"metric\"";
        if (weather_config.units == WeatherUnits::Metric) page += " selected";
        page += ">Metric (&deg;C, km/h)</option><option value=\"imperial\"";
        if (weather_config.units == WeatherUnits::Imperial) page += " selected";
        page += ">Imperial (&deg;F, mph)</option></select>";
        page += "<label>Choose location on map</label><button id=\"load-weather-map\" type=\"button\">Load map</button><div id=\"weather-map\" class=\"map-hidden\" role=\"application\" aria-label=\"Weather location map\"></div><small id=\"map-help\" class=\"map-help\">Loading the map shares the viewed area with OpenStreetMap. You can keep using the coordinate boxes without loading it.</small>";
        page += "<label>Latitude</label><input id=\"weather-latitude\" name=\"latitude\" inputmode=\"decimal\" maxlength=\"15\" placeholder=\"31.7683\" required autocomplete=\"off\" value=\"";
        append_html_escaped(page, weather_config.latitude);
        page += "\"><label>Longitude</label><input id=\"weather-longitude\" name=\"longitude\" inputmode=\"decimal\" maxlength=\"15\" placeholder=\"35.2137\" required autocomplete=\"off\" value=\"";
        append_html_escaped(page, weather_config.longitude);
        page += "\"><small>Decimal point or comma is accepted.</small>";
        page += "<label>Location label</label><input name=\"location\" maxlength=\"48\" placeholder=\"Home\" autocomplete=\"off\" value=\"";
        append_html_escaped(page, weather_config.location);
        page += "\"><button type=\"submit\">Save weather settings</button></form><p>Weather status: ";
        append_html_escaped(page, weather.detail);
        page += "</p></div>";
        page += "<div class=\"card\"><strong>Connection and calendar status</strong><p id=\"status\">Checking status...</p></div><script>async function s(){try{let r=await fetch('/status',{cache:'no-store'});let j=await r.json();let x='Wi-Fi '+(j.wifi?'connected':'not connected');if(j.wifi_detail)x+=' ('+j.wifi_detail+')';x+='; '+j.feed_count+' calendar'+(j.feed_count===1?'':'s')+' configured; '+j.fetch; if(j.detail)x+=': '+j.detail; if(j.import_known)x+='; '+(j.import_ok?'imported ':'could not import ')+j.imported+' event(s), '+j.skipped+' skipped'; document.getElementById('status').textContent=x;}catch(e){document.getElementById('status').textContent='Status unavailable; keep this page open while the download runs.'}}s();setInterval(s,1500);</script>";
        page += "<script src=\"https://unpkg.com/leaflet@1.9.4/dist/leaflet.js\" integrity=\"sha256-20nQCchB9co0qIjJZRGuk2/Z9VM+kNiyxNV1lvTlZBo=\" crossorigin=\"\"></script><script>(()=>{const a=document.getElementById('weather-latitude'),o=document.getElementById('weather-longitude'),h=document.getElementById('map-help'),b=document.getElementById('load-weather-map'),d=document.getElementById('weather-map');if(!window.L){b.disabled=true;h.textContent='Map unavailable. Enter latitude and longitude manually.';return}const n=v=>Number.parseFloat(v.replace(',','.')),valid=(x,y)=>Number.isFinite(x)&&Number.isFinite(y)&&x>=-90&&x<=90&&y>=-180&&y<=180;b.addEventListener('click',()=>{b.disabled=true;b.hidden=true;d.classList.remove('map-hidden');let x=n(a.value),y=n(o.value);if(!valid(x,y)){x=31.7683;y=35.2137;a.value=x.toFixed(6);o.value=y.toFixed(6)}const m=L.map(d).setView([x,y],13),p=L.marker([x,y],{draggable:true}).addTo(m);L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:19,attribution:'&copy; <a href=\"https://www.openstreetmap.org/copyright\">OpenStreetMap</a> contributors'}).addTo(m);const set=(q,r,pan)=>{a.value=q.toFixed(6);o.value=r.toFixed(6);p.setLatLng([q,r]);if(pan)m.panTo([q,r])};m.on('click',e=>set(e.latlng.lat,e.latlng.lng,false));p.on('dragend',()=>{const q=p.getLatLng();set(q.lat,q.lng,true)});const typed=()=>{const q=n(a.value),r=n(o.value);if(valid(q,r))set(q,r,true)};a.addEventListener('change',typed);o.addEventListener('change',typed);h.textContent='Tap the map or drag the pin to update the coordinate boxes.';setTimeout(()=>m.invalidateSize(),0)},{once:true})})();</script>";
        page += "</body></html>";
        // OSM's browser tile policy requires a valid Referer. The origin-only
        // policy exposes no form values or private calendar/API credentials.
        add_private_headers(true);
        server.send(200, "text/html", page);
    }

    void handle_calendar_save() {
        CalendarFeed updated[kMaxCalendarFeeds]{};
        bool input_valid = true;
        for (std::size_t index = 0; index < kMaxCalendarFeeds; ++index) {
            updated[index] = configuration.feeds[index];
            const String suffix(index);
            if (server.hasArg("remove" + suffix)) {
                updated[index] = CalendarFeed{};
                continue;
            }

            String entered_url = server.arg("feed" + suffix);
            String entered_label = server.arg("name" + suffix);
            entered_url.trim();
            entered_label.trim();
            if (entered_url.length() != 0) {
                if (!starts_with_https(entered_url) || has_control_character(entered_url)) {
                    input_valid = false;
                    break;
                }
                copy_text(updated[index].url, entered_url.c_str());
            }
            if (updated[index].configured()) {
                if (entered_label.length() != 0) {
                    if (!valid_calendar_label(entered_label)) {
                        input_valid = false;
                        break;
                    }
                    copy_text(updated[index].label, entered_label.c_str());
                } else if (updated[index].label[0] == '\0') {
                    std::snprintf(updated[index].label, sizeof(updated[index].label), "Calendar %u",
                                  static_cast<unsigned>(index + 1));
                }
            }
        }

        if (!input_valid || !save_calendar_feeds(updated)) {
            add_private_headers();
            server.send(400, "text/html", "<p>Keep at least one calendar. Each configured calendar needs a private https:// iCal address and a display name up to 48 characters.</p><p><a href='/'>Back</a></p>");
            return;
        }
        status.fetch_state = CalendarFetchState::WaitingForNetwork;
        status.import_result_known = false;
        status.calendar_document_available = false;
        set_fetch_detail("Waiting for Wi-Fi and network time");
        set_summary("Calendar settings saved; waiting to download");
        sync_after_response = true;
        server.sendHeader("Location", "/");
        add_private_headers();
        server.send(303, "text/plain", "");
    }

    void handle_weather_save() {
        String api_key = server.arg("api_key");
        String latitude = server.arg("latitude");
        String longitude = server.arg("longitude");
        String location = server.arg("location");
        String units = server.arg("units");
        api_key.trim();
        latitude.trim();
        longitude.trim();
        location.trim();
        units.trim();
        WeatherUnits weather_units = WeatherUnits::Metric;
        if (units == "metric") {
            weather_units = WeatherUnits::Metric;
        } else if (units == "imperial") {
            weather_units = WeatherUnits::Imperial;
        } else {
            add_private_headers();
            server.send(400, "text/html", "<p>Select Metric or Imperial weather units.</p><p><a href='/'>Back</a></p>");
            return;
        }
        // Mobile decimal keyboards commonly follow the phone locale. Accept a
        // decimal comma here, then store/send one canonical representation.
        latitude.replace(',', '.');
        longitude.replace(',', '.');
        const bool saved = weather_service().save_configuration(
            api_key.c_str(), latitude.c_str(), longitude.c_str(), location.c_str(),
            weather_units);
        if (!saved) {
            String page;
            page.reserve(256);
            page += "<p>";
            append_html_escaped(page, weather_service().status().detail);
            page += "</p><p><a href='/'>Back</a></p>";
            add_private_headers();
            server.send(400, "text/html", page);
            return;
        }
        server.sendHeader("Location", "/");
        add_private_headers();
        server.send(303, "text/plain", "");
    }

    void handle_status() {
        String response;
        response.reserve(320);
        response += "{\"wifi\":";
        response += status.wifi_connected ? "true" : "false";
        response += ",\"wifi_detail\":\"";
        append_json_escaped(response, status.wifi_detail);
        response += "\"";
        response += ",\"time\":";
        response += status.time_synchronized ? "true" : "false";
        response += ",\"feed_saved\":";
        response += status.calendar_feed_saved ? "true" : "false";
        response += ",\"feed_count\":" + String(status.configured_feed_count);
        response += ",\"fetch\":\"";
        append_json_escaped(response, fetch_state_text(status.fetch_state));
        response += "\",\"detail\":\"";
        append_json_escaped(response, status.fetch_detail);
        response += "\",\"import_known\":";
        response += status.import_result_known ? "true" : "false";
        response += ",\"import_ok\":";
        response += status.last_import_succeeded ? "true" : "false";
        response += ",\"imported\":" + String(status.last_imported_events);
        response += ",\"skipped\":" + String(status.last_skipped_events);
        const WeatherServiceStatus weather = weather_service().status();
        response += ",\"weather_configured\":";
        response += weather.configured ? "true" : "false";
        response += ",\"weather\":\"";
        append_json_escaped(response, weather.detail);
        response += "\"";
        response += "}";
        add_private_headers();
        server.send(200, "application/json", response);
    }

    void configure_routes() {
        if (routes_configured) return;
        server.on("/", HTTP_GET, [this]() {
            if (portal_running) handle_setup_page(); else handle_lan_page();
        });
        server.on("/wifi", HTTP_POST, [this]() { handle_wifi_save(); });
        server.on("/scan", HTTP_POST, [this]() {
            if (!portal_running) {
                server.send(404, "text/plain", "Not found");
                return;
            }
            if (worker_running || external_activity != NetworkActivity::Idle) {
                add_private_headers();
                server.send(409, "text/plain", "A network operation is in progress");
                return;
            }
            request_wifi_scan();
            add_private_headers();
            server.send(202, "text/plain", "Wi-Fi scan requested");
        });
        server.on("/calendar", HTTP_POST, [this]() {
            if (!portal_running) handle_calendar_save(); else server.send(404, "text/plain", "Not found");
        });
        server.on("/weather", HTTP_POST, [this]() {
            if (!portal_running) handle_weather_save(); else server.send(404, "text/plain", "Not found");
        });
        server.on("/status", HTTP_GET, [this]() { handle_status(); });
        server.onNotFound([this]() {
            server.sendHeader("Location", "/");
            server.send(302, "text/plain", "");
        });
        routes_configured = true;
    }

    void stop_setup_portal() {
        if (!portal_running) return;
        server.stop();
        WiFi.softAPdisconnect(true);
        // Switching from AP+STA to STA preserves a recovered station link.
        WiFi.mode(WIFI_STA);
        portal_running = false;
        fallback_portal_running = false;
        lan_server_running = false;
        status.setup_ap_active = false;
        status.setup_ssid[0] = '\0';
        status.setup_password[0] = '\0';
        status.setup_address[0] = '\0';
    }

    void start_portal(const char* summary = "Connect phone to setup Wi-Fi",
                      bool preserve_station = false) {
        if (portal_running) {
            return;
        }
        if (lan_server_running) {
            server.stop();
            lan_server_running = false;
        }
        if (!preserve_station) WiFi.disconnect(false, false);
        WiFi.mode(WIFI_AP_STA);
        prepare_setup_credentials();
        if (!WiFi.softAP(status.setup_ssid, status.setup_password)) {
            status.state = ConnectivityState::Error;
            set_summary("Could not start setup Wi-Fi");
            return;
        }
        configure_routes();
        server.begin();
        portal_running = true;
        fallback_portal_running = preserve_station;
        scan_in_progress = false;
        scan_requested = false;
        scan_radio_prepared = false;
        scan_completed = false;
        scan_failed = false;
        scanned_network_count = 0;
        request_wifi_scan();
        status.state = ConnectivityState::SetupPortal;
        status.setup_ap_active = true;
        set_summary(summary);
    }

    void start_station_attempt() {
        if (scan_requested || scan_in_progress) return;
        // In fallback AP+STA mode, leave the AP up while the station retries.
        if (!fallback_portal_running) WiFi.disconnect(false, false);
        WiFi.begin(configuration.ssid, configuration.password);
        last_connect_attempt_ms = millis();
        if (station_connect_attempts < UINT8_MAX) ++station_connect_attempts;
    }

    void start_station() {
        if (!configuration.wifi_valid()) {
            status.state = ConnectivityState::NotConfigured;
            set_summary("Set up home Wi-Fi first");
            return;
        }
        if (portal_running) {
            stop_setup_portal();
        }
        lan_server_running = false;
        status.setup_ap_active = false;
        status.setup_ssid[0] = '\0';
        status.setup_password[0] = '\0';
        status.setup_address[0] = '\0';
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        station_connect_attempts = 0;
        start_station_attempt();
        ntp_started = false;
        status.wifi_connected = false;
        status.time_synchronized = false;
        status.station_address[0] = '\0';
        status.state = ConnectivityState::Connecting;
        set_summary("Connecting to home Wi-Fi");
    }

    void start_lan_server() {
        if (lan_server_running || portal_running) return;
        configure_routes();
        server.begin();
        lan_server_running = true;
    }

    void update_station() {
        consume_wifi_disconnect();
        if (WiFi.status() != WL_CONNECTED) {
            if (lan_server_running) {
                server.stop();
                lan_server_running = false;
            }
            status.wifi_connected = false;
            status.time_synchronized = false;
            status.station_address[0] = '\0';
            if (configuration.wifi_valid() && !worker_running &&
                external_activity == NetworkActivity::Idle && !scan_requested && !scan_in_progress &&
                millis() - last_connect_attempt_ms >= kReconnectIntervalMs) {
                if (station_connect_attempts >= kMaxStationConnectAttemptsBeforePortal) {
                    if (!fallback_portal_running) {
                        // Keep the protected AP available for correction while
                        // preserving STA retries in case the saved network
                        // returns without user intervention.
                        start_portal(status.wifi_detail[0] != '\0'
                                         ? status.wifi_detail
                                         : "Home Wi-Fi unavailable; setup Wi-Fi is on",
                                     true);
                        return;
                    } else {
                        start_station_attempt();
                    }
                } else {
                    start_station_attempt();
                }
            }
            if (fallback_portal_running) {
                status.state = ConnectivityState::SetupPortal;
                set_summary(status.wifi_detail[0] != '\0'
                                ? status.wifi_detail
                                : "Home Wi-Fi unavailable; setup Wi-Fi is on");
            } else if (configuration.wifi_valid()) {
                status.state = ConnectivityState::Connecting;
                set_summary(status.wifi_detail[0] != '\0'
                                ? status.wifi_detail
                                : "Connecting to home Wi-Fi");
            }
            return;
        }

        if (fallback_portal_running) {
            // The saved station recovered while the fallback AP was active.
            // Drop only the AP and resume the normal LAN service.
            stop_setup_portal();
        }
        status.wifi_connected = true;
        // Give a later, independent outage a full reconnect budget instead of
        // carrying the boot-time association attempts forever.
        station_connect_attempts = 0;
        status.wifi_detail[0] = '\0';
        const String address = WiFi.localIP().toString();
        copy_text(status.station_address, address.c_str());
        if (!ntp_started) {
            configTzTime(kTzJerusalem, kNtpPrimary, kNtpSecondary);
            ntp_started = true;
        }
        if (std::time(nullptr) >= kPlausibleUnixTime) {
            status.time_synchronized = true;
            status.state = ConnectivityState::Ready;
            start_lan_server();
            if (status.fetch_state != CalendarFetchState::Downloading &&
                status.fetch_state != CalendarFetchState::Succeeded &&
                status.fetch_state != CalendarFetchState::Failed) {
                set_summary("Wi-Fi and Jerusalem time ready");
            }
        } else {
            status.time_synchronized = false;
            status.state = ConnectivityState::SynchronizingTime;
            set_summary("Synchronizing Jerusalem time");
        }
    }

    void run_download() {
        SavedConfiguration local_configuration{};
        std::uint32_t local_configuration_generation = 0;
        xSemaphoreTake(lock, portMAX_DELAY);
        local_configuration = configuration;
        local_configuration_generation = calendar_configuration_generation;
        xSemaphoreGive(lock);

        CalendarDocument document;
        const bool ready = WiFi.status() == WL_CONNECTED && std::time(nullptr) >= kPlausibleUnixTime &&
                           local_configuration.valid();
        bool success = false;
        String failure_detail("Calendar downloads did not complete");
        if (ready) {
            if (!document.reserve(calendar::kMaxIcalFeedBytes)) {
                failure_detail = "No PSRAM available for 1 MiB calendars";
            } else {
                success = true;
                for (std::size_t index = 0; index < kMaxCalendarFeeds && success; ++index) {
                    const CalendarFeed& feed = local_configuration.feeds[index];
                    if (!feed.configured()) continue;

                    String prefix;
                    prefix.reserve(160);
                    prefix += "X-ESP32-CALENDAR-ID:calendar-";
                    prefix += String(index + 1);
                    prefix += "\nX-ESP32-CALENDAR-NAME:";
                    prefix += feed.label;
                    prefix += "\nX-ESP32-CALENDAR-COLOR:";
                    prefix += kCalendarColors[index];
                    prefix += "\n";
                    if (prefix.length() > calendar::kMaxIcalFeedBytes - document.size() ||
                        !document.append(reinterpret_cast<const std::uint8_t*>(prefix.c_str()),
                                         prefix.length())) {
                        failure_detail = "Combined calendars are larger than 1 MiB";
                        success = false;
                        break;
                    }
                    const std::size_t body_start = document.size();

                    bool feed_success = false;
                    for (std::uint8_t attempt = 0; attempt < kCalendarBodyAttempts; ++attempt) {
                        // A failed HTTP body may already have appended bytes.
                        // Always restart the feed at its exact pre-body offset.
                        if (!document.truncate(body_start)) {
                            failure_detail = "Calendar download buffer rollback failed";
                            break;
                        }
                        if (WiFi.status() != WL_CONNECTED) {
                            failure_detail = "Wi-Fi disconnected during calendar download";
                            break;
                        }

                        bool retryable = false;
                        WiFiClientSecure client;
                        client.setCACert(kGoogleTrustServicesRootR1);
                        HTTPClient http;
                        http.setConnectTimeout(12000);
                        http.setTimeout(12000);
                        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
                        if (!http.begin(client, feed.url)) {
                            failure_detail = "Could not connect for calendar " + String(index + 1);
                        } else {
                            const int response = http.GET();
                            if (response < 0) {
                                failure_detail = "Calendar " + String(index + 1) + " transport: " +
                                                 HTTPClient::errorToString(response);
                                retryable = retryable_calendar_body_error(response);
                            } else if (response != HTTP_CODE_OK) {
                                failure_detail = "Calendar " + String(index + 1) +
                                                 " returned HTTP " + String(response);
                            } else {
                                const int content_length = http.getSize();
                                const std::size_t remaining =
                                    calendar::kMaxIcalFeedBytes - document.size();
                                if (content_length >= 0 &&
                                    static_cast<std::size_t>(content_length) > remaining) {
                                    failure_detail = "Combined calendars are larger than 1 MiB";
                                } else {
                                    BoundedDocumentSink sink(document, calendar::kMaxIcalFeedBytes);
                                    const int written = http.writeToStream(&sink);
                                    const std::size_t body_bytes = document.size() - body_start;
                                    if (sink.overflowed()) {
                                        failure_detail = "Combined calendars are larger than 1 MiB";
                                    } else if (written < 0) {
                                        retryable = retryable_calendar_body_error(written);
                                        if (written == HTTPC_ERROR_STREAM_WRITE ||
                                            written == HTTPC_ERROR_CONNECTION_LOST) {
                                            failure_detail = "Calendar " + String(index + 1) +
                                                             " connection closed during body";
                                        } else if (written == HTTPC_ERROR_READ_TIMEOUT) {
                                            failure_detail = "Calendar " + String(index + 1) +
                                                             " body timed out";
                                        } else if (written == HTTPC_ERROR_TOO_LESS_RAM) {
                                            failure_detail =
                                                "Not enough internal RAM for HTTPS download";
                                        } else {
                                            failure_detail = "Calendar " + String(index + 1) +
                                                             " body: " +
                                                             HTTPClient::errorToString(written);
                                        }
                                    } else if (body_bytes == 0) {
                                        failure_detail = "Calendar " + String(index + 1) +
                                                         " was empty";
                                    } else if (content_length >= 0 &&
                                               body_bytes !=
                                                   static_cast<std::size_t>(content_length)) {
                                        failure_detail = "Calendar " + String(index + 1) +
                                                         " download ended early";
                                        retryable = true;
                                    } else {
                                        feed_success = true;
                                    }
                                }
                            }
                        }
                        http.end();
                        if (feed_success || !retryable ||
                            attempt + 1U >= kCalendarBodyAttempts) {
                            break;
                        }
                        vTaskDelay(pdMS_TO_TICKS(kCalendarBodyRetryDelayMs));
                    }
                    success = feed_success;
                    if (success) {
                        static constexpr std::uint8_t newline = '\n';
                        if (!document.append(&newline, 1)) {
                            failure_detail = "Combined calendars are larger than 1 MiB";
                            success = false;
                        }
                    }
                }
            }
        } else {
            failure_detail = "Wi-Fi or network time is not ready";
        }

        xSemaphoreTake(lock, portMAX_DELAY);
        if (local_configuration_generation != calendar_configuration_generation) {
            status.calendar_document_available = false;
            status.fetch_state = CalendarFetchState::WaitingForNetwork;
            status.last_fetch_bytes = 0;
            status.import_result_known = false;
            next_calendar_refresh_ms = 0;
            set_fetch_detail("Calendar settings changed; restarting download");
            set_summary("Calendar settings changed; refreshing");
            worker_running = false;
            xSemaphoreGive(lock);
            return;
        }
        if (success) {
            document.set_configuration_generation(local_configuration_generation);
            downloaded_ical = std::move(document);
            status.calendar_document_available = true;
            status.fetch_state = CalendarFetchState::Succeeded;
            status.last_fetch_bytes = downloaded_ical.size();
            status.import_result_known = false;
            set_fetch_detail("All calendar downloads completed; importing");
            set_summary("Calendars downloaded; importing");
        } else {
            // Keep an already downloaded document available.  The UI can retain
            // its previously parsed calendar while a later refresh is retried.
            status.fetch_state = CalendarFetchState::Failed;
            status.last_fetch_bytes = 0;
            status.import_result_known = false;
            set_fetch_detail(failure_detail.c_str());
            set_summary("Calendar download failed");
        }
        worker_running = false;
        xSemaphoreGive(lock);
    }

    static void download_task(void* argument) {
        auto* impl = static_cast<Impl*>(argument);
        impl->run_download();
        vTaskDelete(nullptr);
    }
};

ConnectivityService::ConnectivityService() : impl_(std::make_unique<Impl>()) {}
ConnectivityService::~ConnectivityService() = default;

bool ConnectivityService::initialize() {
    if (impl_->startup_attempted) return impl_->startup_succeeded;
    impl_->startup_attempted = true;
    impl_->lock = xSemaphoreCreateMutex();
    if (impl_->lock == nullptr) {
        impl_->status.state = ConnectivityState::Error;
        impl_->set_summary("No memory for network service");
        return false;
    }
    if (!psramFound()) {
        impl_->status.state = ConnectivityState::Error;
        impl_->set_summary("PSRAM is unavailable for calendar data");
        vSemaphoreDelete(impl_->lock);
        impl_->lock = nullptr;
        return false;
    }
    // Select RAM-backed ESP-IDF Wi-Fi configuration before the first radio API
    // initializes Wi-Fi. Saved credentials live only in calendar-net and are
    // explicitly supplied to every station attempt.
    WiFi.persistent(false);
    impl_->wifi_disconnect_event_id = WiFi.onEvent(
        [service = impl_.get()](WiFiEvent_t, WiFiEventInfo_t info) {
            service->note_wifi_disconnect(info.wifi_sta_disconnected.reason);
        },
        ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    impl_->wifi_disconnect_event_registered = true;
    if (!impl_->load_configuration()) {
        impl_->status.state = ConnectivityState::Error;
        impl_->set_summary("Could not open saved network settings");
        WiFi.removeEvent(impl_->wifi_disconnect_event_id);
        impl_->wifi_disconnect_event_registered = false;
        vSemaphoreDelete(impl_->lock);
        impl_->lock = nullptr;
        return false;
    }
    impl_->initialized = true;
    impl_->status.service_initialized = true;
    if (impl_->configuration.valid()) {
        impl_->start_station();
    } else if (impl_->configuration.wifi_valid()) {
        impl_->start_station();
    } else {
        // A newly flashed board must be reachable without a display-side
        // action. The protected AP remains available until Wi-Fi is saved.
        impl_->start_portal("No home Wi-Fi saved; setup Wi-Fi is on");
    }
    impl_->startup_succeeded = impl_->status.state != ConnectivityState::Error;
    return impl_->startup_succeeded;
}

void ConnectivityService::tick() {
    if (!impl_->initialized) {
        return;
    }
    // The download task only holds this mutex while copying configuration or
    // publishing its result.  Holding it around the short station/portal
    // bookkeeping keeps status and credentials coherent without stalling the
    // TLS transfer.
    bool request_sync_after_response = false;
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    if (impl_->portal_running) {
        if (impl_->fallback_portal_running) {
            // A fallback AP does not abandon the saved station. This keeps the
            // board reachable for correction while it recovers automatically
            // if the home network returns.
            impl_->update_station();
        }
        if (impl_->portal_running) {
            impl_->service_wifi_scan();
            impl_->server.handleClient();
            if (impl_->connect_after_response && !impl_->scan_requested && !impl_->scan_in_progress) {
                impl_->connect_after_response = false;
                impl_->start_station();
            }
        }
    } else {
        // A display-initiated scan remains asynchronous while the ordinary
        // station/LAN service continues to run.
        impl_->service_wifi_scan();
        impl_->update_station();
        if (impl_->lan_server_running) impl_->server.handleClient();
    }
    if (impl_->sync_after_response) {
        impl_->sync_after_response = false;
        request_sync_after_response = true;
    }
    xSemaphoreGive(impl_->lock);
    if (request_sync_after_response && !request_sync()) {
        xSemaphoreTake(impl_->lock, portMAX_DELAY);
        if (impl_->status.fetch_state != CalendarFetchState::Downloading) {
            impl_->status.fetch_state = CalendarFetchState::WaitingForNetwork;
            impl_->set_summary("Calendars saved; waiting for Wi-Fi and time");
        }
        xSemaphoreGive(impl_->lock);
    }

    // The network owner performs the five-minute check. This only schedules
    // the existing background transfer; it never blocks the LVGL loop.
    bool request_periodic_sync = false;
    const std::uint32_t now_ms = millis();
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    const bool due = impl_->next_calendar_refresh_ms == 0 ||
                     static_cast<std::int32_t>(now_ms - impl_->next_calendar_refresh_ms) >= 0;
    if (!impl_->portal_running && !impl_->worker_running &&
        impl_->external_activity == NetworkActivity::Idle &&
        !impl_->status.calendar_document_available && impl_->configuration.valid() &&
        impl_->status.wifi_connected && impl_->status.time_synchronized && due) {
        request_periodic_sync = true;
    }
    xSemaphoreGive(impl_->lock);
    if (request_periodic_sync) request_sync();
}

void ConnectivityService::begin_setup() {
    if (!impl_->initialized || impl_->lock == nullptr) {
        return;
    }
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    if (impl_->worker_running || impl_->external_activity != NetworkActivity::Idle ||
        impl_->scan_requested || impl_->scan_in_progress) {
        impl_->set_summary((impl_->worker_running || impl_->external_activity != NetworkActivity::Idle)
                               ? "Wait for network operation" : "Wait for Wi-Fi scan");
        xSemaphoreGive(impl_->lock);
        return;
    }
    impl_->start_portal();
    xSemaphoreGive(impl_->lock);
}

bool ConnectivityService::save_display_wifi_credentials(const char* ssid, const char* password) {
    if (!impl_->initialized || impl_->lock == nullptr || ssid == nullptr || password == nullptr) {
        return false;
    }
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    if (impl_->worker_running || impl_->external_activity != NetworkActivity::Idle ||
        impl_->scan_requested || impl_->scan_in_progress) {
        impl_->set_summary((impl_->worker_running || impl_->external_activity != NetworkActivity::Idle)
                               ? "Wait for network operation" : "Wait for Wi-Fi scan");
        xSemaphoreGive(impl_->lock);
        return false;
    }

    const String entered_ssid(ssid);
    const String entered_password(password);
    if (!impl_->save_wifi_configuration(entered_ssid, entered_password)) {
        impl_->set_summary("Wi-Fi name or password is invalid");
        xSemaphoreGive(impl_->lock);
        return false;
    }

    // This only changes the station state. The actual association, NTP, and
    // any calendar transfer continue asynchronously from tick().
    impl_->start_station();
    xSemaphoreGive(impl_->lock);
    return true;
}

WifiScanRequestResult ConnectivityService::request_wifi_scan() {
    if (!impl_->initialized || impl_->lock == nullptr) {
        return WifiScanRequestResult::ServiceStarting;
    }
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    if (impl_->worker_running) {
        xSemaphoreGive(impl_->lock);
        return WifiScanRequestResult::CalendarBusy;
    }
    if (impl_->external_activity == NetworkActivity::WeatherDownload) {
        xSemaphoreGive(impl_->lock);
        return WifiScanRequestResult::WeatherBusy;
    }
    if (impl_->external_activity == NetworkActivity::OtaUpdate) {
        xSemaphoreGive(impl_->lock);
        return WifiScanRequestResult::OtaBusy;
    }
    if (impl_->scan_requested || impl_->scan_in_progress) {
        xSemaphoreGive(impl_->lock);
        return WifiScanRequestResult::AlreadyPending;
    }
    impl_->request_wifi_scan();
    xSemaphoreGive(impl_->lock);
    return WifiScanRequestResult::Queued;
}

WifiScanResults ConnectivityService::wifi_scan_results() const {
    WifiScanResults copy{};
    if (impl_->lock == nullptr) return copy;
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    copy.in_progress = impl_->scan_in_progress;
    copy.completed = impl_->scan_completed;
    copy.failed = impl_->scan_failed;
    copy.generation = impl_->scan_generation;
    copy.count = impl_->scanned_network_count;
    for (std::size_t index = 0; index < copy.count; ++index) {
        copy.networks[index] = impl_->scanned_networks[index];
    }
    xSemaphoreGive(impl_->lock);
    return copy;
}

bool ConnectivityService::request_sync() {
    if (!impl_->initialized || impl_->lock == nullptr || WiFi.status() != WL_CONNECTED ||
        std::time(nullptr) < kPlausibleUnixTime) {
        return false;
    }
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    if (impl_->worker_running || impl_->external_activity != NetworkActivity::Idle ||
        impl_->scan_requested || impl_->scan_in_progress ||
        !impl_->configuration.valid()) {
        xSemaphoreGive(impl_->lock);
        return false;
    }
    impl_->worker_running = true;
    impl_->status.fetch_state = CalendarFetchState::Downloading;
    impl_->status.calendar_document_available = false;
    impl_->status.import_result_known = false;
    impl_->set_fetch_detail("Connecting to calendar server");
    impl_->set_summary("Downloading calendar");
    xSemaphoreGive(impl_->lock);

    TaskHandle_t task = nullptr;
    const BaseType_t created = xTaskCreatePinnedToCore(Impl::download_task, "ical-download", 12288,
                                                        impl_.get(), 1, &task, 0);
    if (created != pdPASS) {
        xSemaphoreTake(impl_->lock, portMAX_DELAY);
        impl_->worker_running = false;
        impl_->set_fetch_detail("Could not start download task");
        impl_->set_summary("Could not start calendar download");
        xSemaphoreGive(impl_->lock);
        return false;
    }
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    impl_->next_calendar_refresh_ms = millis() + kCalendarRefreshIntervalMs;
    xSemaphoreGive(impl_->lock);
    return true;
}

bool ConnectivityService::try_begin_network_activity(NetworkActivity activity) {
    if (!impl_->initialized || impl_->lock == nullptr || activity == NetworkActivity::Idle) {
        return false;
    }
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    const bool available = !impl_->portal_running && !impl_->worker_running &&
                           !impl_->scan_requested && !impl_->scan_in_progress &&
                           impl_->external_activity == NetworkActivity::Idle &&
                           impl_->status.wifi_connected;
    if (available) impl_->external_activity = activity;
    xSemaphoreGive(impl_->lock);
    return available;
}

void ConnectivityService::end_network_activity(NetworkActivity activity) {
    if (impl_->lock == nullptr || activity == NetworkActivity::Idle) return;
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    if (impl_->external_activity == activity) impl_->external_activity = NetworkActivity::Idle;
    xSemaphoreGive(impl_->lock);
}

NetworkActivity ConnectivityService::network_activity() const {
    if (impl_->lock == nullptr) return NetworkActivity::Idle;
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    const NetworkActivity activity = impl_->external_activity;
    xSemaphoreGive(impl_->lock);
    return activity;
}

ConnectivityStatus ConnectivityService::status() const {
    ConnectivityStatus copy{};
    if (impl_->lock == nullptr) {
        return impl_->status;
    }
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    copy = impl_->status;
    xSemaphoreGive(impl_->lock);
    return copy;
}

bool ConnectivityService::take_downloaded_ical(CalendarDocument& document) {
    if (impl_->lock == nullptr) {
        return false;
    }
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    if (impl_->downloaded_ical.empty()) {
        xSemaphoreGive(impl_->lock);
        return false;
    }
    document = std::move(impl_->downloaded_ical);
    impl_->status.calendar_document_available = false;
    xSemaphoreGive(impl_->lock);
    return true;
}

void ConnectivityService::report_ical_import_result(std::uint32_t configuration_generation,
                                                     bool success, std::size_t imported_events,
                                                     std::size_t skipped_events) {
    if (impl_->lock == nullptr) return;
    xSemaphoreTake(impl_->lock, portMAX_DELAY);
    // Ignore reports that do not correspond to a completed download. This
    // prevents an old application update from overwriting a newer web result.
    if (impl_->status.fetch_state == CalendarFetchState::Succeeded &&
        configuration_generation == impl_->calendar_configuration_generation) {
        impl_->status.import_result_known = true;
        impl_->status.last_import_succeeded = success;
        impl_->status.last_imported_events = imported_events;
        impl_->status.last_skipped_events = skipped_events;
        if (success) {
            const std::time_t imported_at = std::time(nullptr);
            if (impl_->status.time_synchronized && imported_at > 0) {
                impl_->status.last_successful_import_utc =
                    static_cast<std::int64_t>(imported_at);
            }
            ++impl_->status.successful_import_generation;
        }
        impl_->set_summary(success ? "Calendar imported" : "Calendar could not be imported");
    }
    xSemaphoreGive(impl_->lock);
}

ConnectivityService& connectivity_service() {
    static ConnectivityService service;
    return service;
}

}  // namespace board
