/* cycles_rv32.h - CPU cycle counter for the ESP32-C3 (RISC-V).
 *
 * NOTE: we use Espressif's esp_cpu_get_cycle_count() rather than reading the
 * mcycle/mcycleh CSRs directly. The ESP32-C3's RISC-V core does not expose the
 * standard 64-bit mcycleh CSR, so a direct `csrr mcycleh` faults with an
 * illegal-instruction panic. esp_cpu_get_cycle_count() returns the 32-bit
 * cycle count, which is plenty: per-frame and per-benchmark intervals are far
 * shorter than the ~27 s it takes to wrap at 160 MHz.
 */
#ifndef CYCLES_RV32_H
#define CYCLES_RV32_H

#include <stdint.h>
#include "esp_cpu.h"

static inline uint64_t rv32_cycles(void)
{
    return (uint64_t)esp_cpu_get_cycle_count();
}

#endif /* CYCLES_RV32_H */
