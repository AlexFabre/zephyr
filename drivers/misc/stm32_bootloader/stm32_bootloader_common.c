/*
 * Copyright (c) 2026 Alex Fabre
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file stm32_bootloader_common.c
 * @brief STM32 system bootloader common functions.
 *
 * This module provides transport-agnostic functionality shared by all STM32
 * bootloader transport drivers (UART, I2C, SPI, …):
 * - GPIO helpers to configure, enter, and exit the system bootloader via
 *   BOOT0/BOOT1/NRST pin control.
 * - A generic flash-partition-to-bootloader programming routine.
 * - A read-back verification routine.
 *
 * @defgroup stm32_bootloader STM32 system bootloader
 * @ingroup stm32_bootloader
 * @{
 */

#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>

#include <zephyr/drivers/misc/stm32_bootloader/stm32_bootloader.h>
#include "stm32_bootloader_common.h"

LOG_MODULE_REGISTER(stm32_bootloader, CONFIG_STM32_BOOTLOADER_LOG_LEVEL);

/**
 * @brief Configure all bootloader GPIOs as output-inactive.
 *
 * Initialises BOOT0, the optional BOOT1 and NRST pins as GPIO outputs
 * driven low.  BOOT1 is silently skipped when its port is @c NULL.
 *
 * @param cfg Pointer to the GPIO configuration populated from devicetree.
 *
 * @retval 0       Success.
 * @retval -ENODEV A required GPIO controller is not ready.
 * @retval <0      Other negative errno from gpio_pin_configure_dt().
 */
int stm32_bootloader_gpio_init(const struct stm32_bootloader_gpio_cfg *cfg)
{
	int ret;

	if (!gpio_is_ready_dt(&cfg->boot0_gpio)) {
		LOG_ERR("BOOT0 GPIO not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&cfg->boot0_gpio, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure BOOT0 GPIO: %d", ret);
		return ret;
	}

	if (cfg->boot1_gpio.port != NULL) {
		if (!gpio_is_ready_dt(&cfg->boot1_gpio)) {
			LOG_ERR("BOOT1 GPIO not ready");
			return -ENODEV;
		}

		ret = gpio_pin_configure_dt(&cfg->boot1_gpio, GPIO_OUTPUT_INACTIVE);
		if (ret < 0) {
			LOG_ERR("Failed to configure BOOT1 GPIO: %d", ret);
			return ret;
		}
	}

	if (!gpio_is_ready_dt(&cfg->nrst_gpio)) {
		LOG_ERR("NRST GPIO not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&cfg->nrst_gpio, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure NRST GPIO: %d", ret);
		return ret;
	}

	return 0;
}

/**
 * @brief Assert BOOT0/BOOT1 and pulse NRST to enter the system bootloader.
 *
 * The sequence is:
 *  -# Drive BOOT0 high (selects system memory boot).
 *  -# Drive BOOT1 high if the pin is defined.
 *  -# Pulse NRST for @c cfg->reset_pulse_ms milliseconds.
 *  -# Wait @c cfg->boot_delay_ms milliseconds for the bootloader to start.
 *
 * @param cfg Pointer to the GPIO configuration.
 *
 * @retval 0  Success.
 * @retval <0 Negative errno from gpio_pin_set_dt().
 */
int stm32_bootloader_gpio_enter(const struct stm32_bootloader_gpio_cfg *cfg)
{
	int ret;

	/* Assert BOOT0 to select system bootloader */
	ret = gpio_pin_set_dt(&cfg->boot0_gpio, 1);
	if (ret < 0) {
		LOG_ERR("Failed to assert BOOT0: %d", ret);
		return ret;
	}

	/* Assert BOOT1 if available */
	if (cfg->boot1_gpio.port != NULL) {
		ret = gpio_pin_set_dt(&cfg->boot1_gpio, 1);
		if (ret < 0) {
			LOG_ERR("Failed to assert BOOT1: %d", ret);
			return ret;
		}
	}

	/* Pulse NRST */
	ret = gpio_pin_set_dt(&cfg->nrst_gpio, 1);
	if (ret < 0) {
		LOG_ERR("Failed to assert NRST: %d", ret);
		return ret;
	}

	k_msleep(cfg->reset_pulse_ms);

	ret = gpio_pin_set_dt(&cfg->nrst_gpio, 0);
	if (ret < 0) {
		LOG_ERR("Failed to de-assert NRST: %d", ret);
		return ret;
	}

	/* Wait for bootloader to start */
	k_msleep(cfg->boot_delay_ms);

	return 0;
}

/**
 * @brief De-assert BOOT0/BOOT1 and optionally pulse NRST to exit the bootloader.
 *
 * BOOT0 (and BOOT1 when present) are driven low so the target will boot
 * from internal flash on the next reset.  If @p reset is true an NRST
 * pulse of @c cfg->reset_pulse_ms milliseconds is generated to force an
 * immediate reboot.
 *
 * @param cfg   Pointer to the GPIO configuration.
 * @param reset If true, pulse NRST to hard-reset the target.
 *
 * @retval 0  Success.
 * @retval <0 Negative errno from gpio_pin_set_dt().
 */
int stm32_bootloader_gpio_exit(const struct stm32_bootloader_gpio_cfg *cfg, bool reset)
{
	int ret;

	/* De-assert BOOT0 so the target runs from flash on next reset */
	gpio_pin_set_dt(&cfg->boot0_gpio, 0);

	/* De-assert BOOT1 if available */
	if (cfg->boot1_gpio.port != NULL) {
		gpio_pin_set_dt(&cfg->boot1_gpio, 0);
	}

	/* Pulse NRST if required */
	if (reset) {
		ret = gpio_pin_set_dt(&cfg->nrst_gpio, 1);
		if (ret < 0) {
			LOG_ERR("Failed to assert NRST: %d", ret);
			return ret;
		}

		k_msleep(cfg->reset_pulse_ms);

		ret = gpio_pin_set_dt(&cfg->nrst_gpio, 0);
		if (ret < 0) {
			LOG_ERR("Failed to de-assert NRST: %d", ret);
			return ret;
		}
	}

	return 0;
}

/**
 * @brief Program a flash partition into the target via the bootloader.
 *
 * Reads firmware data in @c STM32_BOOTLOADER_MAX_DATA_SIZE-byte chunks from
 * the Zephyr flash area identified by @p fa_id and writes each chunk to the
 * target starting at address @p addr using stm32_bootloader_write().
 * An optional progress callback is invoked after every successful chunk.
 *
 * @param dev       Bootloader device instance.
 * @param addr      Target base address (on the STM32 memory map).
 * @param fa_id     Zephyr flash-area identifier of the source partition.
 * @param fw_size   Number of firmware bytes to program.
 * @param cb        Optional progress callback, may be @c NULL.
 * @param user_data Opaque pointer forwarded to @p cb.
 *
 * @retval 0       Success.
 * @retval -ENOMEM Firmware size exceeds partition size.
 * @retval <0      Negative errno from flash_area_open(), flash_area_read()
 *                 or stm32_bootloader_write().
 */
int stm32_bootloader_program_partition(const struct device *dev, uint32_t addr, uint8_t fa_id,
				       size_t fw_size, stm32_bootloader_progress_cb_t cb,
				       void *user_data)
{
	const struct flash_area *fa;
	uint8_t buf[STM32_BOOTLOADER_MAX_DATA_SIZE];
	size_t written = 0;
	int ret;

	ret = flash_area_open(fa_id, &fa);
	if (ret < 0) {
		LOG_ERR("Failed to open flash area %u: %d", fa_id, ret);
		return ret;
	}

	if (fw_size > fa->fa_size) {
		LOG_ERR("Firmware size %zu exceeds partition size %zu", fw_size, fa->fa_size);
		flash_area_close(fa);
		return -ENOMEM;
	}

	while (written < fw_size) {
		size_t chunk = MIN(fw_size - written, sizeof(buf));

		ret = flash_area_read(fa, written, buf, chunk);
		if (ret < 0) {
			LOG_ERR("Flash read failed at offset %zu: %d", written, ret);
			goto out;
		}

		ret = stm32_bootloader_write(dev, addr + written, buf, chunk, NULL, NULL);
		if (ret < 0) {
			LOG_ERR("Write failed at offset %zu: %d", written, ret);
			goto out;
		}

		written += chunk;

		if (cb != NULL) {
			cb(written, fw_size, user_data);
		}
	}

	LOG_INF("Programmed %zu bytes from partition %u to 0x%08x", fw_size, fa_id, addr);

out:
	flash_area_close(fa);
	return ret;
}

/**
 * @brief Verify target memory against a reference buffer.
 *
 * Reads back memory from the target in @c STM32_BOOTLOADER_MAX_DATA_SIZE-byte
 * chunks using stm32_bootloader_read() and compares each chunk against
 * @p data_buf.  Returns an error on the first mismatch.
 *
 * @param dev      Bootloader device instance.
 * @param addr     Target base address to read back.
 * @param data_buf Expected data to compare against.
 * @param len      Number of bytes to verify.
 *
 * @retval 0       All bytes match.
 * @retval -EINVAL @p data_buf is NULL or @p len is zero.
 * @retval -EIO    Data mismatch detected.
 * @retval <0      Negative errno from stm32_bootloader_read().
 */
int stm32_bootloader_verify(const struct device *dev, uint32_t addr, const uint8_t *data_buf,
			    size_t len)
{
	uint8_t rbuf[STM32_BOOTLOADER_MAX_DATA_SIZE];
	size_t verified = 0;
	int ret;

	if (data_buf == NULL || len == 0) {
		return -EINVAL;
	}

	while (verified < len) {
		size_t chunk = MIN(len - verified, sizeof(rbuf));

		ret = stm32_bootloader_read(dev, addr + verified, rbuf, chunk);
		if (ret < 0) {
			return ret;
		}

		if (memcmp(&data_buf[verified], rbuf, chunk) != 0) {
			LOG_ERR("Verify mismatch at offset %zu", verified);
			return -EIO;
		}

		verified += chunk;
	}

	LOG_DBG("Verified %zu bytes at 0x%08x", len, addr);
	return 0;
}
