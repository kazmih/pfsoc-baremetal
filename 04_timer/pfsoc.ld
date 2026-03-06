/*==============================================================================
 * pfsoc.ld — Linker Script for PolarFire SoC Bare Metal
 *
 * WHAT DOES A LINKER SCRIPT DO?
 *
 * The compiler turns your C code into machine code (object files).
 * The linker combines all object files into one final executable.
 * The linker script tells the linker WHERE to place each piece in memory.
 *
 * Without a linker script, the linker doesn't know:
 *   - Where does code go? (LIM? DDR? eNVM?)
 *   - Where does data go?
 *   - Where is the stack?
 *   - Which function should be at address 0x08000000 (the reset vector)?
 *
 * The linker script answers all these questions.
 *
 * POLARFIRE SOC MEMORY REGIONS WE USE:
 *
 *   LIM (Loosely Integrated Memory): 0x08000000 — 256KB
 *     - Fast, on-chip SRAM
 *     - Our code (.text) goes here
 *     - CPU starts executing here after reset
 *
 *   DTIM (Data Tightly Integrated Memory): 0x01000000 — 8KB
 *     - Very fast, tightly coupled to the CPU core
 *     - Our data (.data, .bss) goes here
 *     - Stack goes here
 *==============================================================================*/

ENTRY(_start)   /* Tell linker: execution starts at _start (in startup.S) */

MEMORY {
    /*
     * LIM: 256KB of on-chip SRAM at 0x08000000
     * rwx = readable, writable, executable (code can run from here)
     */
    LIM  (rwx) : ORIGIN = 0x08000000, LENGTH = 256K

    /*
     * DTIM: 8KB of tightly integrated data memory at 0x01000000
     * rw = readable and writable (data, not executable)
     */
    DTIM (rw)  : ORIGIN = 0x01000000, LENGTH = 8K
}

SECTIONS {

    /*──────────────────────────────────────────────────────────────────────────
     * .text section — program code and read-only data
     * Goes into LIM starting at 0x08000000.
     *──────────────────────────────────────────────────────────────────────────*/
    .text : {
        /*
         * KEEP(.text.startup): force _start to be placed first.
         * Without KEEP, the linker might reorder sections.
         * _start must be at 0x08000000 exactly — it's the reset vector.
         */
        KEEP(*(.text.startup))

        /* All other code sections */
        *(.text*)

        /* Read-only data (string literals, const arrays) */
        *(.rodata*)

    } > LIM     /* Place this section in LIM */

    /*──────────────────────────────────────────────────────────────────────────
     * .data section — initialized global/static variables
     *
     * Example:
     *   int counter = 42;   ← goes in .data (has initial value)
     *   int sum;            ← goes in .bss  (no initial value, zero-init)
     *
     * AT > LIM: the initialization values are stored in LIM (in flash/ROM)
     * > DTIM:   at runtime, the values are copied to DTIM for fast access
     *
     * startup.S would normally copy .data from LIM to DTIM.
     * For simplicity in this project, we keep .data simple.
     *──────────────────────────────────────────────────────────────────────────*/
    .data : {
        _data_start = .;    /* Symbol: start of data section */
        *(.data*)
        _data_end = .;      /* Symbol: end of data section */
    } > DTIM AT > LIM

    /*──────────────────────────────────────────────────────────────────────────
     * .bss section — zero-initialized global/static variables
     *
     * The linker reserves space here but doesn't store any values.
     * startup.S clears this region to zero before calling main().
     *
     * _bss_start and _bss_end are used by startup.S's BSS clear loop.
     *──────────────────────────────────────────────────────────────────────────*/
    .bss : {
        _bss_start = .;     /* Symbol used by startup.S */
        *(.bss*)
        *(COMMON)           /* Uninitialized global variables */
        _bss_end = .;       /* Symbol used by startup.S */
    } > DTIM

    /*──────────────────────────────────────────────────────────────────────────
     * Stack — 4KB at the top of DTIM
     *
     * The stack grows DOWNWARD on RISC-V.
     * _stack_top is the highest address — startup.S sets sp = _stack_top.
     * As functions are called, sp decreases.
     * Stack overflow occurs if sp goes below _bss_end.
     *
     * 4KB is sufficient for our simple bare metal application.
     * A real RTOS would allocate per-task stacks.
     *──────────────────────────────────────────────────────────────────────────*/
    .stack : {
        . = ALIGN(16);      /* 16-byte alignment (RISC-V ABI requirement) */
        . += 4096;          /* Reserve 4KB for stack */
        _stack_top = .;     /* Symbol used by startup.S */
    } > DTIM
}
