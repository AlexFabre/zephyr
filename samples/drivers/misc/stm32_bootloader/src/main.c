/*
 * Copyright (c) 2025 Contributors to the Zephyr Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Sample application for the STM32 USART system bootloader driver.
 *
 * Demonstrates how to program a target STM32 via its built-in USART system
 * bootloader (AN3155). The sample enters bootloader mode, reads target
 * information, performs a mass erase, writes a small test pattern, verifies
 * the written data, and finally starts execution on the target.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/misc/stm32_bootloader/stm32_bootloader.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/** Base address of the target STM32 internal flash. */
#define TARGET_FLASH_BASE 0x08000000

/** Bootloader device instance obtained from the devicetree. */
static const struct device *const bl_dev = DEVICE_DT_GET(DT_NODELABEL(stm32_target));

/**
 * @brief Progress callback invoked after each write chunk.
 *
 * @param written   Number of bytes written so far.
 * @param total     Total number of bytes to write.
 * @param user_data User-supplied context (unused).
 */
static void progress_cb(size_t written, size_t total, void *user_data)
{
	unsigned int pct = (unsigned int)((written * 100U) / total);

	LOG_INF("Progress: %u/%u bytes (%u%%)", (unsigned int)written, (unsigned int)total, pct);
}

int main(void)
{
	struct stm32_bootloader_info info;
	int ret;

	if (!device_is_ready(bl_dev)) {
		LOG_ERR("Bootloader device not ready");
		return -ENODEV;
	}

	LOG_INF("=== STM32 USART Bootloader Sample ===");

	/* Step 1: Enter bootloader mode */
	LOG_INF("Entering bootloader...");
	ret = stm32_bootloader_enter(bl_dev);
	if (ret < 0) {
		LOG_ERR("Failed to enter bootloader: %d", ret);
		return ret;
	}

	/* Step 2: Read target info */
	LOG_INF("Reading target info...");
	ret = stm32_bootloader_get_info(bl_dev, &info);
	if (ret < 0) {
		LOG_ERR("Failed to get info: %d", ret);
		return ret;
	}

	LOG_INF("Target: PID=0x%04x, BL v%d.%d, extended_erase=%d", info.pid, info.bl_version >> 4,
		info.bl_version & 0x0F, info.has_extended_erase);

	/* Step 3: Mass erase */
	LOG_INF("Erasing target flash...");
	ret = stm32_bootloader_mass_erase(bl_dev);
	if (ret < 0) {
		LOG_ERR("Mass erase failed: %d", ret);
		return ret;
	}

	/*
	 * Step 4: Write firmware.
	 *
	 * To use stm32_bootloader_program_partition(), you need a valid flash
	 * partition with firmware data and its flash area ID. For this sample,
	 * we demonstrate with a small test pattern instead.
	 */
	static const uint8_t test_data[] = {
		/* Minimal ARM vector table: initial SP and reset handler */
		0x00,
		0x00,
		0x02,
		0x20, /* SP = 0x20020000 */
		0x09,
		0x00,
		0x00,
		0x08, /* Reset = 0x08000009 */
		/* Thumb: infinite loop (b .) */
		0xFE,
		0xE7,
	};

	LOG_INF("Writing test data (%zu bytes)...", sizeof(test_data));
	ret = stm32_bootloader_write(bl_dev, TARGET_FLASH_BASE, test_data, sizeof(test_data),
				     progress_cb, NULL);
	if (ret < 0) {
		LOG_ERR("Write failed: %d", ret);
		return ret;
	}

	/* Step 5: Verify */
	LOG_INF("Verifying...");
	ret = stm32_bootloader_verify(bl_dev, TARGET_FLASH_BASE, test_data, sizeof(test_data));
	if (ret < 0) {
		LOG_ERR("Verify failed: %d", ret);
		return ret;
	}

	/* Step 6: Go */
	LOG_INF("Jumping to 0x%08x...", TARGET_FLASH_BASE);
	ret = stm32_bootloader_go(bl_dev, TARGET_FLASH_BASE);
	if (ret < 0) {
		LOG_ERR("Go failed: %d", ret);
		return ret;
	}

	/* Step 7: Reset target */
	LOG_INF("Reset target");
	ret = stm32_bootloader_exit(bl_dev, true);
	if (ret < 0) {
		LOG_ERR("Reset failed: %d", ret);
		return ret;
	}

	LOG_INF("=== Done! Target should be running. ===");
	return 0;
}
