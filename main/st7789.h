/* st7789.h - minimal ST7789 240x240 SPI driver for the ESP32-C3. */
#ifndef ST7789_H
#define ST7789_H

#include <stdint.h>

void     st7789_init(void);
void     st7789_fill_rect(int x, int y, int w, int h, uint16_t color565);
/* draw one glyph using the shared font; scale >=1 */
void     st7789_draw_char(int x, int y, char ch, uint16_t fg, int scale);
void     st7789_draw_string(int x, int y, const char *s, uint16_t fg, int scale);

#endif
