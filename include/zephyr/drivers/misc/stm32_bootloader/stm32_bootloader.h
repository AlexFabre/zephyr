/*
 * Copyright (c) 2025 Contributors to the Zephyr Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public API for the STM32 system bootloader driver.
 *
 * Transport-agnostic interface for programming a target STM32 via its
 * built-in system bootloader. Concrete transports (USART, I2C, CAN)
 * register a driver_api implementing these operations.
 *
 * @defgroup stm32_bootloader STM32 system bootloader
 * @ingroup stm32_bootloader
 * @{
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_MISC_STM32_BOOTLOADER_H_
#define ZEPHYR_INCLUDE_DRIVERS_MISC_STM32_BOOTLOADER_H_

#include <zephyr/device.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum data bytes per single bootloader read/write command (AN3155). */
#define STM32_BOOTLOADER_MAX_DATA_SIZE 256

/** Information retrieved from the target bootloader. */
struct stm32_bootloader_info {
	/** Bootloader version (e.g. 0x31 = v3.1) */
	uint8_t bl_version;
	/** Number of supported commands */
	uint8_t num_cmds;
	/** Supported command bytes */
	uint8_t cmds[16];
	/** Option byte 1 (from Get Version) */
	uint8_t option1;
	/** Option byte 2 (from Get Version) */
	uint8_t option2;
	/** Product ID */
	uint16_t pid;
	/** true if Extended Erase (0x44) is supported, false for standard Erase (0x43) */
	bool has_extended_erase;
};

/**
 * @brief Progress callback for write/program operations.
 *
 * @param bytes_written Number of bytes written so far.
 * @param total_bytes   Total number of bytes to write.
 * @param user_data     User-supplied context pointer.
 */
typedef void (*stm32_bootloader_progress_cb_t)(size_t bytes_written, size_t total_bytes,
					       void *user_data);

/** @cond INTERNAL_HIDDEN */

typedef int (*stm32_bootloader_enter_t)(const struct device *dev);
typedef int (*stm32_bootloader_exit_t)(const struct device *dev, bool reset_target);
typedef int (*stm32_bootloader_go_t)(const struct device *dev, uint32_t addr);
typedef int (*stm32_bootloader_get_info_t)(const struct device *dev,
					   struct stm32_bootloader_info *info);
typedef int (*stm32_bootloader_mass_erase_t)(const struct device *dev);
typedef int (*stm32_bootloader_erase_pages_t)(const struct device *dev, const uint16_t *pages,
					      size_t num_pages);
typedef int (*stm32_bootloader_write_t)(const struct device *dev, uint32_t addr,
					const uint8_t *data, size_t len,
					stm32_bootloader_progress_cb_t cb, void *user_data);
typedef int (*stm32_bootloader_read_t)(const struct device *dev, uint32_t addr, uint8_t *buf,
				       size_t len);

__subsystem struct stm32_bootloader_driver_api {
	stm32_bootloader_enter_t enter;
	stm32_bootloader_exit_t exit;
	stm32_bootloader_go_t go;
	stm32_bootloader_get_info_t get_info;
	stm32_bootloader_mass_erase_t mass_erase;
	stm32_bootloader_erase_pages_t erase_pages;
	stm32_bootloader_write_t write;
	stm32_bootloader_read_t read;
};

/** @endcond */

/**
 * @brief Enter the target STM32 system bootloader.
 *
 * Asserts BOOT0, pulses NRST (if available), and performs
 * transport-specific synchronization. On success the target is ready
 * to accept commands.
 *
 * @param dev Driver device instance.
 * @return 0 on success, negative errno on failure.
 */
static inline int stm32_bootloader_enter(const struct device *dev)
{
	const struct stm32_bootloader_driver_api *api = DEVICE_API_GET(stm32_bootloader, dev);

	return api->enter(dev);
}

/**
 * @brief Exit the bootloader.
 *
 * Restores transport configuration, de-asserts BOOT0/BOOT1, and
 * optionally pulses NRST to reset the target.
 *
 * @param dev          Driver device instance.
 * @param reset_target If true, pulse NRST to hard-reset the target.
 * @return 0 on success, negative errno on failure.
 */
static inline int stm32_bootloader_exit(const struct device *dev, bool reset_target)
{
	const struct stm32_bootloader_driver_api *api = DEVICE_API_GET(stm32_bootloader, dev);

	return api->exit(dev, reset_target);
}

/**
 * @brief Execute a Go command.
 *
 * Sends the Go command with the given address, causing the target to
 * jump and run from that address.
 *
 * @note BOOT0/BOOT1 are de-asserted so the target runs from flash on
 * next reset.
 * stm32_bootloader_exit() should be called afterward to properly exit the
 * bootloader mode and reset the USART peripheral.
 * On most STM32 a hard reset (NRST toggle) is not necessary, the target
 * autonomously runs the code after a 'go' command.
 *
 *
 * @param dev  Driver device instance.
 * @param addr Target address to jump to.
 * @return 0 on success, negative errno on failure.
 */
static inline int stm32_bootloader_go(const struct device *dev, uint32_t addr)
{
	const struct stm32_bootloader_driver_api *api = DEVICE_API_GET(stm32_bootloader, dev);

	return api->go(dev, addr);
}

/**
 * @brief Query target bootloader information.
 *
 * Executes Get, Get Version, and Get ID commands and populates @p info.
 *
 * @param dev  Driver device instance.
 * @param info Output structure to fill.
 * @return 0 on success, negative errno on failure.
 */
static inline int stm32_bootloader_get_info(const struct device *dev,
					    struct stm32_bootloader_info *info)
{
	const struct stm32_bootloader_driver_api *api = DEVICE_API_GET(stm32_bootloader, dev);

	return api->get_info(dev, info);
}

/**
 * @brief Perform a mass erase of the target flash.
 *
 * Automatically selects standard Erase (0x43) or Extended Erase (0x44)
 * based on the target's capabilities. get_info must be called first.
 *
 * @param dev Driver device instance.
 * @return 0 on success, negative errno on failure.
 */
static inline int stm32_bootloader_mass_erase(const struct device *dev)
{
	const struct stm32_bootloader_driver_api *api = DEVICE_API_GET(stm32_bootloader, dev);

	return api->mass_erase(dev);
}

/**
 * @brief Erase specific pages/sectors on the target.
 *
 * @param dev       Driver device instance.
 * @param pages     Array of page/sector numbers to erase.
 * @param num_pages Number of entries in @p pages.
 * @return 0 on success, negative errno on failure.
 */
static inline int stm32_bootloader_erase_pages(const struct device *dev, const uint16_t *pages,
					       size_t num_pages)
{
	const struct stm32_bootloader_driver_api *api = DEVICE_API_GET(stm32_bootloader, dev);

	return api->erase_pages(dev, pages, num_pages);
}

/**
 * @brief Write data to target memory.
 *
 * The data is sent in transport-specific chunks. An optional progress
 * callback is invoked after each chunk.
 *
 * @param dev       Driver device instance.
 * @param addr      Target start address (must be word-aligned per AN3155).
 * @param data      Data buffer to write.
 * @param len       Number of bytes to write.
 * @param cb        Optional progress callback (may be NULL).
 * @param user_data User context passed to @p cb.
 * @return 0 on success, negative errno on failure.
 */
static inline int stm32_bootloader_write(const struct device *dev, uint32_t addr,
					 const uint8_t *data, size_t len,
					 stm32_bootloader_progress_cb_t cb, void *user_data)
{
	const struct stm32_bootloader_driver_api *api = DEVICE_API_GET(stm32_bootloader, dev);

	return api->write(dev, addr, data, len, cb, user_data);
}

/**
 * @brief Read from target memory.
 *
 * @param dev  Driver device instance.
 * @param addr Target start address.
 * @param buf  Buffer to store read data.
 * @param len  Number of bytes to read.
 * @return 0 on success, negative errno on failure.
 */
static inline int stm32_bootloader_read(const struct device *dev, uint32_t addr, uint8_t *buf,
					size_t len)
{
	const struct stm32_bootloader_driver_api *api = DEVICE_API_GET(stm32_bootloader, dev);

	return api->read(dev, addr, buf, len);
}

/**
 * @brief Program firmware from a flash partition into the target.
 *
 * Reads firmware from the host flash partition and writes it to the
 * target using the transport's write operation.
 *
 * @param dev       Driver device instance.
 * @param addr      Target start address.
 * @param fa_id     Flash area ID of the source partition.
 * @param fw_size   Size of the firmware to program (bytes).
 * @param cb        Optional progress callback (may be NULL).
 * @param user_data User context passed to @p cb.
 * @return 0 on success, negative errno on failure.
 */
int stm32_bootloader_program_partition(const struct device *dev, uint32_t addr, uint8_t fa_id,
				       size_t fw_size, stm32_bootloader_progress_cb_t cb,
				       void *user_data);

/**
 * @brief Verify target memory against a data buffer.
 *
 * Reads back data from the target and compares it byte-by-byte.
 *
 * @param dev  Driver device instance.
 * @param addr Target start address.
 * @param data Expected data buffer.
 * @param len  Number of bytes to verify.
 * @return 0 if data matches, -EIO on mismatch, other negative errno on failure.
 */
int stm32_bootloader_verify(const struct device *dev, uint32_t addr, const uint8_t *data,
			    size_t len);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_MISC_STM32_BOOTLOADER_H_ */
