cmake_minimum_required(VERSION 3.16)
project(esp32_touch_calendar_host_tests LANGUAGES CXX)

enable_testing()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(calendar_core
    src/app/calendar_count_format.cpp
    src/app/calendar_types.cpp
    src/app/calendar_provider.cpp
    src/app/calendar_period.cpp
    src/app/calendar_navigation.cpp
    src/app/ical_feed.cpp
    src/app/theme_palette.cpp
    src/app/weather_parser.cpp
    src/app/ui_status.cpp
    src/board/ota_health_window.cpp)
target_include_directories(calendar_core PUBLIC include)
target_compile_options(calendar_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(calendar_core_tests tests/calendar_core_tests.cpp)
target_link_libraries(calendar_core_tests PRIVATE calendar_core)
target_compile_options(calendar_core_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME calendar_core_tests COMMAND calendar_core_tests)

add_executable(ical_feed_tests tests/ical_feed_tests.cpp)
target_link_libraries(ical_feed_tests PRIVATE calendar_core)
target_compile_options(ical_feed_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME ical_feed_tests COMMAND ical_feed_tests)

add_executable(weather_tests tests/weather_tests.cpp)
target_link_libraries(weather_tests PRIVATE calendar_core)
target_compile_options(weather_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME weather_tests COMMAND weather_tests)

add_executable(ota_health_window_tests tests/ota_health_window_tests.cpp)
target_link_libraries(ota_health_window_tests PRIVATE calendar_core)
target_compile_options(ota_health_window_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME ota_health_window_tests COMMAND ota_health_window_tests)

add_executable(theme_palette_tests tests/theme_palette_tests.cpp)
target_link_libraries(theme_palette_tests PRIVATE calendar_core)
target_compile_options(theme_palette_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME theme_palette_tests COMMAND theme_palette_tests)

add_executable(ui_status_tests tests/ui_status_tests.cpp)
target_link_libraries(ui_status_tests PRIVATE calendar_core)
target_compile_options(ui_status_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME ui_status_tests COMMAND ui_status_tests)
