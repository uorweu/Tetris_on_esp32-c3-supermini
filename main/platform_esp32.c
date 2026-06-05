/* platform_esp32.c - implements platform.h for the ESP32-C3 (RISC-V).
 * Maps the portable code's logical colors to RGB565 and forwards drawing
 * to the ST7789 driver. Drawing is direct-to-panel, so flush() is a no-op.
 */
#include "platform.h"
#include "st7789.h"
#include "input_joystick.h"
#include "cycles_rv32.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const uint16_t PAL[COLOR_COUNT] = {
    [COLOR_BG]     = 0x0000,   /* black  */
    [COLOR_I]      = 0x07FF,   /* cyan   */
    [COLOR_O]      = 0xFFE0,   /* yellow */
    [COLOR_T]      = 0xA01F,   /* purple */
    [COLOR_S]      = 0x07E0,   /* green  */
    [COLOR_Z]      = 0xF800,   /* red    */
    [COLOR_J]      = 0x001F,   /* blue   */
    [COLOR_L]      = 0xFD20,   /* orange */
    [COLOR_WHITE]  = 0xFFFF,
    [COLOR_GRAY]   = 0x8410,
    [COLOR_BORDER] = 0x4208,
};

void platform_init(void)
{
    /* NVS holds the recent-scores list across power cycles */
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    st7789_init();
    joystick_init();
}

void platform_clear(int color)
{
    st7789_fill_rect(0, 0, 240, 240, PAL[color]);
}

void platform_fill_rect(int x, int y, int w, int h, int color)
{
    st7789_fill_rect(x, y, w, h, PAL[color]);
}

void platform_draw_text(int x, int y, const char *s, int color, int scale)
{
    st7789_draw_string(x, y, s, PAL[color], scale);
}

void platform_flush(void) { /* direct draw - nothing to do */ }

void platform_get_input(input_state_t *out)
{
    joystick_read(out);
}

uint64_t platform_cycles(void) { return rv32_cycles(); }

uint32_t platform_millis(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

void platform_delay(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms ? ms : 1)); }

int platform_should_quit(void) { return 0; }

void platform_scores_load(scores_t *s)
{
    scores_init(s);
    nvs_handle_t h;
    if (nvs_open("game", NVS_READONLY, &h) == ESP_OK) {
        size_t len = sizeof(*s);
        if (nvs_get_blob(h, "scores", s, &len) != ESP_OK || len != sizeof(*s))
            scores_init(s);
        if (s->count < 0 || s->count > SCORES_MAX)
            scores_init(s);
        nvs_close(h);
    }
}

void platform_scores_save(const scores_t *s)
{
    nvs_handle_t h;
    if (nvs_open("game", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, "scores", s, sizeof(*s));
        nvs_commit(h);
        nvs_close(h);
    }
}
