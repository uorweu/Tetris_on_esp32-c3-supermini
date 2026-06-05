/* benchmarks.h - the five microbenchmarks run on both architectures.
 * Each returns elapsed CPU cycles (via platform_cycles), so results are
 * directly comparable in the report's Chapter 5.5 table. */
#ifndef BENCHMARKS_H
#define BENCHMARKS_H

#include <stdint.h>

typedef struct {
    uint64_t int_arith;     /* integer ALU throughput        */
    uint64_t mem_copy;      /* memcpy, cache bandwidth        */
    uint64_t rand_access;   /* pointer chase, cache misses    */
    uint64_t branches;      /* branch-predictor stress        */
    uint64_t bitops;        /* bit manipulation / ISA richness*/
} bench_result_t;

/* Run all five and fill `out`. Safe to call on either target. */
void bench_run_all(bench_result_t *out);

/* Individual benchmarks (exposed so the host CSV mode can sweep sizes). */
uint64_t bench_int_arith(uint32_t iters);
uint64_t bench_mem_copy(uint32_t bytes, uint32_t reps);
uint64_t bench_rand_access(uint32_t slots, uint32_t steps);
uint64_t bench_branches(uint32_t n, int predictable);
uint64_t bench_bitops(uint32_t iters);

#endif /* BENCHMARKS_H */
