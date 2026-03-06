/*==============================================================================
 * hw_reg_access.h
 * Hardware register access macros for PolarFire SoC bare metal
 *
 * CONCEPT: Every peripheral on the chip is controlled by writing to or
 * reading from specific memory addresses. This file defines:
 *   1. Macros to access those addresses cleanly
 *   2. The base addresses of each peripheral
 *   3. The register offsets within each peripheral
 *   4. The bit masks for individual bits within registers
 *
 * Without this file, your code would be full of magic numbers like:
 *   *((volatile uint32_t*)0x2000000C) |= 0x80;   // What does this mean??
 *
 * With this file:
 *   HW_REG32(UART0_BASE, UART_LCR) |= UART_LCR_DLAB;  // Much clearer!
 *============================================================================*/
#ifndef HW_REG_ACCESS_H
#define HW_REG_ACCESS_H

#include <stdint.h>

/*──────────────────────────────────────────────────────────────────────────────
 * REGISTER ACCESS MACROS
 *
 * HW_REG32(BASE, OFFSET) expands to a pointer dereference:
 *   *((volatile uint32_t *)(BASE + OFFSET))
 *
 * volatile tells the compiler: this memory location has hardware side effects.
 * Do not cache it, do not optimize it away, always generate the actual
 * memory read/write instruction.
 *──────────────────────────────────────────────────────────────────────────────*/
#define HW_REG32(BASE, OFFSET) \
    (*((volatile uint32_t *)((uint32_t)(BASE) + (uint32_t)(OFFSET))))

#define HW_REG8(BASE, OFFSET) \
    (*((volatile uint8_t *)((uint32_t)(BASE) + (uint32_t)(OFFSET))))

/*──────────────────────────────────────────────────────────────────────────────
 * MSS PERIPHERAL BASE ADDRESSES
 * These are the starting addresses of each peripheral's register block.
 * From the PolarFire SoC datasheet, Table 3-1: MSS Memory Map.
 *──────────────────────────────────────────────────────────────────────────────*/
#define UART0_BASE      0x20000000UL    /* UART0 — connected to USB debug port */
#define UART1_BASE      0x20100000UL    /* UART1 — general purpose             */
#define GPIO0_BASE      0x20200000UL    /* GPIO Bank 0 — 14 pins               */
#define GPIO1_BASE      0x20210000UL    /* GPIO Bank 1 — 24 pins               */
#define GPIO2_BASE      0x20220000UL    /* GPIO Bank 2 — 32 pins               */
#define SPI0_BASE       0x20300000UL    /* SPI0                                */
#define SPI1_BASE       0x20400000UL    /* SPI1                                */
#define I2C0_BASE       0x20500000UL    /* I2C0                                */
#define I2C1_BASE       0x20600000UL    /* I2C1                                */
#define CLINT_BASE      0x02000000UL    /* CLINT — timer and software IRQ      */
#define PLIC_BASE       0x0C000000UL    /* PLIC — platform interrupt controller*/

/* FPGA Fabric slave window — your custom IPs (Task 6 PWM) go here */
#define FABRIC_BASE     0x60000000UL

/*──────────────────────────────────────────────────────────────────────────────
 * UART REGISTER OFFSETS (16550-compatible)
 *
 * The UART uses the same register at offset 0x00 for three different
 * purposes depending on context:
 *   - Reading when DLAB=0: RBR (received byte)
 *   - Writing when DLAB=0: THR (byte to transmit)
 *   - R/W when DLAB=1:     DLL (baud rate low byte)
 *
 * DLAB = Divisor Latch Access Bit (bit 7 of LCR)
 *──────────────────────────────────────────────────────────────────────────────*/
#define UART_THR        0x00U   /* Transmit Holding Register    [W, DLAB=0] */
#define UART_RBR        0x00U   /* Receive Buffer Register      [R, DLAB=0] */
#define UART_DLL        0x00U   /* Divisor Latch Low            [RW,DLAB=1] */
#define UART_IER        0x04U   /* Interrupt Enable Register    [RW,DLAB=0] */
#define UART_DLM        0x04U   /* Divisor Latch High           [RW,DLAB=1] */
#define UART_IIR        0x08U   /* Interrupt Identification     [R]         */
#define UART_FCR        0x08U   /* FIFO Control Register        [W]         */
#define UART_LCR        0x0CU   /* Line Control Register        [RW]        */
#define UART_MCR        0x10U   /* Modem Control Register       [RW]        */
#define UART_LSR        0x14U   /* Line Status Register         [R]         */
#define UART_MSR        0x18U   /* Modem Status Register        [R]         */

/* UART LCR bit masks */
#define UART_LCR_DLAB   (1U << 7)   /* 1=access divisor, 0=normal access   */
#define UART_LCR_8N1    (0x03U)     /* 8 data bits, no parity, 1 stop bit  */

/* UART LSR bit masks — read these to know hardware state */
#define UART_LSR_DR     (1U << 0)   /* Data Ready: 1=byte waiting to read  */
#define UART_LSR_OE     (1U << 1)   /* Overrun Error                       */
#define UART_LSR_PE     (1U << 2)   /* Parity Error                        */
#define UART_LSR_FE     (1U << 3)   /* Framing Error                       */
#define UART_LSR_THRE   (1U << 5)   /* TX Holding Empty: 1=ready to send   */
#define UART_LSR_TEMT   (1U << 6)   /* TX Empty: 1=both THR and shift empty*/

/* UART FCR bit masks */
#define UART_FCR_ENABLE (1U << 0)   /* Enable FIFOs                        */
#define UART_FCR_RX_RST (1U << 1)   /* Reset RX FIFO                       */
#define UART_FCR_TX_RST (1U << 2)   /* Reset TX FIFO                       */

/* APB bus clock — used for baud rate calculation */
#define MSS_APB_CLK_HZ  150000000UL /* 150 MHz                             */

/*──────────────────────────────────────────────────────────────────────────────
 * GPIO REGISTER OFFSETS
 *
 * CFG: Each bit configures one pin. Bit N: 0=input, 1=output.
 * OUT: Each bit drives one output pin. Only affects pins configured as output.
 * IN:  Read current state of all pins regardless of direction.
 *──────────────────────────────────────────────────────────────────────────────*/
#define GPIO_CFG        0x00U   /* Direction configuration register        */
#define GPIO_OUT        0x40U   /* Output value register                   */
#define GPIO_IN         0x80U   /* Input value register                    */

/*──────────────────────────────────────────────────────────────────────────────
 * CLINT REGISTER OFFSETS
 *
 * The CLINT contains a 64-bit free-running counter (mtime) at 1MHz.
 * You compare it against mtimecmp to generate timer interrupts.
 * On 32-bit access, each 64-bit register is split into two 32-bit halves.
 *──────────────────────────────────────────────────────────────────────────────*/
#define CLINT_MSIP          0x0000U /* Machine Software Interrupt Pending  */
#define CLINT_MTIMECMP_LO   0x4000U /* Timer compare — low 32 bits         */
#define CLINT_MTIMECMP_HI   0x4004U /* Timer compare — high 32 bits        */
#define CLINT_MTIME_LO      0xBFF8U /* Current timer value — low 32 bits   */
#define CLINT_MTIME_HI      0xBFFCU /* Current timer value — high 32 bits  */

/* CLINT reference clock — 1 tick per microsecond */
#define CLINT_FREQ_HZ       1000000UL

/*──────────────────────────────────────────────────────────────────────────────
 * RISC-V CSR BIT MASKS
 * CSRs are not memory-mapped — accessed via csrr/csrw assembly instructions.
 * These masks are used with inline assembly in timer.c.
 *──────────────────────────────────────────────────────────────────────────────*/
#define MSTATUS_MIE     (1UL << 3)  /* Global machine interrupt enable     */
#define MIE_MSIE        (1UL << 3)  /* Machine software interrupt enable   */
#define MIE_MTIE        (1UL << 7)  /* Machine timer interrupt enable      */
#define MIE_MEIE        (1UL << 11) /* Machine external interrupt enable   */

/* mcause values for interrupts (bit 31 set) */
#define MCAUSE_INT_BIT      (1UL << 31)
#define MCAUSE_TIMER_INT    (7UL)       /* Machine timer interrupt cause   */
#define MCAUSE_SW_INT       (3UL)       /* Machine software interrupt      */
#define MCAUSE_EXT_INT      (11UL)      /* Machine external interrupt      */

#endif /* HW_REG_ACCESS_H */
