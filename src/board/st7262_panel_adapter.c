// Project-owned adapter for the pinned esp32-smartdisplay ST7262 RGB panel.
// The vendor adapter calls lv_display_flush_ready() from every RGB frame ISR.
// Our synchronous partial flush in runtime.cpp owns that signal instead.
#ifdef DISPLAY_ST7262_PAR

#include <esp32_smartdisplay.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <lvgl.h>

#include <assert.h>
#include <stdint.h>

#ifdef APP_LVGL_DRAW_BUFFER_ROWS
_Static_assert(APP_LVGL_DRAW_BUFFER_ROWS > 0,
               "APP_LVGL_DRAW_BUFFER_ROWS must be positive");
_Static_assert(APP_LVGL_DRAW_BUFFER_ROWS <= UINT32_MAX / DISPLAY_WIDTH,
               "APP_LVGL_DRAW_BUFFER_ROWS overflows the pixel count");
#endif

#if defined(APP_RGB_DIAGNOSTICS) && ESP_IDF_VERSION_MAJOR >= 5
#include <esp_attr.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

static portMUX_TYPE diagnostics_vsync_mux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t diagnostics_vsync_count;

static bool IRAM_ATTR diagnostics_rgb_vsync(
    esp_lcd_panel_handle_t panel,
    const esp_lcd_rgb_panel_event_data_t *event_data, void *user_context) {
    (void)panel;
    (void)event_data;
    (void)user_context;
    portENTER_CRITICAL_ISR(&diagnostics_vsync_mux);
    ++diagnostics_vsync_count;
    portEXIT_CRITICAL_ISR(&diagnostics_vsync_mux);
    return false;
}

uint32_t app_rgb_take_vsync_count(void) {
    portENTER_CRITICAL(&diagnostics_vsync_mux);
    const uint32_t count = diagnostics_vsync_count;
    diagnostics_vsync_count = 0;
    portEXIT_CRITICAL(&diagnostics_vsync_mux);
    return count;
}
#elif defined(APP_RGB_DIAGNOSTICS)
uint32_t app_rgb_take_vsync_count(void) {
    return 0;
}
#endif

static void initial_rgb_flush(lv_display_t *display, const lv_area_t *area,
                              uint8_t *pixels) {
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(display);
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1,
                                              area->x2 + 1, area->y2 + 1,
                                              pixels));
    lv_display_flush_ready(display);
}

lv_display_t *lvgl_lcd_init(void) {
    lv_display_t *display = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    assert(display != NULL);
    const lv_color_format_t format = lv_display_get_color_format(display);
    const uint32_t bytes_per_pixel = lv_color_format_get_size(format);
#ifdef APP_LVGL_DRAW_BUFFER_ROWS
    uint32_t draw_buffer_pixels = DISPLAY_WIDTH * APP_LVGL_DRAW_BUFFER_ROWS;
#else
    // Preserve the board profile's exact default draw-buffer contract.
    uint32_t draw_buffer_pixels = LVGL_BUFFER_PIXELS;
#endif
    assert(bytes_per_pixel > 0);
    assert(draw_buffer_pixels > 0);
    assert(draw_buffer_pixels <= UINT32_MAX / bytes_per_pixel);
    uint32_t draw_buffer_size = bytes_per_pixel * draw_buffer_pixels;
    void *draw_buffer =
        heap_caps_malloc(draw_buffer_size, LVGL_BUFFER_MALLOC_FLAGS);
#ifdef APP_LVGL_DRAW_BUFFER_ROWS
    if (draw_buffer == NULL && APP_LVGL_DRAW_BUFFER_ROWS != 8) {
        log_w("LVGL %lu-row draw buffer allocation failed; falling back to 8 rows",
              (unsigned long)APP_LVGL_DRAW_BUFFER_ROWS);
        draw_buffer_pixels = DISPLAY_WIDTH * 8;
        draw_buffer_size = bytes_per_pixel * draw_buffer_pixels;
        draw_buffer =
            heap_caps_malloc(draw_buffer_size, LVGL_BUFFER_MALLOC_FLAGS);
    }
#endif
    assert(draw_buffer != NULL);
    log_i("LVGL draw buffer: %lu pixels, %lu bytes",
          (unsigned long)draw_buffer_pixels, (unsigned long)draw_buffer_size);
    lv_display_set_buffers(display, draw_buffer, NULL, draw_buffer_size,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    const esp_lcd_rgb_panel_config_t config = {
        .clk_src = ST7262_PANEL_CONFIG_CLK_SRC,
        .timings = {
            .pclk_hz = ST7262_PANEL_CONFIG_TIMINGS_PCLK_HZ,
            .h_res = ST7262_PANEL_CONFIG_TIMINGS_H_RES,
            .v_res = ST7262_PANEL_CONFIG_TIMINGS_V_RES,
            .hsync_pulse_width = ST7262_PANEL_CONFIG_TIMINGS_HSYNC_PULSE_WIDTH,
            .hsync_back_porch = ST7262_PANEL_CONFIG_TIMINGS_HSYNC_BACK_PORCH,
            .hsync_front_porch = ST7262_PANEL_CONFIG_TIMINGS_HSYNC_FRONT_PORCH,
            .vsync_pulse_width = ST7262_PANEL_CONFIG_TIMINGS_VSYNC_PULSE_WIDTH,
            .vsync_back_porch = ST7262_PANEL_CONFIG_TIMINGS_VSYNC_BACK_PORCH,
            .vsync_front_porch = ST7262_PANEL_CONFIG_TIMINGS_VSYNC_FRONT_PORCH,
            .flags = {
                .hsync_idle_low = ST7262_PANEL_CONFIG_TIMINGS_FLAGS_HSYNC_IDLE_LOW,
                .vsync_idle_low = ST7262_PANEL_CONFIG_TIMINGS_FLAGS_VSYNC_IDLE_LOW,
                .de_idle_high = ST7262_PANEL_CONFIG_TIMINGS_FLAGS_DE_IDLE_HIGH,
                .pclk_active_neg = ST7262_PANEL_CONFIG_TIMINGS_FLAGS_PCLK_ACTIVE_NEG,
                .pclk_idle_high = ST7262_PANEL_CONFIG_TIMINGS_FLAGS_PCLK_IDLE_HIGH,
            },
        },
        .data_width = ST7262_PANEL_CONFIG_DATA_WIDTH,
        .sram_trans_align = ST7262_PANEL_CONFIG_SRAM_TRANS_ALIGN,
        .psram_trans_align = ST7262_PANEL_CONFIG_PSRAM_TRANS_ALIGN,
#if ESP_IDF_VERSION_MAJOR >= 5
        // Two internal RGB565 DMA buffers, ten scanlines each. The panel keeps
        // its single full-size framebuffer in PSRAM.
        .bounce_buffer_size_px = DISPLAY_WIDTH * 10,
#endif
        .hsync_gpio_num = ST7262_PANEL_CONFIG_HSYNC,
        .vsync_gpio_num = ST7262_PANEL_CONFIG_VSYNC,
        .de_gpio_num = ST7262_PANEL_CONFIG_DE,
        .pclk_gpio_num = ST7262_PANEL_CONFIG_PCLK,
        .data_gpio_nums = {
            ST7262_PANEL_CONFIG_DATA_R0, ST7262_PANEL_CONFIG_DATA_R1,
            ST7262_PANEL_CONFIG_DATA_R2, ST7262_PANEL_CONFIG_DATA_R3,
            ST7262_PANEL_CONFIG_DATA_R4, ST7262_PANEL_CONFIG_DATA_G0,
            ST7262_PANEL_CONFIG_DATA_G1, ST7262_PANEL_CONFIG_DATA_G2,
            ST7262_PANEL_CONFIG_DATA_G3, ST7262_PANEL_CONFIG_DATA_G4,
            ST7262_PANEL_CONFIG_DATA_G5, ST7262_PANEL_CONFIG_DATA_B0,
            ST7262_PANEL_CONFIG_DATA_B1, ST7262_PANEL_CONFIG_DATA_B2,
            ST7262_PANEL_CONFIG_DATA_B3, ST7262_PANEL_CONFIG_DATA_B4,
        },
        .disp_gpio_num = ST7262_PANEL_CONFIG_DISP,
#if ESP_IDF_VERSION_MAJOR < 5
        .on_frame_trans_done = NULL,
        .user_ctx = NULL,
#endif
        .flags = {
            .disp_active_low = ST7262_PANEL_CONFIG_FLAGS_DISP_ACTIVE_LOW,
#if ESP_IDF_VERSION_MAJOR < 5
            .relax_on_idle = ST7262_PANEL_CONFIG_FLAGS_RELAX_ON_IDLE,
#endif
            .fb_in_psram = ST7262_PANEL_CONFIG_FLAGS_FB_IN_PSRAM,
        },
    };
    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&config, &panel));
#if defined(APP_RGB_DIAGNOSTICS) && ESP_IDF_VERSION_MAJOR >= 5
    const esp_lcd_rgb_panel_event_callbacks_t diagnostics_callbacks = {
        .on_vsync = diagnostics_rgb_vsync,
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(
        panel, &diagnostics_callbacks, NULL));
#endif
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
#ifdef DISPLAY_IPS
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
#endif
    lv_display_set_user_data(display, panel);
    lv_display_set_flush_cb(display, initial_rgb_flush);
    return display;
}

#endif
