/*
 * Copyright (c) 2026 Alex Fabre
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Internal header shared by STM32 system bootloader transport drivers.
 *
 * Provides the common GPIO configuration structure and helper functions
 * used by all transport-specific bootloader drivers (USART, I2C, SPI, ...)
 * to enter and exit the STM32 system bootloader via BOOT0/BOOT1/NRST pins.
 *
 * @defgroup stm32_bootloader STM32 system bootloader
 * @ingroup stm32_bootloader
 * @{
 */

#ifndef ZEPHYR_DRIVERS_MISC_STM32_BOOTLOADER_COMMON_H_
#define ZEPHYR_DRIVERS_MISC_STM32_BOOTLOADER_COMMON_H_

#include <zephyr/drivers/gpio.h>

/** Common GPIO configuration for entering/exiting the bootloader. */
struct stm32_bootloader_gpio_cfg {
	/** BOOT0 pin specification (active-high selects system memory boot). */
	struct gpio_dt_spec boot0_gpio;
	/** BOOT1 pin specification (optional, port is NULL when absent). */
	struct gpio_dt_spec boot1_gpio;
	/** NRST pin specification for resetting the target. */
	struct gpio_dt_spec nrst_gpio;
	/** Duration of the NRST low pulse in milliseconds. */
	uint8_t reset_pulse_ms;
	/** Delay in milliseconds after reset before the bootloader is ready. */
	uint8_t boot_delay_ms;
};

/** Common target MCU related configuration. */
struct stm32_bootloader_mcu_cfg {
	/** Timeout in milliseconds for mass erase operations. */
	unsigned erase_timeout_ms;
};

/**
 * @brief Configure all bootloader GPIOs as output inactive.
 *
 * @param cfg Pointer to the GPIO configuration.
 * @return 0 on success, negative errno on failure.
 */
int stm32_bootloader_gpio_init(const struct stm32_bootloader_gpio_cfg *cfg);

/**
 * @brief Assert BOOT0/BOOT1 and pulse NRST to enter the bootloader.
 *
 * @param cfg Pointer to the GPIO configuration.
 * @return 0 on success, negative errno on failure.
 */
int stm32_bootloader_gpio_enter(const struct stm32_bootloader_gpio_cfg *cfg);

/**
 * @brief De-assert BOOT0/BOOT1 and optionally pulse NRST to exit the bootloader.
 *
 * @param cfg   Pointer to the GPIO configuration.
 * @param reset If true, pulse NRST to hard-reset the target.
 * @return 0 on success, negative errno on failure.
 */
int stm32_bootloader_gpio_exit(const struct stm32_bootloader_gpio_cfg *cfg, bool reset);

#endif /* ZEPHYR_DRIVERS_MISC_STM32_BOOTLOADER_COMMON_H_ */
