/* scores.h - a small fixed-size record list of recent scores.
 *
 * Architecture-independent on purpose: scores_add and scores_delete are pure
 * memory-shuffling operations, so they make clean specimens for the RISC-vs-
 * CISC assembly comparison (Chapter 5.2) - especially scores_delete, which is
 * a classic "remove a record and shift the rest up" pattern.
 */
#ifndef SCORES_H
#define SCORES_H

#include <stdint.h>

#define SCORES_MAX 5

typedef struct {
    uint32_t score[SCORES_MAX];   /* newest first */
    int      count;               /* number of valid entries, 0..SCORES_MAX */
} scores_t;

void scores_init(scores_t *s);
void scores_add(scores_t *s, uint32_t value);   /* push newest, drop oldest */
void scores_delete(scores_t *s, int index);     /* remove one, shift up      */

#endif /* SCORES_H */
