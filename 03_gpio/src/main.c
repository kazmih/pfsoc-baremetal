#include <stdint.h>
#include "uart.h"
#include "gpio.h"

int main(void)
{
    uart_init(UART_0, BAUD_115200);
    uart_println(UART_0, "GPIO Test Starting...");

    /* configure GPIO2 pin 0 as output (connected to LED in Renode) */
    gpio_set_dir(GPIO_BANK_2, 0, GPIO_DIR_OUTPUT);
    uart_println(UART_0, "GPIO2 pin 0 set as output");

    /* blink 5 times */
    int i;
    for (i = 0; i < 5; i++) {
        gpio_set_high(GPIO_BANK_2, 0);
        uart_println(UART_0, "LED ON");

        volatile int delay;
        for (delay = 0; delay < 100000; delay++) {}

        gpio_set_low(GPIO_BANK_2, 0);
        uart_println(UART_0, "LED OFF");

        for (delay = 0; delay < 100000; delay++) {}
    }

    uart_println(UART_0, "Done.");
    while(1) {}
    return 0;
}
