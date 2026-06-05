/* platform.h - the boundary between architecture-independent game/benchmark
 * code and the two targets (ESP32-C3 RISC-V, and x86-64 host).
 *
 * RULE: nothing in common/ may include a target-specific header. All I/O,
 * timing, and drawing goes through this interface. Each target provides its
 * own implementation. This is what guarantees the SAME C source compiles for
 * both architectures, so every measured difference is attributable to the ISA.
 */
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include "scores.h"

/* Logical color indices. Each target maps these to its native pixel format
 * (RGB565 on ST7789, RGB888 on SDL). */
enum {
    COLOR_BG     = 0,
    COLOR_I      = 1,   /* cyan   */
    COLOR_O      = 2,   /* yellow */
    COLOR_T      = 3,   /* purple */
    COLOR_S      = 4,   /* green  */
    COLOR_Z      = 5,   /* red    */
    COLOR_J      = 6,   /* blue   */
    COLOR_L      = 7,   /* orange */
    COLOR_WHITE  = 8,
    COLOR_GRAY   = 9,
    COLOR_BORDER = 10,
    COLOR_COUNT  = 11
};

/* Input is pre-thresholded by the platform: directions are simple booleans,
 * already past the analog dead-zone on the joystick target. Edge detection
 * (e.g. "rotate once per press") is handled in the portable core. */
typedef struct {
    uint8_t left;
    uint8_t right;
    uint8_t up;
    uint8_t down;
    uint8_t action;   /* hard drop (button 2 or joystick click) */
    uint8_t menu;     /* opens the benchmark screen             */
    uint8_t newgame;  /* start a fresh game (button 1)          */
} input_state_t;

/* Lifecycle */
void     platform_init(void);

/* Drawing - pixel coordinates, origin top-left, 240x240 logical surface.
 * The portable core owns all layout; the platform only knows how to fill
 * rectangles and stamp text. flush() presents the frame (no-op where the
 * driver draws directly to the panel). */
void     platform_clear(int color);
void     platform_fill_rect(int x, int y, int w, int h, int color);
void     platform_draw_text(int x, int y, const char *s, int color, int scale);
void     platform_flush(void);

/* Input */
void     platform_get_input(input_state_t *out);

/* Timing & measurement */
uint64_t platform_cycles(void);    /* free-running CPU cycle counter        */
uint32_t platform_millis(void);    /* monotonic milliseconds                */
void     platform_delay(uint32_t ms);

/* Host can ask to quit (window closed); embedded target always returns 0. */
int      platform_should_quit(void);

/* Persistent recent-scores storage (NVS flash on the C3, a file on the host).
 * load fills `s` (or initializes it empty if nothing is stored yet). */
void     platform_scores_load(scores_t *s);
void     platform_scores_save(const scores_t *s);

#endif /* PLATFORM_H */
