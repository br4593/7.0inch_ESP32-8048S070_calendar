#pragma once

#include <cstddef>

namespace calendar {

// One shared ceiling for both the HTTPS receiver and the iCalendar parser.
// The receiver allocates this buffer strictly from ESP32 PSRAM.
inline constexpr std::size_t kMaxIcalFeedBytes = 1024U * 1024U;

}  // namespace calendar
