/* st7789.c - ST7789 driver using ESP-IDF SPI master.
 *
 * Pin map (chosen to avoid the GPIO8/9 strapping pins):
 *   SCLK GPIO4   MOSI GPIO6   DC GPIO3   RST GPIO10   CS GPIO7   BLK 3.3V
 */
#include "st7789.h"
#include "font5x7.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define PIN_SCLK 4
#define PIN_MOSI 6
#define PIN_DC   3
#define PIN_RST  10
#define PIN_CS   7

#define LCD_W 240
#define LCD_H 240

static spi_device_handle_t s_spi;

static void dc(int level)  { gpio_set_level(PIN_DC, level); }

static void wr_cmd(uint8_t cmd)
{
    spi_transaction_t t = {0};
    t.length = 8;
    t.tx_buffer = &cmd;
    dc(0);
    spi_device_polling_transmit(s_spi, &t);
}

static void wr_data(const uint8_t *data, int len)
{
    if (len <= 0) return;
    spi_transaction_t t = {0};
    t.length = 8 * len;
    t.tx_buffer = data;
    dc(1);
    spi_device_polling_transmit(s_spi, &t);
}

static void wr_data8(uint8_t b) { wr_data(&b, 1); }

static void set_window(int x0, int y0, int x1, int y1)
{
    uint8_t buf[4];
    wr_cmd(0x2A);                         /* CASET */
    buf[0] = x0 >> 8; buf[1] = x0 & 0xFF;
    buf[2] = x1 >> 8; buf[3] = x1 & 0xFF;
    wr_data(buf, 4);
    wr_cmd(0x2B);                         /* RASET */
    buf[0] = y0 >> 8; buf[1] = y0 & 0xFF;
    buf[2] = y1 >> 8; buf[3] = y1 & 0xFF;
    wr_data(buf, 4);
    wr_cmd(0x2C);                         /* RAMWR */
}

void st7789_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_DC) | (1ULL << PIN_RST),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);

    spi_bus_config_t bus = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_W * 2 * 16,
    };
    spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 40 * 1000 * 1000,   /* 40 MHz */
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &dev, &s_spi);

    /* hardware reset */
    gpio_set_level(PIN_RST, 0); vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_RST, 1); vTaskDelay(pdMS_TO_TICKS(120));

    wr_cmd(0x01); vTaskDelay(pdMS_TO_TICKS(150));   /* SWRESET */
    wr_cmd(0x11); vTaskDelay(pdMS_TO_TICKS(120));   /* SLPOUT  */
    wr_cmd(0x3A); wr_data8(0x55);                   /* 16-bit  */
    wr_cmd(0x36); wr_data8(0x00);                   /* MADCTL  */
    wr_cmd(0x21);                                   /* INVON (ST7789 needs it) */
    wr_cmd(0x13);                                   /* NORON   */
    wr_cmd(0x29); vTaskDelay(pdMS_TO_TICKS(50));    /* DISPON  */
}

void st7789_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > LCD_W) w = LCD_W - x;
    if (y + h > LCD_H) h = LCD_H - y;
    if (w <= 0 || h <= 0) return;

    set_window(x, y, x + w - 1, y + h - 1);

    /* byte-swapped line buffer (ST7789 is big-endian over SPI) */
    static uint16_t line[LCD_W];
    uint16_t be = (uint16_t)((color << 8) | (color >> 8));
    for (int i = 0; i < w; i++) line[i] = be;

    dc(1);
    spi_transaction_t t = {0};
    for (int row = 0; row < h; row++) {
        t.length = 8 * 2 * w;
        t.tx_buffer = line;
        t.flags = 0;
        spi_device_polling_transmit(s_spi, &t);
    }
}

void st7789_draw_char(int x, int y, char ch, uint16_t fg, int scale)
{
    const uint8_t *g = font_glyph(ch);
    for (int row = 0; row < 7; row++) {
        uint8_t bits = g[row];
        for (int col = 0; col < 5; col++) {
            if (bits & (1 << (4 - col)))
                st7789_fill_rect(x + col * scale, y + row * scale,
                                 scale, scale, fg);
        }
    }
}

void st7789_draw_string(int x, int y, const char *s, uint16_t fg, int scale)
{
    int cx = x;
    for (; *s; s++) {
        st7789_draw_char(cx, y, *s, fg, scale);
        cx += 6 * scale;                 /* 5px glyph + 1px gap */
    }
}
