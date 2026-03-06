#include <stdint.h>
#include "uart.h"
#include "gpio.h"
#include "timer.h"

static volatile uint32_t g_ticks = 0;

void timer_callback(void)
{
    g_ticks++;
    gpio_toggle(GPIO_BANK_2, 0);

    if (g_ticks % 2 == 0) {
        uart_println(UART_0, "LED ON");
    } else {
        uart_println(UART_0, "LED OFF");
    }
}

int main(void)
{
    uart_init(UART_0, BAUD_115200);
    uart_println(UART_0, "Timer Test Starting...");

    gpio_set_dir(GPIO_BANK_2, 0, GPIO_DIR_OUTPUT);

    timer_init(500, timer_callback);

    uart_println(UART_0, "Timer initialized. Interrupts enabled.");
    uart_println(UART_0, "LED will toggle every 500ms.");

    while(1) {}

    return 0;
}
