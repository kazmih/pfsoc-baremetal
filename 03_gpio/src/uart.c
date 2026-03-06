/*==============================================================================
 * uart.c — UART driver implementation for PolarFire SoC
 *
 * READ THIS BEFORE THE CODE:
 *
 * A UART frame looks like this on the wire (115200 baud, 8N1):
 *
 *   IDLE  START  D0  D1  D2  D3  D4  D5  D6  D7  STOP  IDLE
 *    ___   ___   _   _   _   _   _   _   _   _   ___   ___
 *   |   | |   |   |   |   |   |   |   |   |   | |   | |   |
 *   |   | |   |   |   |   |   |   |   |   |   | |   | |   |
 *   └───┘ └─┐ └───┘   └───┘   └───┘   └───┘   └─┘   └───┘
 *             ↑ START bit (always low for 1 bit time)
 *
 * At 115200 baud, each bit takes: 1/115200 = 8.68 microseconds
 * A complete 8N1 frame (10 bits total) takes: 86.8 microseconds
 *
 * INITIALIZATION SEQUENCE (must be in this exact order):
 *   1. Set DLAB=1  → unlocks DLL/DLM baud rate registers
 *   2. Write DLL   → baud rate divisor low byte
 *   3. Write DLM   → baud rate divisor high byte
 *   4. Set DLAB=0  → lock baud rate, configure frame format (8N1)
 *   5. Enable FIFO → buffer multiple bytes for efficiency
 *   6. Disable IRQ → we'll use polling mode
 *
 * WHY THIS ORDER?
 * The DLAB bit is a "key" that switches two register sets.
 * When DLAB=1, addresses 0x00 and 0x04 become DLL and DLM.
 * When DLAB=0, they revert to THR/RBR and IER.
 * You must set DLAB=1 to configure baud rate, then DLAB=0 to use UART normally.
 *============================================================================*/
#include "uart.h"
#include "../hal/hw_reg_access.h"

/*──────────────────────────────────────────────────────────────────────────────
 * Internal helper: return base address for the requested UART instance
 *──────────────────────────────────────────────────────────────────────────────*/
static uint32_t uart_get_base(uart_id_t id)
{
    return (id == UART_0) ? UART0_BASE : UART1_BASE;
}

/*──────────────────────────────────────────────────────────────────────────────
 * uart_init()
 *
 * Physical effect: configures the UART hardware to transmit and receive
 * at the specified baud rate using 8N1 format.
 *
 * After this function returns, writing to THR will transmit bytes.
 *──────────────────────────────────────────────────────────────────────────────*/
void uart_init(uart_id_t id, uint32_t baud_rate)
{
    uint32_t base = uart_get_base(id);

    /*
     * Calculate baud rate divisor.
     *
     * The UART hardware divides the APB bus clock by (16 × divisor)
     * to generate the baud rate clock.
     *
     * divisor = APB_CLK / (16 × baud_rate)
     *
     * At 150MHz APB clock, for 115200 baud:
     *   divisor = 150,000,000 / (16 × 115200) = 81
     *
     * The "16" factor comes from the UART's internal oversampling —
     * it samples each bit 16 times to detect it reliably.
     */
    uint32_t divisor = MSS_APB_CLK_HZ / (16U * baud_rate);

    /*
     * Step 1: Set DLAB=1 to access divisor registers.
     *
     * LCR = 0x80 = 1000 0000 in binary
     *              ↑ bit 7 = DLAB
     *
     * After this write, addresses 0x00 and 0x04 become DLL and DLM.
     */
    HW_REG32(base, UART_LCR) = UART_LCR_DLAB;

    /*
     * Step 2 & 3: Write baud rate divisor.
     *
     * Divisor 81 = 0x0051
     *   DLL = 0x51 (low byte)
     *   DLM = 0x00 (high byte)
     */
    HW_REG32(base, UART_DLL) = (divisor & 0xFFU);
    HW_REG32(base, UART_DLM) = ((divisor >> 8U) & 0xFFU);

    /*
     * Step 4: Set DLAB=0 and configure 8N1 frame format.
     *
     * LCR = 0x03 = 0000 0011 in binary
     *              ││││ ││└┘ → bits[1:0] = 11 = 8 data bits
     *              ││││ │└── → bit[2]   =  0 = 1 stop bit
     *              ││││ └─── → bits[4:3] = 00 = no parity
     *              └└└└───── → bits[7:5] = 000 = DLAB=0, normal mode
     *
     * After this, UART is ready in 8N1 format.
     */
    HW_REG32(base, UART_LCR) = UART_LCR_8N1;

    /*
     * Step 5: Enable and reset FIFOs.
     *
     * FCR = 0x07 = 0000 0111
     *               ││└ → bit[0] = 1 = enable FIFO (16-byte buffer)
     *               │└── → bit[1] = 1 = reset RX FIFO (clear any garbage)
     *               └─── → bit[2] = 1 = reset TX FIFO (start fresh)
     *
     * Without FIFOs, you must read each byte the instant it arrives.
     * With FIFOs, the hardware buffers up to 16 bytes for you.
     */
    HW_REG32(base, UART_FCR) = (UART_FCR_ENABLE | UART_FCR_RX_RST | UART_FCR_TX_RST);

    /*
     * Step 6: Disable all UART interrupts.
     * We use polling (checking LSR status bits) instead of interrupts.
     * This is simpler for our learning purposes.
     */
    HW_REG32(base, UART_IER) = 0x00U;
}

/*──────────────────────────────────────────────────────────────────────────────
 * uart_putc()
 *
 * Physical effect: transmits one character over the serial line.
 *
 * The polling loop waits for THRE (TX Holding Register Empty) bit in LSR.
 * When THRE=1, the hardware has finished transmitting the previous byte
 * and is ready to accept a new one.
 *
 * THRE=0: Hardware is busy shifting out the previous byte. We must wait.
 * THRE=1: Hardware is idle. Safe to write the next byte.
 *──────────────────────────────────────────────────────────────────────────────*/
void uart_putc(uart_id_t id, char c)
{
    uint32_t base = uart_get_base(id);

    /*
     * '\n' (newline) alone moves the cursor down but not to the left.
     * '\r' (carriage return) moves cursor to start of line.
     * Most terminals need '\r\n' to go to the beginning of the next line.
     */
    if (c == '\n') {
        uart_putc(id, '\r');
    }

    /* Poll THRE bit — wait until TX holding register is empty */
    while (!(HW_REG32(base, UART_LSR) & UART_LSR_THRE)) {
        /* Busy wait. In production firmware, yield to RTOS here. */
    }

    /*
     * Write character to THR (Transmit Holding Register).
     * The UART hardware immediately begins shifting it out serially.
     * We don't wait for completion — THRE will clear, and we'll
     * check it before the next write.
     */
    HW_REG32(base, UART_THR) = (uint32_t)c;
}

/*──────────────────────────────────────────────────────────────────────────────
 * uart_puts() / uart_println()
 *──────────────────────────────────────────────────────────────────────────────*/
void uart_puts(uart_id_t id, const char *str)
{
    while (*str != '\0') {
        uart_putc(id, *str++);
    }
}

void uart_println(uart_id_t id, const char *str)
{
    uart_puts(id, str);
    uart_putc(id, '\n');
}

/*──────────────────────────────────────────────────────────────────────────────
 * uart_print_hex()
 *
 * Converts a 32-bit number to its hexadecimal string representation.
 * Example: 0xDEADBEEF → "0xDEADBEEF"
 *
 * We do not use sprintf() because that requires the C standard library,
 * which requires an OS (or at minimum a semihosting setup).
 * On bare metal, we implement it ourselves.
 *──────────────────────────────────────────────────────────────────────────────*/
void uart_print_hex(uart_id_t id, uint32_t value)
{
    const char hex_chars[] = "0123456789ABCDEF";
    char buf[11];   /* "0x" + 8 hex digits + null terminator */
    int i;

    buf[0]  = '0';
    buf[1]  = 'x';
    buf[10] = '\0';

    /* Extract nibbles from right to left */
    for (i = 9; i >= 2; i--) {
        buf[i] = hex_chars[value & 0xFU];  /* lowest 4 bits → hex digit */
        value >>= 4U;                        /* shift right by 4          */
    }

    uart_puts(id, buf);
}

/*──────────────────────────────────────────────────────────────────────────────
 * uart_print_dec()
 *
 * Converts a 32-bit number to its decimal string representation.
 * Example: 12345 → "12345"
 *──────────────────────────────────────────────────────────────────────────────*/
void uart_print_dec(uart_id_t id, uint32_t value)
{
    char buf[11];   /* max 10 digits for uint32_max (4294967295) + null */
    int i = 10;

    buf[10] = '\0';

    if (value == 0U) {
        uart_putc(id, '0');
        return;
    }

    /* Extract digits from right to left (least significant first) */
    while (value > 0U && i > 0) {
        buf[--i] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    uart_puts(id, &buf[i]);
}

/*──────────────────────────────────────────────────────────────────────────────
 * uart_getc() — blocking receive
 *
 * Polls DR (Data Ready) bit in LSR.
 * DR=1 means the hardware has received a byte and stored it in RBR.
 *──────────────────────────────────────────────────────────────────────────────*/
char uart_getc(uart_id_t id)
{
    uint32_t base = uart_get_base(id);

    /* Wait until a byte arrives */
    while (!(HW_REG32(base, UART_LSR) & UART_LSR_DR)) {}

    /* Read from RBR — clears the DR bit */
    return (char)(HW_REG32(base, UART_RBR) & 0xFFU);
}

/*──────────────────────────────────────────────────────────────────────────────
 * uart_rx_ready() — non-blocking check
 *──────────────────────────────────────────────────────────────────────────────*/
int uart_rx_ready(uart_id_t id)
{
    uint32_t base = uart_get_base(id);
    return (HW_REG32(base, UART_LSR) & UART_LSR_DR) ? 1 : 0;
}
