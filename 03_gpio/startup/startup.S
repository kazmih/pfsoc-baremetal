/*==============================================================================
 * startup.S — RISC-V bare metal startup code for PolarFire SoC
 *
 * THIS FILE RUNS BEFORE main(). IT IS THE FIRST CODE THE CPU EXECUTES.
 *
 * After reset, the PolarFire SoC CPU starts executing at 0x08000000
 * (the start of LIM — Loosely Integrated Memory).
 * Our linker script places _start at this exact address.
 *
 * WHAT WE MUST DO BEFORE main() CAN RUN:
 *
 * 1. Set up the stack pointer (sp register)
 *    Without this, function calls crash. The stack is where local
 *    variables and return addresses are stored. The CPU needs to know
 *    where the stack is before calling any function.
 *
 * 2. Clear the BSS segment
 *    BSS contains global variables that are zero-initialized.
 *    The C standard guarantees they start at zero, but flash memory
 *    might contain garbage. We zero it out explicitly.
 *
 * 3. Call main()
 *
 * THIS FILE ALSO CONTAINS THE TRAP HANDLER:
 * All CPU exceptions and interrupts jump to trap_handler.
 * It saves CPU state, identifies the cause, calls the right handler,
 * then restores CPU state and resumes normal execution.
 *
 * ASSEMBLY SYNTAX (GNU RISC-V):
 *   la  rd, symbol   → load address of symbol into rd
 *   li  rd, imm      → load immediate value into rd
 *   sw  rs2, off(rs1) → store word: memory[rs1+off] = rs2
 *   lw  rd, off(rs1)  → load word: rd = memory[rs1+off]
 *   j   label        → unconditional jump
 *   call symbol      → function call (saves return address in ra)
 *   mret             → return from machine-mode trap
 *==============================================================================*/

    .section .text.startup      /* Linker places this section first → 0x08000000 */
    .global _start
    .global trap_handler

/*──────────────────────────────────────────────────────────────────────────────
 * _start: the reset vector
 *
 * This is the very first instruction the CPU executes after power-on or reset.
 * The linker script ensures this is placed at 0x08000000.
 *──────────────────────────────────────────────────────────────────────────────*/
_start:
    /*
     * Set the stack pointer.
     * _stack_top is defined in the linker script (pfsoc.ld) as the
     * top of the DTIM region. The stack grows downward from here.
     *
     * Without this line, the first function call in main() would
     * store its return address at whatever garbage sp contains.
     */
    la   sp, _stack_top

    /*
     * Clear the BSS segment.
     *
     * BSS contains zero-initialized global/static variables.
     * _bss_start and _bss_end are defined by the linker script.
     *
     * We loop from _bss_start to _bss_end, writing 0 at each word.
     *
     * a0 = current address (starts at _bss_start)
     * a1 = end address    (_bss_end)
     */
    la   a0, _bss_start
    la   a1, _bss_end
bss_clear_loop:
    bge  a0, a1, bss_clear_done    /* if a0 >= a1: all done, exit loop */
    sw   zero, 0(a0)               /* store 0 at address a0            */
    addi a0, a0, 4                 /* advance pointer by 4 bytes       */
    j    bss_clear_loop            /* repeat                           */
bss_clear_done:

    /*
     * Call main().
     *
     * From this point, C code takes over.
     * main() should never return (it contains an infinite loop).
     * If it does return, we hang in the loop below.
     */
    call main

    /* Safety net: hang if main() ever returns */
_hang:
    j    _hang

/*──────────────────────────────────────────────────────────────────────────────
 * trap_handler: the single entry point for ALL exceptions and interrupts
 *
 * When ANY interrupt or exception fires, the CPU hardware automatically:
 *   1. Saves the current PC into mepc (machine exception program counter)
 *   2. Saves the CPU privilege mode
 *   3. Disables further interrupts (clears mstatus.MIE)
 *   4. Jumps here (because mtvec points here)
 *
 * We then:
 *   1. Save all caller-saved registers (the C calling convention says
 *      called functions must not corrupt these, but ISR is like a function
 *      called by hardware — we must save them ourselves)
 *   2. Read mcause to find out WHY we got here
 *   3. Call the appropriate handler
 *   4. Restore all registers
 *   5. Execute mret to resume where we were interrupted
 *
 * .balign 4: mtvec requires the handler address to be 4-byte aligned.
 *──────────────────────────────────────────────────────────────────────────────*/
    .balign 4
trap_handler:

    /*
     * Save caller-saved registers to the stack.
     *
     * RISC-V calling convention defines "caller-saved" registers as those
     * that a called function is allowed to overwrite. Since our interrupt
     * handler calls C functions (timer_isr), those C functions might
     * overwrite these registers. We must save and restore them.
     *
     * Registers saved: ra, t0-t6, a0-a7 (17 registers × 4 bytes = 68 bytes)
     * We allocate 128 bytes for alignment and future expansion.
     */
    addi sp, sp, -128       /* grow stack down by 128 bytes */

    sw   ra,   0(sp)        /* ra = return address register              */
    sw   t0,   4(sp)        /* t0-t6 = temporary registers               */
    sw   t1,   8(sp)
    sw   t2,  12(sp)
    sw   a0,  16(sp)        /* a0-a7 = argument/return value registers   */
    sw   a1,  20(sp)
    sw   a2,  24(sp)
    sw   a3,  28(sp)
    sw   a4,  32(sp)
    sw   a5,  36(sp)
    sw   a6,  40(sp)
    sw   a7,  44(sp)
    sw   t3,  48(sp)
    sw   t4,  52(sp)
    sw   t5,  56(sp)
    sw   t6,  60(sp)

    /*
     * Read mcause to determine why we got here.
     *
     * mcause format (RV32):
     *   Bit 31:    1 = interrupt, 0 = exception/trap
     *   Bits[30:0]: cause code
     *
     * Timer interrupt: bit31=1, bits[30:0]=7
     * So mcause for timer = 0x80000007
     */
    csrr t0, mcause

    /*
     * Check bit 31: is this an interrupt or an exception?
     */
    li   t1, 0x80000000
    and  t2, t0, t1
    beqz t2, _exception_handler    /* bit31 = 0 → exception, not interrupt */

    /*
     * It's an interrupt. Check the cause code (bits [30:0]).
     */
    li   t1, 0x7FFFFFFF        /* mask off bit31, keep cause code */
    and  t0, t0, t1

    li   t1, 7                     /* 7 = Machine Timer Interrupt */
    beq  t0, t1, _handle_timer

    /* Unknown interrupt — just return */
    j    _trap_exit

_handle_timer:
    /* timer_isr not linked yet — skip for UART-only test */
    j    _trap_exit

_exception_handler:
    /*
     * An exception occurred (illegal instruction, misaligned access, etc.)
     *
     * In a real system: read mepc to find the offending instruction,
     * log the error, attempt recovery or halt.
     *
     * For now: infinite loop so we can catch it in the debugger.
     * If Renode is connected, execution will stop here and you can
     * inspect mcause and mepc to understand what went wrong.
     */
_exception_loop:
    j    _exception_loop

_trap_exit:
    /* Restore all saved registers */
    lw   ra,   0(sp)
    lw   t0,   4(sp)
    lw   t1,   8(sp)
    lw   t2,  12(sp)
    lw   a0,  16(sp)
    lw   a1,  20(sp)
    lw   a2,  24(sp)
    lw   a3,  28(sp)
    lw   a4,  32(sp)
    lw   a5,  36(sp)
    lw   a6,  40(sp)
    lw   a7,  44(sp)
    lw   t3,  48(sp)
    lw   t4,  52(sp)
    lw   t5,  56(sp)
    lw   t6,  60(sp)
    addi sp, sp, 128        /* restore stack pointer */

    /*
     * mret: Machine-mode Return.
     * This instruction:
     *   1. Restores PC from mepc (resumes where we were interrupted)
     *   2. Restores privilege mode from mstatus.MPP
     *   3. Re-enables interrupts (sets mstatus.MIE = mstatus.MPIE)
     *
     * Execution resumes exactly where it was before the interrupt.
     */
    mret
