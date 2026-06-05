/* benchmarks.c - portable microbenchmarks.
 *
 * Anti-optimization discipline (so we measure what we think we measure):
 *   - accumulators are written through a volatile sink
 *   - a compiler memory barrier separates timing reads from work
 *   - buffers are sized modestly so they fit the ESP32-C3's 400 KB SRAM
 *
 * Build with -O2 (NOT -O3) on both targets for a fair, representative
 * comparison; record the exact flags in the report.
 */
#include "benchmarks.h"
#include "platform.h"

/* force the optimizer to keep a value live */
static volatile uint64_t g_sink;
#define BARRIER() __asm__ __volatile__("" ::: "memory")

/* ---- buffers sized to stay within embedded SRAM ---- */
#define COPY_BYTES   4096u
#define RAND_SLOTS   2048u      /* 8 KB of uint32 - bigger than nothing,    */
                                /* small enough for the C3; on PC this fits */
                                /* in L1/L2 so the contrast with DRAM-scale */
                                /* sweeps (host --bench) is the real story. */
static uint8_t  copy_src[COPY_BYTES];
static uint8_t  copy_dst[COPY_BYTES];
static uint32_t rand_buf[RAND_SLOTS];
static uint8_t  branch_data[4096];

static uint32_t lcg(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s; }

uint64_t bench_int_arith(uint32_t iters)
{
    uint64_t t0 = platform_cycles();
    BARRIER();
    uint32_t a = 1, b = 3, c = 7;
    for (uint32_t i = 0; i < iters; i++) {
        a = a * 31u + b;
        b = b + c + i;
        c = c ^ (a + 5u);
    }
    BARRIER();
    uint64_t t1 = platform_cycles();
    g_sink = a + b + c;
    return t1 - t0;
}

uint64_t bench_mem_copy(uint32_t bytes, uint32_t reps)
{
    if (bytes > COPY_BYTES) bytes = COPY_BYTES;
    for (uint32_t i = 0; i < bytes; i++) copy_src[i] = (uint8_t)i;

    uint64_t t0 = platform_cycles();
    BARRIER();
    for (uint32_t r = 0; r < reps; r++)
        for (uint32_t i = 0; i < bytes; i++)
            copy_dst[i] = copy_src[i];
    BARRIER();
    uint64_t t1 = platform_cycles();
    g_sink = copy_dst[bytes ? bytes - 1 : 0];
    return t1 - t0;
}

uint64_t bench_rand_access(uint32_t slots, uint32_t steps)
{
    if (slots > RAND_SLOTS) slots = RAND_SLOTS;
    /* build a random permutation cycle so each read depends on the last
     * (defeats prefetching - this is the cache-miss benchmark) */
    for (uint32_t i = 0; i < slots; i++) rand_buf[i] = i;
    uint32_t seed = 12345u;
    for (uint32_t i = slots - 1; i > 0; i--) {
        uint32_t j = lcg(&seed) % (i + 1);
        uint32_t t = rand_buf[i]; rand_buf[i] = rand_buf[j]; rand_buf[j] = t;
    }

    uint64_t t0 = platform_cycles();
    BARRIER();
    uint32_t idx = 0, acc = 0;
    for (uint32_t s = 0; s < steps; s++) {
        idx = rand_buf[idx];
        acc += idx;
    }
    BARRIER();
    uint64_t t1 = platform_cycles();
    g_sink = acc;
    return t1 - t0;
}

uint64_t bench_branches(uint32_t n, int predictable)
{
    if (n > sizeof(branch_data)) n = sizeof(branch_data);
    uint32_t seed = 999u;
    for (uint32_t i = 0; i < n; i++) {
        if (predictable)
            branch_data[i] = (i < n / 2) ? 1 : 0;          /* one taken run */
        else
            branch_data[i] = (uint8_t)(lcg(&seed) & 1);    /* coin flips    */
    }

    uint64_t t0 = platform_cycles();
    BARRIER();
    uint32_t hits = 0;
    for (uint32_t rep = 0; rep < 256; rep++)
        for (uint32_t i = 0; i < n; i++)
            if (branch_data[i]) hits += 3; else hits += 1;
    BARRIER();
    uint64_t t1 = platform_cycles();
    g_sink = hits;
    return t1 - t0;
}

uint64_t bench_bitops(uint32_t iters)
{
    uint64_t t0 = platform_cycles();
    BARRIER();
    uint32_t x = 0x12345678u;
    for (uint32_t i = 0; i < iters; i++) {
        x = (x << 13) | (x >> 19);     /* rotate */
        x ^= (x >> 7);
        x += 0x9E3779B9u;
        x &= 0xFFFFFFFFu;
    }
    BARRIER();
    uint64_t t1 = platform_cycles();
    g_sink = x;
    return t1 - t0;
}

void bench_run_all(bench_result_t *out)
{
    out->int_arith   = bench_int_arith(2000000u);
    out->mem_copy    = bench_mem_copy(COPY_BYTES, 256u);
    out->rand_access = bench_rand_access(RAND_SLOTS, 200000u);
    out->branches    = bench_branches(sizeof(branch_data), 0 /* random */);
    out->bitops      = bench_bitops(2000000u);
}
