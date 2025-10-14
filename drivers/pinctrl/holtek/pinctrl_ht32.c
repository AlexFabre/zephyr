/*
 * Copyright (c) 2025 Alex Fabre <alex.fabre@rtone.fr>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/pinctrl.h>
#include <zephyr/sys/sys_io.h>

/* AFIO base address */
#define HT32_AFIO_BASE 0x40022000

/* GPIO Port Configuration Register offsets in AFIO */
#define HT32_AFIO_GPACFGR0_OFFSET 0x020
#define HT32_AFIO_GPACFGR1_OFFSET 0x024
#define HT32_AFIO_GPBCFGR0_OFFSET 0x028
#define HT32_AFIO_GPBCFGR1_OFFSET 0x02C
#define HT32_AFIO_GPCCFGR0_OFFSET 0x030
#define HT32_AFIO_GPCCFGR1_OFFSET 0x034

/* Pinctrl state structure */
struct pinctrl_ht32_config {
	uint8_t port;    /* Port number (0=A, 1=B, 2=C) */
	uint8_t pin;     /* Pin number (0-15) */
	uint8_t af;      /* Alternate function (0-15) */
};

/* Extract port, pin, and AF from pinctrl encoding
 * Encoding: bits [31:24] = port, bits [23:16] = pin, bits [15:8] = AF
 */
#define HT32_PINCTRL_PORT(cfg)  (((cfg) >> 24) & 0xFF)
#define HT32_PINCTRL_PIN(cfg)   (((cfg) >> 16) & 0xFF)
#define HT32_PINCTRL_AF(cfg)    (((cfg) >> 8) & 0xFF)

int pinctrl_configure_pins(const pinctrl_soc_pin_t *pins, uint8_t pin_cnt,
			    uintptr_t reg)
{
	ARG_UNUSED(reg);

	for (uint8_t i = 0; i < pin_cnt; i++) {
		uint8_t port = HT32_PINCTRL_PORT(pins[i]);
		uint8_t pin = HT32_PINCTRL_PIN(pins[i]);
		uint8_t af = HT32_PINCTRL_AF(pins[i]);
		uint32_t cfg_offset;
		uint32_t cfg_val;

		/* Determine the configuration register offset based on port and pin */
		if (port == 0) { /* GPIOA */
			cfg_offset = (pin < 8) ? HT32_AFIO_GPACFGR0_OFFSET :
						 HT32_AFIO_GPACFGR1_OFFSET;
		} else if (port == 1) { /* GPIOB */
			cfg_offset = (pin < 8) ? HT32_AFIO_GPBCFGR0_OFFSET :
						 HT32_AFIO_GPBCFGR1_OFFSET;
		} else if (port == 2) { /* GPIOC */
			cfg_offset = (pin < 8) ? HT32_AFIO_GPCCFGR0_OFFSET :
						 HT32_AFIO_GPCCFGR1_OFFSET;
		} else {
			return -EINVAL;
		}

		/* Each pin uses 4 bits in the configuration register */
		uint8_t pin_pos = (pin % 8) * 4;
		uint32_t pin_mask = 0xF << pin_pos;

		/* Read current configuration */
		cfg_val = sys_read32(HT32_AFIO_BASE + cfg_offset);

		/* Clear the pin configuration bits */
		cfg_val &= ~pin_mask;

		/* Set the new alternate function */
		cfg_val |= (af & 0xF) << pin_pos;

		/* Write back the configuration */
		sys_write32(cfg_val, HT32_AFIO_BASE + cfg_offset);
	}

	return 0;
}
