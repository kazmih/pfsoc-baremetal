#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/*
 * Timer callback function type.
 * You pass a function to timer_init() and it gets called
 * every time the timer fires.
 *
 * Example:
 *   void my_callback(void) { gpio_toggle(GPIO_BANK_2, 0); }
 *   timer_init(500, my_callback);  // call every 500ms
 */
typedef void (*timer_callback_t)(void);

/*
 * Initialize the CLINT timer.
 *
 * interval_ms  — how often to fire (in milliseconds)
 * callback     — function to call on each timer tick
 */
void timer_init(uint32_t interval_ms, timer_callback_t callback);

/*
 * Read current mtime value (64-bit microsecond counter)
 */
uint64_t timer_get_mtime(void);

/*
 * Called by trap_handler in startup.S on every timer interrupt.
 * Do not call this directly.
 */
void timer_isr(void);

#endif
