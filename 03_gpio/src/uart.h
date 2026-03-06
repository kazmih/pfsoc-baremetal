/*==============================================================================
 * uart.h — UART driver interface for PolarFire SoC
 *
 * The UART (Universal Asynchronous Receiver Transmitter) converts bytes
 * into a serial stream of voltage pulses and back again.
 *
 * "Asynchronous" means there is no shared clock between sender and receiver.
 * Instead, both sides agree on a baud rate (bits per second) in advance.
 * The receiver uses a local oscillator to sample incoming bits at the right times.
 *
 * PolarFire SoC uses a 16550-compatible UART — one of the most common
 * UART designs in the world, used since 1987.
 *============================================================================*/
#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stddef.h>

/*──────────────────────────────────────────────────────────────────────────────
 * Baud rate constants
 * Baud rate = number of bits transmitted per second.
 * 115200 baud = 115,200 bits/sec = ~11,520 bytes/sec (with start/stop bits)
 *──────────────────────────────────────────────────────────────────────────────*/
#define BAUD_115200     115200U
#define BAUD_9600       9600U
#define BAUD_57600      57600U

/*──────────────────────────────────────────────────────────────────────────────
 * UART instance selection
 *──────────────────────────────────────────────────────────────────────────────*/
typedef enum {
    UART_0 = 0,     /* UART0 at 0x20000000 — connected to USB debug port */
    UART_1 = 1      /* UART1 at 0x20100000 — general purpose             */
} uart_id_t;

/*──────────────────────────────────────────────────────────────────────────────
 * Public API
 *──────────────────────────────────────────────────────────────────────────────*/

/*
 * uart_init() — Initialize UART hardware
 *
 * Must call this before any other uart_* function.
 * Configures baud rate, 8N1 format, enables FIFO.
 *
 * 8N1 means: 8 data bits, No parity, 1 stop bit.
 * This is the most common serial format in the world.
 */
void uart_init(uart_id_t id, uint32_t baud_rate);

/*
 * uart_putc() — Transmit one character
 *
 * Blocks (busy-waits) until the UART TX buffer is empty, then writes
 * the character. For '\n', automatically sends '\r\n' for terminal compat.
 */
void uart_putc(uart_id_t id, char c);

/*
 * uart_puts() — Transmit a null-terminated string
 */
void uart_puts(uart_id_t id, const char *str);

/*
 * uart_println() — Transmit a string followed by newline
 */
void uart_println(uart_id_t id, const char *str);

/*
 * uart_print_hex() — Print a 32-bit value in hex format
 * Example: uart_print_hex(UART_0, 0xDEADBEEF) → prints "0xDEADBEEF"
 * Useful for dumping register values during debugging.
 */
void uart_print_hex(uart_id_t id, uint32_t value);

/*
 * uart_print_dec() — Print a 32-bit value in decimal
 * Example: uart_print_dec(UART_0, 12345) → prints "12345"
 */
void uart_print_dec(uart_id_t id, uint32_t value);

/*
 * uart_getc() — Receive one character (blocking)
 * Waits until a byte arrives in the RX buffer, then returns it.
 */
char uart_getc(uart_id_t id);

/*
 * uart_rx_ready() — Check if a character is waiting (non-blocking)
 * Returns 1 if a byte is available, 0 if not.
 */
int uart_rx_ready(uart_id_t id);

#endif /* UART_H */
