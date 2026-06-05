/* app.c - the portable game + benchmark front-end.
 *
 * Owns ALL layout. The platform only fills rectangles and stamps text, so
 * this single file drives both the ST7789 panel and the SDL window
 * identically. Drawing is diff-based: only cells that changed since last
 * frame are repainted (no flicker, minimal SPI traffic).
 */
#include "app.h"
#include "platform.h"
#include "tetris_core.h"
#include "benchmarks.h"
#include "scores.h"
#include <stdio.h>
#include <string.h>

/* ---- screen layout (240 x 240) ---- */
#define CELL    11
#define OX      4                      /* playfield origin x */
#define OY      8                      /* playfield origin y */
#define PFW     (FIELD_W * CELL)       /* 110 */
#define PFH     (FIELD_H * CELL)       /* 220 */
#define PANEL_X (OX + PFW + 8)         /* 122 */

/* ---- input edge / auto-repeat tuning ---- */
#define DAS_MS      160                /* delay before horizontal auto-repeat */
#define ARR_MS      45                 /* auto-repeat rate                     */
#define SOFT_MS     40                 /* soft-drop step                       */

static int piece_color(int piece) { return piece + 1; }

/* composite field + active piece into a 10x20 color grid */
static void build_grid(const game_t *g, uint8_t grid[FIELD_H][FIELD_W])
{
    memcpy(grid, g->field, sizeof(g->field));
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (piece_cell(g->cur_piece, g->cur_rot, r, c)) {
                int bx = g->cur_x + c, by = g->cur_y + r;
                if (by >= 0 && by < FIELD_H && bx >= 0 && bx < FIELD_W)
                    grid[by][bx] = (uint8_t)piece_color(g->cur_piece);
            }
}

static void draw_cell(int gx, int gy, int color)
{
    int px = OX + gx * CELL, py = OY + gy * CELL;
    /* body */
    platform_fill_rect(px, py, CELL, CELL,
                       color ? color : COLOR_BG);
    if (color) {
        /* 1px inner border for a beveled look */
        platform_fill_rect(px, py, CELL, 1, COLOR_BG);
        platform_fill_rect(px, py, 1, CELL, COLOR_BG);
    }
}

static void draw_static_chrome(void)
{
    platform_clear(COLOR_BG);
    /* playfield border */
    platform_fill_rect(OX - 2, OY - 2, PFW + 4, 2, COLOR_BORDER);
    platform_fill_rect(OX - 2, OY + PFH, PFW + 4, 2, COLOR_BORDER);
    platform_fill_rect(OX - 2, OY - 2, 2, PFH + 4, COLOR_BORDER);
    platform_fill_rect(OX + PFW, OY - 2, 2, PFH + 4, COLOR_BORDER);

    platform_draw_text(PANEL_X, 8,  "TETRIS", COLOR_WHITE, 2);
    platform_draw_text(PANEL_X, 30, "NEXT",   COLOR_GRAY,  2);
    platform_draw_text(PANEL_X, 92, "SCORE",  COLOR_GRAY,  2);
    platform_draw_text(PANEL_X, 122,"LEVEL",  COLOR_GRAY,  2);
    platform_draw_text(PANEL_X, 152,"LINES",  COLOR_GRAY,  2);
    platform_draw_text(PANEL_X, 188,"CYC/T",  COLOR_GRAY,  2);
    platform_draw_text(PANEL_X, 214,"FPS",    COLOR_GRAY,  2);
}

static void draw_value(int y, uint32_t v, int color)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)v);
    platform_fill_rect(PANEL_X, y, 240 - PANEL_X, 16, COLOR_BG);
    platform_draw_text(PANEL_X, y, buf, color, 2);
}

static void draw_next(int piece)
{
    /* clear the small preview box */
    platform_fill_rect(PANEL_X, 46, 4 * 9 + 2, 4 * 9 + 2, COLOR_BG);
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (piece_cell(piece, 0, r, c))
                platform_fill_rect(PANEL_X + c * 9, 46 + r * 9, 8, 8,
                                   piece_color(piece));
}

/* ---------- benchmark screen ---------- */
static void show_benchmarks(void)
{
    platform_clear(COLOR_BG);
    platform_draw_text(8, 8, "BENCHMARKS", COLOR_WHITE, 2);
    platform_draw_text(8, 30, "RUNNING", COLOR_GRAY, 2);
    platform_flush();

    bench_result_t br;
    bench_run_all(&br);

    platform_clear(COLOR_BG);
    platform_draw_text(8, 6, "BENCH CYCLES", COLOR_WHITE, 2);

    const char *labels[5] = {"ARITH", "COPY", "RAND", "BRANCH", "BITOPS"};
    uint64_t vals[5] = {br.int_arith, br.mem_copy, br.rand_access,
                        br.branches, br.bitops};
    char buf[20];
    for (int i = 0; i < 5; i++) {
        int y = 34 + i * 30;
        platform_draw_text(8, y, labels[i], COLOR_GRAY, 2);
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)vals[i]);
        platform_draw_text(8, y + 14, buf, COLOR_WHITE, 2);
    }
    platform_draw_text(8, 200, "PRESS TO EXIT", COLOR_GRAY, 1);
    platform_flush();

    /* wait for a fresh press to leave */
    input_state_t in;
    do { platform_get_input(&in); platform_delay(30); } while (in.action);
    do { platform_get_input(&in); platform_delay(30);
         if (platform_should_quit()) return; } while (!in.action);
    do { platform_get_input(&in); platform_delay(30); } while (in.action);
}

/* ---------- game over screen: final score + recent-5 list + delete ------- */
static void draw_gameover_screen(const game_t *g, const scores_t *hs, int sel)
{
    char buf[20];
    platform_clear(COLOR_BG);
    platform_draw_text(18, 14, "GAME OVER", COLOR_Z, 3);

    platform_draw_text(18, 58, "SCORE", COLOR_GRAY, 2);
    snprintf(buf, sizeof buf, "%lu", (unsigned long)g->score);
    platform_draw_text(104, 58, buf, COLOR_WHITE, 2);

    platform_draw_text(18, 88, "RECENT", COLOR_GRAY, 2);
    if (hs->count == 0) {
        platform_draw_text(18, 112, "NONE", COLOR_GRAY, 2);
    } else {
        for (int i = 0; i < hs->count; i++) {
            int y = 112 + i * 22;
            int col = (i == sel) ? COLOR_WHITE : COLOR_GRAY;
            platform_draw_text(18, y, (i == sel) ? ">" : " ", col, 2);
            snprintf(buf, sizeof buf, "%lu", (unsigned long)hs->score[i]);
            platform_draw_text(42, y, buf, col, 2);
        }
    }

    platform_draw_text(6, 224, "B1 NEW  UP/DN SEL", COLOR_GRAY, 1);
    platform_draw_text(6, 234, "B2 DEL  B3 BENCH", COLOR_GRAY, 1);
}

/* Blocking screen shown when the game ends. Returns when the player starts a
 * new game (button 1) or quits. Handles deleting a stored record and opening
 * the benchmark screen. */
static void show_gameover(const game_t *g, scores_t *hs)
{
    input_state_t in, pin;
    platform_get_input(&pin);          /* baseline so we require fresh presses */
    int sel = 0, redraw = 1;

    for (;;) {
        platform_get_input(&in);
        if (platform_should_quit()) return;

        if (in.newgame && !pin.newgame) return;            /* start new game   */

        if (in.menu && !pin.menu) {                        /* benchmark screen */
            show_benchmarks();
            redraw = 1;
        }
        if (hs->count > 0) {
            if (in.up && !pin.up)   { sel = (sel - 1 + hs->count) % hs->count; redraw = 1; }
            if (in.down && !pin.down) { sel = (sel + 1) % hs->count; redraw = 1; }
            if (in.action && !pin.action) {                /* delete selected  */
                scores_delete(hs, sel);
                platform_scores_save(hs);
                if (sel >= hs->count && sel > 0) sel--;
                redraw = 1;
            }
        }

        if (redraw) { draw_gameover_screen(g, hs, sel); platform_flush(); redraw = 0; }
        pin = in;
        platform_delay(40);
    }
}

void app_run(void)
{
    game_t g;
    game_init(&g, 0x1234abcdu);

    scores_t hs;
    platform_scores_load(&hs);

    uint8_t prev[FIELD_H][FIELD_W];
    uint8_t cur[FIELD_H][FIELD_W];
    memset(prev, 0xFF, sizeof(prev));  /* force full first paint */

    draw_static_chrome();

    int last_piece_shown = -1;
    uint32_t shown_score = 0xFFFFFFFFu, shown_level = 0, shown_lines = 0;

    /* input edge state */
    input_state_t in, pin = {0};
    uint32_t das_t = 0, arr_t = 0, soft_t = 0;

    /* perf measurement */
    uint64_t logic_cyc = 0;
    uint32_t shown_cyc = 0xFFFFFFFFu, shown_fps = 0xFFFFFFFF;
    uint32_t frames = 0, fps = 0, fps_window_start = platform_millis();
    uint32_t stat_tick = platform_millis();

    while (!platform_should_quit()) {
        uint32_t now = platform_millis();
        platform_get_input(&in);

        uint64_t c0 = platform_cycles();   /* ---- measure game logic ---- */

        /* button 1: start a fresh game at any time */
        if (in.newgame && !pin.newgame) {
            game_init(&g, g.rng ^ now);
            draw_static_chrome();
            memset(prev, 0xFF, sizeof(prev));
            last_piece_shown = -1;
            shown_score = shown_level = shown_lines = 0xFFFFFFFFu;
            shown_cyc = shown_fps = 0xFFFFFFFFu;
            platform_get_input(&pin);
            platform_delay(16);
            continue;
        }

        if (!g.game_over) {
            /* rotate: edge on up */
            if (in.up && !pin.up) {
                int nr = (g.cur_rot + 1) & 3;
                if (!check_collision(&g, g.cur_piece, nr, g.cur_x, g.cur_y))
                    g.cur_rot = nr;
                else if (!check_collision(&g, g.cur_piece, nr, g.cur_x - 1, g.cur_y))
                    { g.cur_x--; g.cur_rot = nr; }         /* simple wall kick */
                else if (!check_collision(&g, g.cur_piece, nr, g.cur_x + 1, g.cur_y))
                    { g.cur_x++; g.cur_rot = nr; }
            }

            /* horizontal move with DAS/ARR */
            int dir = in.right ? 1 : (in.left ? -1 : 0);
            int pdir = pin.right ? 1 : (pin.left ? -1 : 0);
            if (dir && dir != pdir) {                       /* fresh press */
                if (!check_collision(&g, g.cur_piece, g.cur_rot, g.cur_x + dir, g.cur_y))
                    g.cur_x += dir;
                das_t = now; arr_t = now;
            } else if (dir && dir == pdir) {                /* held */
                if (now - das_t >= DAS_MS && now - arr_t >= ARR_MS) {
                    if (!check_collision(&g, g.cur_piece, g.cur_rot, g.cur_x + dir, g.cur_y))
                        g.cur_x += dir;
                    arr_t = now;
                }
            }

            /* soft drop */
            if (in.down && now - soft_t >= SOFT_MS) {
                if (!check_collision(&g, g.cur_piece, g.cur_rot, g.cur_x, g.cur_y + 1)) {
                    g.cur_y++;
                    g.score += 1;
                }
                soft_t = now;
            }

            /* action button / joystick click: hard drop on press edge */
            if (in.action && !pin.action) {
                while (!check_collision(&g, g.cur_piece, g.cur_rot,
                                        g.cur_x, g.cur_y + 1)) {
                    g.cur_y++; g.score += 2;
                }
                game_lock(&g);
            }

            /* menu button: open the benchmark screen on press edge */
            if (in.menu && !pin.menu) {
                show_benchmarks();
                draw_static_chrome();
                memset(prev, 0xFF, sizeof(prev));
                last_piece_shown = -1;
                shown_score = shown_level = shown_lines = 0xFFFFFFFFu;
                shown_cyc = shown_fps = 0xFFFFFFFFu;
            }

            game_step_gravity(&g, now);
        }

        uint64_t c1 = platform_cycles();
        logic_cyc = c1 - c0;             /* ---- end measure ---- */

        /* on game over: record the score, then show the recent-scores screen */
        if (g.game_over) {
            scores_add(&hs, g.score);
            platform_scores_save(&hs);
            show_gameover(&g, &hs);
            if (platform_should_quit()) break;
            game_init(&g, g.rng ^ now);
            draw_static_chrome();
            memset(prev, 0xFF, sizeof(prev));
            last_piece_shown = -1;
            shown_score = shown_level = shown_lines = 0xFFFFFFFFu;
            shown_cyc = shown_fps = 0xFFFFFFFFu;
            platform_get_input(&pin);
            continue;
        }

        /* ---- render (diff-based) ---- */
        build_grid(&g, cur);
        for (int r = 0; r < FIELD_H; r++)
            for (int c = 0; c < FIELD_W; c++)
                if (cur[r][c] != prev[r][c]) {
                    draw_cell(c, r, cur[r][c]);
                    prev[r][c] = cur[r][c];
                }

        if (g.next_piece != last_piece_shown) {
            draw_next(g.next_piece);
            last_piece_shown = g.next_piece;
        }
        if (g.score != shown_score) { draw_value(106, g.score, COLOR_WHITE); shown_score = g.score; }
        if (g.level != shown_level) { draw_value(136, g.level, COLOR_WHITE); shown_level = g.level; }
        if (g.lines != shown_lines) { draw_value(166, g.lines, COLOR_WHITE); shown_lines = g.lines; }

        /* throttle the perf readouts to ~4 Hz so they stay readable */
        frames++;
        if (now - fps_window_start >= 1000) {
            fps = frames * 1000 / (now - fps_window_start);
            frames = 0; fps_window_start = now;
        }
        if (now - stat_tick >= 250) {
            uint32_t lc = (uint32_t)logic_cyc;
            if (lc != shown_cyc) { draw_value(202, lc, COLOR_I); shown_cyc = lc; }
            if (fps != shown_fps) { draw_value(228, fps, COLOR_I); shown_fps = fps; }
            stat_tick = now;
        }

        platform_flush();
        pin = in;
        platform_delay(16);   /* ~60 Hz cap */
    }
}
