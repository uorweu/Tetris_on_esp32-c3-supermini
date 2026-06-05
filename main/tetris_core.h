/* tetris_core.h - architecture-independent Tetris logic.
 *
 * The functions declared here are the SPECIMENS dissected in Chapter 5.2 of
 * the report (the same-code-different-assembly comparison). Keep them free of
 * any platform dependency so `gcc -S` and `riscv32-esp-elf-gcc -S` see
 * identical input.
 */
#ifndef TETRIS_CORE_H
#define TETRIS_CORE_H

#include <stdint.h>

#define FIELD_W 10
#define FIELD_H 20

typedef struct {
    uint8_t  field[FIELD_H][FIELD_W];  /* 0 empty, 1..7 = locked color   */
    int      cur_piece;                /* 0..6                            */
    int      cur_rot;                  /* 0..3                            */
    int      cur_x, cur_y;             /* top-left of the 4x4 piece box   */
    int      next_piece;
    uint32_t score;
    uint32_t lines;
    uint32_t level;
    int      game_over;

    uint8_t  bag[7];                   /* 7-bag randomizer                */
    int      bag_idx;
    uint32_t rng;                      /* xorshift32 state                */

    uint32_t gravity_ms;               /* current fall interval           */
    uint32_t last_drop_ms;
} game_t;

/* --- functions compared in the assembly study (Chapter 5.2) --- */

/* 1 if cell (r,c) of the 4x4 box is solid for this piece/rotation. */
int  piece_cell(int piece, int rot, int r, int c);

/* 1 if placing `piece` at (x,y) in rotation `rot` collides with a wall,
 * the floor, or a locked block. Exercises nested loops + memory reads. */
int  check_collision(const game_t *g, int piece, int rot, int x, int y);

/* Scan from the bottom, remove full rows, shift everything down.
 * Returns the number of rows cleared. Exercises memory copies. */
int  clear_lines(game_t *g);

/* Classic scoring table. Compiled to a jump table on x86, a branch ladder
 * (or compressed branches) on RV32 - a clean switch-statement comparison. */
uint32_t score_for_lines(int n, uint32_t level);

/* Draw the next piece from the shuffled 7-bag. PRNG = bit ops (xor/shift). */
int  bag_next(game_t *g);

/* --- ordinary game API --- */
void game_init(game_t *g, uint32_t seed);
void game_spawn(game_t *g);
void game_lock(game_t *g);
int  game_step_gravity(game_t *g, uint32_t now_ms); /* 1 if a piece locked */

#endif /* TETRIS_CORE_H */
