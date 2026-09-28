cmake_minimum_required(VERSION 3.16)
project(calendar_alignment_audit LANGUAGES C CXX)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CALENDAR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
set(LVGL_SOURCE_DIR "${CALENDAR_ROOT}/.pio/libdeps/rgb-staged-startup/lvgl")
set(LV_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/preview_lv_conf.h" CACHE STRING "Preview config" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
add_subdirectory("${LVGL_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/lvgl")
file(GLOB GENERATED_SOURCES "${CALENDAR_ROOT}/src/ui_generated/*.c")
set(CORE_SOURCES calendar_count_format calendar_types calendar_provider calendar_period calendar_navigation ical_feed theme_palette weather_parser)
foreach(NAME IN LISTS CORE_SOURCES)
    list(APPEND APP_SOURCES "${CALENDAR_ROOT}/src/app/${NAME}.cpp")
endforeach()
list(APPEND APP_SOURCES "${CALENDAR_ROOT}/src/app/ui_status.cpp")
add_executable(calendar_alignment_audit alignment.cpp
    "${CALENDAR_ROOT}/tools/ui_preview/services.cpp"
    ${GENERATED_SOURCES} ${APP_SOURCES}
    "${CALENDAR_ROOT}/src/app/ui_controller.cpp"
    "${CALENDAR_ROOT}/src/app/ui_actions.cpp"
    "${CALENDAR_ROOT}/src/app/weather_icon.cpp"
    "${CALENDAR_ROOT}/src/app/weather_icon_assets.cpp")
target_include_directories(calendar_alignment_audit PRIVATE stubs
    "${CALENDAR_ROOT}/tools/ui_preview"
    "${CALENDAR_ROOT}/include" "${CALENDAR_ROOT}/src")
target_compile_definitions(calendar_alignment_audit PRIVATE APP_RGB_STAGED_STARTUP=1)
target_link_libraries(calendar_alignment_audit PRIVATE lvgl m)
target_link_options(calendar_alignment_audit PRIVATE "-Wl,--wrap=time")
