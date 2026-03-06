#include <stdint.h>
#include "uart.h"

int main(void)
{
    uart_init(UART_0, BAUD_115200);

    uart_println(UART_0, "");
    uart_println(UART_0, "================================");
    uart_println(UART_0, "  PolarFire SoC Bare Metal");
    uart_println(UART_0, "  Hello World from RISC-V!");
    uart_println(UART_0, "================================");
    uart_println(UART_0, "");
    uart_println(UART_0, "UART driver working correctly.");
    uart_println(UART_0, "Baud rate: 115200, Format: 8N1");
    uart_println(UART_0, "APB clock: 150MHz");
    uart_println(UART_0, "Divisor: 81");

    while(1) {}

    return 0;
}
