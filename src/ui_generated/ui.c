#include <string.h>

#include "ui.h"
#include "screens.h"
#include "images.h"
#include "actions.h"
#include "vars.h"

static int16_t currentScreen = -1;

static lv_obj_t *getLvglObjectFromIndex(int32_t index) {
    if (index == -1) {
        return 0;
    }
    return ((lv_obj_t **)&objects)[index];
}

void loadScreen(enum ScreensEnum screenId) {
    currentScreen = screenId - 1;
    lv_obj_t *screen = getLvglObjectFromIndex(currentScreen);
    // Avoid a full-screen fade layer on this PSRAM-constrained display.
    // Navigation remains immediate and safe from touch callbacks.
    lv_screen_load(screen);
}

void ui_init(void) {
    create_screens();
    loadScreen(SCREEN_ID_MAIN_SCREEN);

}

void ui_tick(void) {
    tick_screen(currentScreen);
}