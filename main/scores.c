/* scores.c - recent-scores record list. No platform dependencies. */
#include "scores.h"

void scores_init(scores_t *s)
{
    for (int i = 0; i < SCORES_MAX; i++) s->score[i] = 0;
    s->count = 0;
}

/* Insert a new score at the front (most recent). Older entries shift down by
 * one; the oldest falls off when the list is full. */
void scores_add(scores_t *s, uint32_t value)
{
    for (int i = SCORES_MAX - 1; i > 0; i--)
        s->score[i] = s->score[i - 1];
    s->score[0] = value;
    if (s->count < SCORES_MAX)
        s->count++;
}

/* Delete the record at `index`, shifting every later entry up by one and
 * clearing the freed slot. This is the operation studied in the report:
 * on x86 it tends to compile to a tight rep-style move, on RV32 to an
 * explicit load/store loop. */
void scores_delete(scores_t *s, int index)
{
    if (index < 0 || index >= s->count)
        return;
    for (int i = index; i < s->count - 1; i++)
        s->score[i] = s->score[i + 1];
    s->count--;
    s->score[s->count] = 0;
}
