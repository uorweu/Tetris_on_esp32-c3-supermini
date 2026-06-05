/* tetris_core.c - portable Tetris logic. No platform headers allowed here. */
#include "tetris_core.h"

/* 7 tetrominoes x 4 rotations, each a 4x4 bitmask.
 * Bit n = (row*4 + col); bit set = filled cell.
 * Piece order: 0=I 1=O 2=T 3=S 4=Z 5=J 6=L  (color = piece + 1). */
static const uint16_t PIECES[7][4] = {
    /* I */ {0x00F0, 0x4444, 0x0F00, 0x2222},
    /* O */ {0x0066, 0x0066, 0x0066, 0x0066},
    /* T */ {0x0072, 0x0262, 0x0270, 0x0232},
    /* S */ {0x0036, 0x0462, 0x0360, 0x0231},
    /* Z */ {0x0063, 0x0264, 0x0630, 0x0132},
    /* J */ {0x0071, 0x0226, 0x0470, 0x0322},
    /* L */ {0x0074, 0x0622, 0x0170, 0x0223},
};

int piece_cell(int piece, int rot, int r, int c)
{
    return (PIECES[piece][rot] >> (r * 4 + c)) & 1;
}

int check_collision(const game_t *g, int piece, int rot, int x, int y)
{
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!piece_cell(piece, rot, r, c))
                continue;
            int bx = x + c;
            int by = y + r;
            if (bx < 0 || bx >= FIELD_W) return 1;        /* wall  */
            if (by >= FIELD_H)           return 1;         /* floor */
            if (by >= 0 && g->field[by][bx]) return 1;     /* block */
        }
    }
    return 0;
}

int clear_lines(game_t *g)
{
    int cleared = 0;
    for (int r = FIELD_H - 1; r >= 0; r--) {
        int full = 1;
        for (int c = 0; c < FIELD_W; c++) {
            if (!g->field[r][c]) { full = 0; break; }
        }
        if (full) {
            for (int rr = r; rr > 0; rr--)
                for (int c = 0; c < FIELD_W; c++)
                    g->field[rr][c] = g->field[rr - 1][c];
            for (int c = 0; c < FIELD_W; c++)
                g->field[0][c] = 0;
            cleared++;
            r++; /* re-examine this row after the shift */
        }
    }
    return cleared;
}

uint32_t score_for_lines(int n, uint32_t level)
{
    uint32_t base;
    switch (n) {
        case 1:  base = 100;  break;
        case 2:  base = 300;  break;
        case 3:  base = 500;  break;
        case 4:  base = 800;  break;
        default: base = 0;    break;
    }
    return base * (level + 1);
}

/* xorshift32: a few shifts and xors - shows the ISA's bit-manipulation ops. */
static uint32_t xorshift32(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

static void shuffle_bag(game_t *g)
{
    for (int i = 0; i < 7; i++) g->bag[i] = (uint8_t)i;
    for (int i = 6; i > 0; i--) {
        int j = (int)(xorshift32(&g->rng) % (uint32_t)(i + 1));
        uint8_t t = g->bag[i]; g->bag[i] = g->bag[j]; g->bag[j] = t;
    }
    g->bag_idx = 0;
}

int bag_next(game_t *g)
{
    if (g->bag_idx >= 7) shuffle_bag(g);
    return g->bag[g->bag_idx++];
}

void game_spawn(game_t *g)
{
    g->cur_piece = g->next_piece;
    g->next_piece = bag_next(g);
    g->cur_rot = 0;
    g->cur_x = 3;
    g->cur_y = 0;
    if (check_collision(g, g->cur_piece, g->cur_rot, g->cur_x, g->cur_y))
        g->game_over = 1;
}

void game_lock(game_t *g)
{
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (piece_cell(g->cur_piece, g->cur_rot, r, c)) {
                int bx = g->cur_x + c, by = g->cur_y + r;
                if (by >= 0 && by < FIELD_H && bx >= 0 && bx < FIELD_W)
                    g->field[by][bx] = (uint8_t)(g->cur_piece + 1);
            }

    int n = clear_lines(g);
    if (n) {
        g->lines += (uint32_t)n;
        g->score += score_for_lines(n, g->level);
        g->level = 1 + g->lines / 10;
        uint32_t gm = (g->level < 16) ? (800 - (g->level - 1) * 45) : 100;
        if (gm < 100) gm = 100;
        g->gravity_ms = gm;
    }
    game_spawn(g);
}

int game_step_gravity(game_t *g, uint32_t now_ms)
{
    if (g->game_over) return 0;
    if (now_ms - g->last_drop_ms < g->gravity_ms) return 0;
    g->last_drop_ms = now_ms;

    if (!check_collision(g, g->cur_piece, g->cur_rot, g->cur_x, g->cur_y + 1)) {
        g->cur_y++;
        return 0;
    }
    game_lock(g);
    return 1;
}

void game_init(game_t *g, uint32_t seed)
{
    for (int r = 0; r < FIELD_H; r++)
        for (int c = 0; c < FIELD_W; c++)
            g->field[r][c] = 0;
    g->rng = seed ? seed : 0xC0FFEEu;
    g->bag_idx = 7;             /* force a shuffle on first draw */
    g->score = 0;
    g->lines = 0;
    g->level = 1;
    g->game_over = 0;
    g->gravity_ms = 800;
    g->last_drop_ms = 0;
    g->next_piece = bag_next(g);
    game_spawn(g);
}
