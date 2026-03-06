#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>

typedef enum {
    GPIO_BANK_0 = 0,
    GPIO_BANK_1 = 1,
    GPIO_BANK_2 = 2
} gpio_bank_t;

typedef enum {
    GPIO_DIR_INPUT  = 0,
    GPIO_DIR_OUTPUT = 1
} gpio_dir_t;

void     gpio_set_dir(gpio_bank_t bank, uint8_t pin, gpio_dir_t dir);
void     gpio_set_high(gpio_bank_t bank, uint8_t pin);
void     gpio_set_low(gpio_bank_t bank, uint8_t pin);
void     gpio_toggle(gpio_bank_t bank, uint8_t pin);
uint8_t  gpio_read(gpio_bank_t bank, uint8_t pin);
void     gpio_write_bank(gpio_bank_t bank, uint32_t value);
uint32_t gpio_read_bank(gpio_bank_t bank);

#endif
