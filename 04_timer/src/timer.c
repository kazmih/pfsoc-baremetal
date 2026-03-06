/*==============================================================================
 * timer.c — RISC-V CLINT timer driver
 *
 * THE BIG PICTURE BEFORE READING THE CODE:
 *
 * The CLINT (Core Local Interruptor) is a block of hardware that:
 *   1. Contains a 64-bit counter called mtime that ticks at 1MHz (1 tick/µs)
 *   2. Contains a 64-bit compare register called mtimecmp
 *   3. When mtime >= mtimecmp, it asserts the timer interrupt signal to the CPU
 *
 * To create a 1ms periodic interrupt:
 *   - Read mtime (say it's 500000)
 *   - Write mtimecmp = 500000 + 1000  (1000 ticks = 1ms at 1MHz)
 *   - Enable interrupts
 *   - At t=501000, interrupt fires
 *   - In the interrupt handler: write mtimecmp = current_mtime + 1000
 *   - Repeat forever
 *
 * RISC-V INTERRUPT ENABLE CHAIN:
 * Three things must ALL be true for a timer interrupt to reach the CPU:
 *
 *   1. mstatus.MIE = 1  (global interrupt enable — the master switch)
 *   2. mie.MTIE = 1     (machine timer interrupt enabled specifically)
 *   3. mtime >= mtimecmp (the hardware condition is met)
 *
 * If ANY of these is false, the interrupt is blocked.
 *
 * CSR REGISTERS:
 * mstatus, mie, mtvec, mepc, mcause are not memory-mapped registers.
 * They are special registers inside the CPU core itself, accessed via
 * dedicated RISC-V instructions: csrr (read) and csrw (write).
 *============================================================================*/
#include "timer.h"
#include "../hal/hw_reg_access.h"

/*──────────────────────────────────────────────────────────────────────────────
 * Private state
 *──────────────────────────────────────────────────────────────────────────────*/
static timer_callback_t  g_timer_callback = (void*)0;
static volatile uint32_t g_tick_count     = 0U;
static uint32_t          g_interval_ticks = 0U;

/*──────────────────────────────────────────────────────────────────────────────
 * CSR access macros
 *
 * These use GCC inline assembly to emit RISC-V CSR instructions.
 * You cannot access CSRs with normal C pointer dereferences because
 * they are not part of the memory address space — they are CPU-internal.
 *
 * csrr rd, csr   → read CSR into register rd
 * csrw csr, rs1  → write register rs1 into CSR
 * csrs csr, rs1  → set bits in CSR (CSR |= rs1)
 * csrc csr, rs1  → clear bits in CSR (CSR &= ~rs1)
 *──────────────────────────────────────────────────────────────────────────────*/
#define CSR_READ(reg, val)   __asm__ volatile("csrr %0, " #reg : "=r"(val))
#define CSR_WRITE(reg, val)  __asm__ volatile("csrw " #reg ", %0" :: "r"(val))
#define CSR_SET(reg, bits)   __asm__ volatile("csrs " #reg ", %0" :: "r"(bits))
#define CSR_CLEAR(reg, bits) __asm__ volatile("csrc " #reg ", %0" :: "r"(bits))

/*──────────────────────────────────────────────────────────────────────────────
 * timer_get_mtime()
 *
 * Reads the 64-bit mtime counter from the CLINT.
 *
 * WHY THE LOOP?
 * mtime is 64 bits but our CPU reads 32 bits at a time.
 * Between reading the low 32 bits and the high 32 bits, the counter
 * might roll over (carry from bit 31 to bit 32).
 *
 * Example of the problem:
 *   mtime = 0x00000001_FFFFFFFF
 *   We read HIGH first: 0x00000001
 *   Counter ticks:      0x00000002_00000000
 *   We read LOW:        0x00000000
 *   Combined: 0x00000001_00000000 ← WRONG! Off by almost 4 billion!
 *
 * Solution: read HIGH, read LOW, read HIGH again.
 * If HIGH changed, a rollover happened → retry.
 *──────────────────────────────────────────────────────────────────────────────*/
uint64_t timer_get_mtime(void)
{
    uint32_t hi, lo, hi2;
    do {
        hi  = HW_REG32(CLINT_BASE, CLINT_MTIME_HI);
        lo  = HW_REG32(CLINT_BASE, CLINT_MTIME_LO);
        hi2 = HW_REG32(CLINT_BASE, CLINT_MTIME_HI);
    } while (hi != hi2);

    return ((uint64_t)hi << 32U) | (uint64_t)lo;
}

/*──────────────────────────────────────────────────────────────────────────────
 * set_mtimecmp() — arm the next timer interrupt
 *
 * WHY WRITE HIGH WORD FIRST with 0xFFFFFFFF?
 * mtimecmp is 64-bit, written as two 32-bit stores.
 * If mtime is currently, say, 0x00000001_00000050 and we want to set
 * mtimecmp to 0x00000001_000003E8:
 *
 * WRONG order (low first):
 *   Write mtimecmp_lo = 0x000003E8
 *   → mtimecmp is now 0x00000000_000003E8 temporarily (old hi, new lo)
 *   → if mtime > this value, SPURIOUS INTERRUPT fires!
 *   Write mtimecmp_hi = 0x00000001
 *   → now correct
 *
 * CORRECT order (set hi to max first, then write lo, then correct hi):
 *   Write mtimecmp_hi = 0xFFFFFFFF  → mtimecmp very large, no interrupt
 *   Write mtimecmp_lo = new_lo      → set correct low
 *   Write mtimecmp_hi = new_hi      → set correct high
 *──────────────────────────────────────────────────────────────────────────────*/
static void set_mtimecmp(uint64_t value)
{
    HW_REG32(CLINT_BASE, CLINT_MTIMECMP_HI) = 0xFFFFFFFFU;
    HW_REG32(CLINT_BASE, CLINT_MTIMECMP_LO) = (uint32_t)(value & 0xFFFFFFFFU);
    HW_REG32(CLINT_BASE, CLINT_MTIMECMP_HI) = (uint32_t)(value >> 32U);
}

/*──────────────────────────────────────────────────────────────────────────────
 * timer_init()
 *──────────────────────────────────────────────────────────────────────────────*/
void timer_init(uint32_t interval_ms, timer_callback_t callback)
{
    g_timer_callback = callback;
    g_tick_count     = 0U;

    /*
     * Convert milliseconds to CLINT ticks.
     * CLINT runs at 1MHz = 1,000,000 ticks per second = 1000 ticks per ms.
     */
    g_interval_ticks = (CLINT_FREQ_HZ / 1000U) * interval_ms;

    /* Arm first interrupt: fire after one interval from now */
    uint64_t now = timer_get_mtime();
    set_mtimecmp(now + g_interval_ticks);

    /*
     * Set mtvec to point to our trap handler.
     * mtvec in "direct mode" (bits[1:0]=00) routes ALL traps to this address.
     * trap_handler is defined in startup.S.
     */
    extern void trap_handler(void);
    uintptr_t handler_addr = (uintptr_t)&trap_handler;
    CSR_WRITE(mtvec, handler_addr);

    /*
     * Enable Machine Timer Interrupt in mie CSR.
     * This enables the timer interrupt specifically.
     * Note: global interrupts (mstatus.MIE) are still off until timer_enable().
     */
    uintptr_t mie_val;
    CSR_READ(mie, mie_val);
    mie_val |= MIE_MTIE;
    CSR_WRITE(mie, mie_val);
}

/*──────────────────────────────────────────────────────────────────────────────
 * timer_enable() — turn on global interrupts
 *
 * Sets mstatus.MIE = 1.
 * From this point forward, any pending interrupt in mip will fire.
 *──────────────────────────────────────────────────────────────────────────────*/
void timer_enable(void)
{
    CSR_SET(mstatus, MSTATUS_MIE);
}

void timer_disable(void)
{
    CSR_CLEAR(mstatus, MSTATUS_MIE);
}

uint32_t timer_get_ticks(void)
{
    return g_tick_count;
}

/*──────────────────────────────────────────────────────────────────────────────
 * delay_ms() — simple blocking delay
 *──────────────────────────────────────────────────────────────────────────────*/
void delay_ms(uint32_t ms)
{
    uint32_t start = g_tick_count;
    while ((g_tick_count - start) < ms) {
        /* Timer interrupts fire in background, incrementing g_tick_count */
    }
}

/*──────────────────────────────────────────────────────────────────────────────
 * timer_isr() — called from trap_handler in startup.S
 *
 * CRITICAL: Must re-arm mtimecmp before returning.
 * If mtimecmp is not updated, the condition (mtime >= mtimecmp) remains
 * true and the interrupt will fire again immediately — infinite loop in ISR.
 *──────────────────────────────────────────────────────────────────────────────*/
void timer_isr(void)
{
    /* Re-arm: next interrupt = now + interval */
    uint64_t now = timer_get_mtime();
    set_mtimecmp(now + g_interval_ticks);

    /* Increment global tick counter */
    g_tick_count++;

    /* Call user-registered callback if any */
    if (g_timer_callback != (void*)0) {
        g_timer_callback();
    }
}
