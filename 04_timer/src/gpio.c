/*==============================================================================
 * gpio.c — GPIO driver implementation for PolarFire SoC
 *
 * GPIO PHYSICAL REALITY:
 * Each GPIO pin is a metal pad on the chip package connected to internal
 * logic. When configured as output and driven HIGH, the pad is pulled to
 * 1.8V (the I/O voltage on PolarFire SoC). When driven LOW, it goes to 0V.
 *
 * Connect an LED between GPIO pin and GND through a resistor:
 *   GPIO_PIN ──[330Ω]──[LED]──[GND]
 * Writing 1 to the output register → 1.8V on pin → current flows → LED lights.
 * Writing 0 to the output register → 0V on pin → no current → LED off.
 *
 * REGISTER LAYOUT (per GPIO bank):
 *   Offset 0x00: CFG — direction register (1 bit per pin: 0=in, 1=out)
 *   Offset 0x40: OUT — output value register
 *   Offset 0x80: IN  — input value register (always readable)
 *
 * WHY OFFSETS 0x40 and 0x80? (not 0x04 and 0x08?)
 * The PolarFire SoC GPIO has a more complex register map with interrupt
 * configuration registers between CFG and OUT. The 0x40 and 0x80 offsets
 * account for these.
 *============================================================================*/
#include "gpio.h"
#include "../hal/hw_reg_access.h"

/*──────────────────────────────────────────────────────────────────────────────
 * Internal: map GPIO bank enum to base address
 *──────────────────────────────────────────────────────────────────────────────*/
static uint32_t gpio_get_base(gpio_bank_t bank)
{
    switch (bank) {
        case GPIO_BANK_0: return GPIO0_BASE;
        case GPIO_BANK_1: return GPIO1_BASE;
        case GPIO_BANK_2: return GPIO2_BASE;
        default:          return GPIO0_BASE;
    }
}

/*──────────────────────────────────────────────────────────────────────────────
 * gpio_set_dir()
 *
 * Configures whether a pin is an input or output.
 * Uses read-modify-write to avoid changing other pins in the bank.
 *
 * Example: Set pin 3 as output
 *   Before: CFG = 0b00000000 00000000 00000000 00000000
 *   Mask:         0b00000000 00000000 00000000 00001000  (1 << 3)
 *   After:  CFG = 0b00000000 00000000 00000000 00001000
 *──────────────────────────────────────────────────────────────────────────────*/
void gpio_set_dir(gpio_bank_t bank, uint8_t pin, gpio_dir_t dir)
{
    uint32_t base = gpio_get_base(bank);
    uint32_t cfg  = HW_REG32(base, GPIO_CFG);  /* Read current config */

    if (dir == GPIO_DIR_OUTPUT) {
        cfg |= (1U << pin);     /* Set bit → output */
    } else {
        cfg &= ~(1U << pin);    /* Clear bit → input */
    }

    HW_REG32(base, GPIO_CFG) = cfg;            /* Write back */
}

/*──────────────────────────────────────────────────────────────────────────────
 * gpio_set_high() / gpio_set_low()
 *
 * These use |= and &= to set/clear individual bits without affecting others.
 *──────────────────────────────────────────────────────────────────────────────*/
void gpio_set_high(gpio_bank_t bank, uint8_t pin)
{
    HW_REG32(gpio_get_base(bank), GPIO_OUT) |= (1U << pin);
}

void gpio_set_low(gpio_bank_t bank, uint8_t pin)
{
    HW_REG32(gpio_get_base(bank), GPIO_OUT) &= ~(1U << pin);
}

/*──────────────────────────────────────────────────────────────────────────────
 * gpio_toggle()
 *
 * XOR flips the bit: 0 XOR 1 = 1, 1 XOR 1 = 0
 * All other bits are XOR'd with 0, so they stay unchanged.
 *──────────────────────────────────────────────────────────────────────────────*/
void gpio_toggle(gpio_bank_t bank, uint8_t pin)
{
    HW_REG32(gpio_get_base(bank), GPIO_OUT) ^= (1U << pin);
}

/*──────────────────────────────────────────────────────────────────────────────
 * gpio_read() — read single pin state
 *──────────────────────────────────────────────────────────────────────────────*/
uint8_t gpio_read(gpio_bank_t bank, uint8_t pin)
{
    return (uint8_t)((HW_REG32(gpio_get_base(bank), GPIO_IN) >> pin) & 1U);
}

void gpio_write_bank(gpio_bank_t bank, uint32_t value)
{
    HW_REG32(gpio_get_base(bank), GPIO_OUT) = value;
}

uint32_t gpio_read_bank(gpio_bank_t bank)
{
    return HW_REG32(gpio_get_base(bank), GPIO_IN);
}
