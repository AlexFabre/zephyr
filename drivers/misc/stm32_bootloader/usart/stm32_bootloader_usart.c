/*
 * Copyright (c) 2026 Alex Fabre
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief STM32 USART system bootloader driver (AN3155 protocol).
 *
 * Implements the STM32 bootloader driver API over USART using the AN3155
 * protocol. Provides enter/exit, command/address framing, flash erase,
 * read, write, and go operations via polling-mode UART.
 *
 * @defgroup stm32_bootloader STM32 system bootloader
 * @ingroup stm32_bootloader
 * @{
 */

#define DT_DRV_COMPAT st_stm32_bootloader_uart

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

#include <zephyr/drivers/misc/stm32_bootloader/stm32_bootloader.h>
#include "../stm32_bootloader_common.h"
#include "stm32_bootloader_usart.h"

LOG_MODULE_REGISTER(stm32_bootloader_uart, CONFIG_STM32_BOOTLOADER_UART_LOG_LEVEL);

/** Per-instance configuration, populated from devicetree. */
struct stm32_bootloader_uart_config {
	/** Common BOOT0/BOOT1/NRST GPIO configuration. */
	const struct stm32_bootloader_gpio_cfg gpio;
	/** Common target MCU specific configurations. */
	const struct stm32_bootloader_mcu_cfg mcu;

	/** UART device used for AN3155 communication. */
	const struct device *uart_dev;
	/**  UART bootloader communication speed */
	const unsigned baudrate;
	/** Does UART need to have parity set to "none" instead of "even" */
	const bool parity_none;
	/** Per-byte receive timeout in milliseconds. */
	const uint8_t timeout_ms;
};

/** Per-instance runtime data. */
struct stm32_bootloader_uart_data {
	/** Mutex protecting concurrent access to the driver. */
	struct k_mutex lock;
	/** Cached bootloader information from the last get_info call. */
	struct stm32_bootloader_info info;
	/** Backup of the original UART configuration, restored on exit. */
	struct uart_config uart_cfg_bkp;
	/** True when a bootloader session is active. */
	bool connected;
};

/* --------------------------------------------------------------------------
 * UART helpers (polling mode)
 * -------------------------------------------------------------------------- */

/**
 * @brief Transmit a single byte over UART in polling mode.
 *
 * @param uart_dev UART device instance.
 * @param byte     Byte to send.
 */
static void stm32_bootloader_uart_put(const struct device *uart_dev, uint8_t byte)
{
	uart_poll_out(uart_dev, byte);
}

/**
 * @brief Receive a single byte over UART with a timeout.
 *
 * Polls the UART until a byte is available or the deadline expires.
 *
 * @param uart_dev   UART device instance.
 * @param byte       Pointer to store the received byte.
 * @param timeout_ms Maximum time to wait in milliseconds.
 *
 * @retval 0          Success.
 * @retval -ETIMEDOUT No byte received within @p timeout_ms.
 */
static int stm32_bootloader_uart_get(const struct device *uart_dev, uint8_t *byte,
				     uint32_t timeout_ms)
{
	int64_t deadline = k_uptime_get() + timeout_ms;

	while (k_uptime_get() < deadline) {
		if (uart_poll_in(uart_dev, byte) == 0) {
			return 0;
		}
		k_yield();
	}

	return -ETIMEDOUT;
}

/**
 * @brief Drain any stale bytes from the UART receive buffer.
 *
 * @param uart_dev UART device instance.
 */
static void stm32_bootloader_uart_drain_rx(const struct device *uart_dev)
{
	uint8_t dummy;

	while (uart_poll_in(uart_dev, &dummy) == 0) {
		/* discard */
	}
}

/* --------------------------------------------------------------------------
 * AN3155 protocol primitives
 * -------------------------------------------------------------------------- */

/**
 * @brief Compute the XOR checksum of a byte buffer.
 *
 * @param buf Pointer to the data buffer.
 * @param len Number of bytes in @p buf.
 *
 * @return XOR of all bytes in the buffer.
 */
static uint8_t stm32_bootloader_uart_xor_checksum(const uint8_t *buf, size_t len)
{
	uint8_t xor = 0;

	for (size_t i = 0; i < len; i++) {
		xor ^= buf[i];
	}

	return xor;
}

/**
 * @brief Wait for an ACK/NACK response from the bootloader.
 *
 * @param dev        Driver device instance.
 * @param timeout_ms Maximum time to wait for a response in milliseconds.
 *
 * @retval 0          ACK received.
 * @retval -EPROTO    NACK or unexpected byte received.
 * @retval -ETIMEDOUT No response within @p timeout_ms.
 */
static int stm32_bootloader_uart_wait_ack(const struct device *dev, uint32_t timeout_ms)
{
	const struct stm32_bootloader_uart_config *cfg = dev->config;
	uint8_t byte;
	int ret;

	ret = stm32_bootloader_uart_get(cfg->uart_dev, &byte, timeout_ms);
	if (ret < 0) {
		return ret;
	}

	if (byte == STM32_BOOTLOADER_UART_ACK) {
		return 0;
	}

	if (byte == STM32_BOOTLOADER_UART_NACK) {
		LOG_WRN("NACK received");
		return -EPROTO;
	}

	LOG_WRN("Unexpected response: 0x%02x", byte);
	return -EPROTO;
}

/**
 * @brief Send a command byte and its complement, then wait for ACK.
 *
 * Per AN3155, each command is sent as {cmd, ~cmd} followed by an ACK.
 *
 * @param dev Driver device instance.
 * @param cmd Command byte to send.
 *
 * @retval 0       ACK received.
 * @retval -EPROTO Command rejected (NACK).
 * @retval <0      Other negative errno on failure.
 */
static int stm32_bootloader_uart_send_cmd(const struct device *dev, uint8_t cmd)
{
	const struct stm32_bootloader_uart_config *cfg = dev->config;

	stm32_bootloader_uart_put(cfg->uart_dev, cmd);
	stm32_bootloader_uart_put(cfg->uart_dev, ~cmd);

	return stm32_bootloader_uart_wait_ack(dev, cfg->timeout_ms);
}

/**
 * @brief Send a 32-bit address (big-endian) with XOR checksum and wait for ACK.
 *
 * @param dev  Driver device instance.
 * @param addr 32-bit target address.
 *
 * @retval 0       ACK received.
 * @retval -EPROTO Address rejected (NACK).
 * @retval <0      Other negative errno on failure.
 */
static int stm32_bootloader_uart_send_addr(const struct device *dev, uint32_t addr)
{
	const struct stm32_bootloader_uart_config *cfg = dev->config;
	uint8_t buf[4];

	sys_put_be32(addr, buf);

	for (int i = 0; i < 4; i++) {
		stm32_bootloader_uart_put(cfg->uart_dev, buf[i]);
	}
	stm32_bootloader_uart_put(cfg->uart_dev, stm32_bootloader_uart_xor_checksum(buf, 4));

	return stm32_bootloader_uart_wait_ack(dev, cfg->timeout_ms);
}

/* --------------------------------------------------------------------------
 * Driver API: Enter / Go / Exit
 * -------------------------------------------------------------------------- */

/**
 * @brief Enter the STM32 system bootloader over USART.
 *
 * Backs up the current UART configuration, reconfigures UART for AN3155,
 * asserts BOOT pins, pulses NRST, drains stale RX data, and sends the
 * synchronization byte (0x7F). On success the target is ready for commands.
 *
 * @param dev Driver device instance.
 *
 * @retval 0       Success, bootloader synchronised.
 * @retval -EPROTO Synchronization failed (no ACK from target).
 * @retval <0      Other negative errno on failure.
 */
static int stm32_bootloader_uart_enter(const struct device *dev)
{
	const struct stm32_bootloader_uart_config *cfg = dev->config;
	struct stm32_bootloader_uart_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);

	/* Backup current uart configuration */
	ret = uart_config_get(cfg->uart_dev, &data->uart_cfg_bkp);
	if (ret < 0) {
		LOG_ERR("Failed to backup current UART config: %d", ret);
		goto out;
	}

	/* Prepare STM32 bootloader UART configuration (AN3155) */
	struct uart_config uart_bootloader_cfg = {
		.baudrate = cfg->baudrate,
		.parity = (cfg->parity_none) ? UART_CFG_PARITY_NONE : UART_CFG_PARITY_EVEN,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
		.data_bits = UART_CFG_DATA_BITS_8,
	};

	/* Configure the UART driver for STM32 bootloader mode */
	ret = uart_configure(cfg->uart_dev, &uart_bootloader_cfg);
	if (ret < 0) {
		LOG_ERR("Failed to configure UART for stm32 bootloader mode: %d", ret);
		goto out;
	}

	/* Assert BOOT pins and pulse NRST via common GPIO helper */
	ret = stm32_bootloader_gpio_enter(&cfg->gpio);
	if (ret < 0) {
		goto out;
	}

	/* Drain any stale data */
	stm32_bootloader_uart_drain_rx(cfg->uart_dev);

	/* Send sync byte */
	stm32_bootloader_uart_put(cfg->uart_dev, STM32_BOOTLOADER_UART_SYNC);

	ret = stm32_bootloader_uart_wait_ack(dev, cfg->timeout_ms);
	if (ret < 0) {
		LOG_ERR("Bootloader sync failed: %d", ret);
		goto out;
	}

	data->connected = true;
	LOG_INF("Connected to bootloader");

out:
	k_mutex_unlock(&data->lock);
	return ret;
}

/**
 * @brief Send the Go command to jump to a target address.
 *
 * Issues the AN3155 Go command causing the target to begin execution at
 * @p addr. BOOT pins are de-asserted so the target boots from flash on
 * subsequent resets.
 *
 * @param dev  Driver device instance.
 * @param addr Target address to jump to.
 *
 * @retval 0        Success, target is executing from @p addr.
 * @retval -ENOTCONN No active bootloader session.
 * @retval <0       Other negative errno on failure.
 */
static int stm32_bootloader_uart_go(const struct device *dev, uint32_t addr)
{
	const struct stm32_bootloader_uart_config *cfg = dev->config;
	struct stm32_bootloader_uart_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);

	if (!data->connected) {
		ret = -ENOTCONN;
		goto out;
	}

	ret = stm32_bootloader_uart_send_cmd(dev, STM32_BOOTLOADER_UART_CMD_GO);
	if (ret < 0) {
		LOG_ERR("Go command rejected: %d", ret);
		goto out;
	}

	ret = stm32_bootloader_uart_send_addr(dev, addr);
	if (ret < 0) {
		LOG_ERR("Go address rejected: %d", ret);
		goto out;
	}

	/* De-assert BOOT pins so the target runs from flash on next reset */
	stm32_bootloader_gpio_exit(&cfg->gpio, false);

	data->connected = false;
	LOG_INF("Started execution from 0x%08x", addr);

out:
	k_mutex_unlock(&data->lock);
	return ret;
}

/**
 * @brief Exit the bootloader and restore UART configuration.
 *
 * Restores the original UART settings, de-asserts BOOT pins, and
 * optionally pulses NRST to hard-reset the target.
 *
 * @param dev          Driver device instance.
 * @param reset_target If true, pulse NRST to reset the target.
 *
 * @retval 0  Success.
 * @retval <0 Negative errno on failure.
 */
static int stm32_bootloader_uart_exit(const struct device *dev, bool reset_target)
{
	const struct stm32_bootloader_uart_config *cfg = dev->config;
	struct stm32_bootloader_uart_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);

	/* Configure the UART driver back to its original configuration */
	ret = uart_configure(cfg->uart_dev, &data->uart_cfg_bkp);
	if (ret < 0) {
		LOG_ERR("Failed to reset UART configuration: %d", ret);
	}

	/* De-assert BOOT pins and optionally reset */
	ret = stm32_bootloader_gpio_exit(&cfg->gpio, reset_target);
	if (ret < 0) {
		goto out;
	}

	data->connected = false;
	LOG_INF("Disconnected from bootloader");

out:
	k_mutex_unlock(&data->lock);
	return ret;
}

/* --------------------------------------------------------------------------
 * Driver API: Get Info
 * -------------------------------------------------------------------------- */

/**
 * @brief Query target bootloader information over USART.
 *
 * Executes the AN3155 Get (0x00), Get Version (0x01), and Get ID (0x02)
 * commands and populates @p info with the results. The information is
 * also cached in driver data for use by erase commands.
 *
 * @param dev  Driver device instance.
 * @param info Output structure to fill with bootloader information.
 *
 * @retval 0        Success.
 * @retval -EINVAL  @p info is NULL.
 * @retval -ENOTCONN No active bootloader session.
 * @retval <0       Other negative errno on failure.
 */
static int stm32_bootloader_uart_get_info(const struct device *dev,
					  struct stm32_bootloader_info *info)
{
	const struct stm32_bootloader_uart_config *cfg = dev->config;
	struct stm32_bootloader_uart_data *data = dev->data;
	uint8_t byte;
	int ret;

	if (info == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	if (!data->connected) {
		ret = -ENOTCONN;
		goto out;
	}

	memset(info, 0, sizeof(*info));

	/* --- Get command (0x00) --- */
	ret = stm32_bootloader_uart_send_cmd(dev, STM32_BOOTLOADER_UART_CMD_GET);
	if (ret < 0) {
		LOG_ERR("Get command failed: %d", ret);
		goto out;
	}

	/* Number of bytes to follow (N) */
	ret = stm32_bootloader_uart_get(cfg->uart_dev, &byte, cfg->timeout_ms);
	if (ret < 0) {
		goto out;
	}
	info->num_cmds = byte;

	/* Bootloader version */
	ret = stm32_bootloader_uart_get(cfg->uart_dev, &info->bl_version, cfg->timeout_ms);
	if (ret < 0) {
		goto out;
	}

	/* Supported commands */
	for (uint8_t i = 0; i < info->num_cmds && i < sizeof(info->cmds); i++) {
		ret = stm32_bootloader_uart_get(cfg->uart_dev, &info->cmds[i], cfg->timeout_ms);
		if (ret < 0) {
			goto out;
		}
	}

	/* Skip any extra command bytes beyond our buffer */
	for (uint8_t i = sizeof(info->cmds); i < info->num_cmds; i++) {
		ret = stm32_bootloader_uart_get(cfg->uart_dev, &byte, cfg->timeout_ms);
		if (ret < 0) {
			goto out;
		}
	}

	/* Trailing ACK */
	ret = stm32_bootloader_uart_wait_ack(dev, cfg->timeout_ms);
	if (ret < 0) {
		goto out;
	}

	/* Check for Extended Erase support */
	info->has_extended_erase = false;
	for (uint8_t i = 0; i < info->num_cmds && i < sizeof(info->cmds); i++) {
		if (info->cmds[i] == STM32_BOOTLOADER_UART_CMD_EE) {
			info->has_extended_erase = true;
			break;
		}
	}

	/* --- Get Version (0x01) --- */
	ret = stm32_bootloader_uart_send_cmd(dev, STM32_BOOTLOADER_UART_CMD_GV);
	if (ret < 0) {
		LOG_ERR("Get Version failed: %d", ret);
		goto out;
	}

	ret = stm32_bootloader_uart_get(cfg->uart_dev, &byte, cfg->timeout_ms);
	if (ret < 0) {
		goto out;
	}
	/* bl_version already set by Get, but Get Version may differ — update */
	info->bl_version = byte;

	ret = stm32_bootloader_uart_get(cfg->uart_dev, &info->option1, cfg->timeout_ms);
	if (ret < 0) {
		goto out;
	}

	ret = stm32_bootloader_uart_get(cfg->uart_dev, &info->option2, cfg->timeout_ms);
	if (ret < 0) {
		goto out;
	}

	ret = stm32_bootloader_uart_wait_ack(dev, cfg->timeout_ms);
	if (ret < 0) {
		goto out;
	}

	/* --- Get ID (0x02) --- */
	ret = stm32_bootloader_uart_send_cmd(dev, STM32_BOOTLOADER_UART_CMD_GID);
	if (ret < 0) {
		LOG_ERR("Get ID failed: %d", ret);
		goto out;
	}

	/* Number of PID bytes - 1 */
	ret = stm32_bootloader_uart_get(cfg->uart_dev, &byte, cfg->timeout_ms);
	if (ret < 0) {
		goto out;
	}

	/* PID: typically 1 byte count meaning 2 bytes of PID */
	{
		uint8_t pid_hi, pid_lo;

		ret = stm32_bootloader_uart_get(cfg->uart_dev, &pid_hi, cfg->timeout_ms);
		if (ret < 0) {
			goto out;
		}

		ret = stm32_bootloader_uart_get(cfg->uart_dev, &pid_lo, cfg->timeout_ms);
		if (ret < 0) {
			goto out;
		}

		info->pid = ((uint16_t)pid_hi << 8) | pid_lo;
	}

	ret = stm32_bootloader_uart_wait_ack(dev, cfg->timeout_ms);
	if (ret < 0) {
		goto out;
	}

	/* Cache info in driver data */
	memcpy(&data->info, info, sizeof(*info));

	LOG_INF("BL v%d.%d, PID 0x%04x, extended_erase=%d", info->bl_version >> 4,
		info->bl_version & 0x0F, info->pid, info->has_extended_erase);

out:
	k_mutex_unlock(&data->lock);
	return ret;
}

/* --------------------------------------------------------------------------
 * Driver API: Erase
 * -------------------------------------------------------------------------- */

/**
 * @brief Perform a mass erase of the target flash over USART.
 *
 * Automatically selects Standard Erase (0x43) or Extended Erase (0x44)
 * based on the cached target capabilities from a prior get_info call.
 *
 * @param dev Driver device instance.
 *
 * @retval 0        Success.
 * @retval -ENOTCONN No active bootloader session.
 * @retval -EPROTO  Command rejected by the target.
 * @retval <0       Other negative errno on failure.
 */
static int stm32_bootloader_uart_mass_erase(const struct device *dev)
{
	const struct stm32_bootloader_uart_config *cfg = dev->config;
	struct stm32_bootloader_uart_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);

	if (!data->connected) {
		ret = -ENOTCONN;
		goto out;
	}

	if (data->info.has_extended_erase) {
		/* Extended Erase (0x44) */
		ret = stm32_bootloader_uart_send_cmd(dev, STM32_BOOTLOADER_UART_CMD_EE);
		if (ret < 0) {
			LOG_ERR("Extended Erase command rejected: %d", ret);
			goto out;
		}

		/* Send 0xFFFF (mass erase) + checksum */
		stm32_bootloader_uart_put(cfg->uart_dev, 0xFF);
		stm32_bootloader_uart_put(cfg->uart_dev, 0xFF);
		stm32_bootloader_uart_put(cfg->uart_dev, 0x00); /* XOR of 0xFF ^ 0xFF = 0x00 */

		ret = stm32_bootloader_uart_wait_ack(dev, cfg->mcu.erase_timeout_ms);
	} else {
		/* Standard Erase (0x43) */
		ret = stm32_bootloader_uart_send_cmd(dev, STM32_BOOTLOADER_UART_CMD_ER);
		if (ret < 0) {
			LOG_ERR("Erase command rejected: %d", ret);
			goto out;
		}

		/* Send 0xFF (mass erase) + checksum */
		stm32_bootloader_uart_put(cfg->uart_dev, STM32_BOOTLOADER_UART_MASS_ERASE);
		stm32_bootloader_uart_put(cfg->uart_dev,
					  0x00); /* XOR of 0xFF = complement trick: checksum */

		ret = stm32_bootloader_uart_wait_ack(dev, cfg->mcu.erase_timeout_ms);
	}

	if (ret < 0) {
		LOG_ERR("Mass erase failed: %d", ret);
	} else {
		LOG_INF("Mass erase complete");
	}

out:
	k_mutex_unlock(&data->lock);
	return ret;
}

/**
 * @brief Erase specific flash pages/sectors on the target over USART.
 *
 * Automatically selects Standard Erase (0x43) or Extended Erase (0x44)
 * based on the cached target capabilities.
 *
 * @param dev       Driver device instance.
 * @param pages     Array of page/sector numbers to erase.
 * @param num_pages Number of entries in @p pages.
 *
 * @retval 0        Success.
 * @retval -EINVAL  @p pages is NULL or @p num_pages is zero.
 * @retval -ENOTCONN No active bootloader session.
 * @retval <0       Other negative errno on failure.
 */
static int stm32_bootloader_uart_erase_pages(const struct device *dev, const uint16_t *pages,
					     size_t num_pages)
{
	const struct stm32_bootloader_uart_config *cfg = dev->config;
	struct stm32_bootloader_uart_data *data = dev->data;
	int ret;

	if (pages == NULL || num_pages == 0) {
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	if (!data->connected) {
		ret = -ENOTCONN;
		goto out;
	}

	if (data->info.has_extended_erase) {
		/* Extended Erase (0x44) */
		ret = stm32_bootloader_uart_send_cmd(dev, STM32_BOOTLOADER_UART_CMD_EE);
		if (ret < 0) {
			LOG_ERR("Extended Erase command rejected: %d", ret);
			goto out;
		}

		/* N-1 (number of pages - 1) as 2 bytes big-endian */
		uint16_t n_minus_1 = (uint16_t)(num_pages - 1);
		uint8_t xor = 0;
		uint8_t hi, lo;

		hi = (uint8_t)(n_minus_1 >> 8);
		lo = (uint8_t)(n_minus_1 & 0xFF);
		stm32_bootloader_uart_put(cfg->uart_dev, hi);
		stm32_bootloader_uart_put(cfg->uart_dev, lo);
		xor ^= hi;
		xor ^= lo;

		/* Page numbers, each 2 bytes big-endian */
		for (size_t i = 0; i < num_pages; i++) {
			hi = (uint8_t)(pages[i] >> 8);
			lo = (uint8_t)(pages[i] & 0xFF);
			stm32_bootloader_uart_put(cfg->uart_dev, hi);
			stm32_bootloader_uart_put(cfg->uart_dev, lo);
			xor ^= hi;
			xor ^= lo;
		}

		stm32_bootloader_uart_put(cfg->uart_dev, xor);

		ret = stm32_bootloader_uart_wait_ack(dev, cfg->mcu.erase_timeout_ms);
	} else {
		/* Standard Erase (0x43) */
		ret = stm32_bootloader_uart_send_cmd(dev, STM32_BOOTLOADER_UART_CMD_ER);
		if (ret < 0) {
			LOG_ERR("Erase command rejected: %d", ret);
			goto out;
		}

		/* N-1 (number of pages - 1) as 1 byte */
		uint8_t n_minus_1 = (uint8_t)(num_pages - 1);
		uint8_t xor = n_minus_1;

		stm32_bootloader_uart_put(cfg->uart_dev, n_minus_1);

		/* Page numbers, each 1 byte */
		for (size_t i = 0; i < num_pages; i++) {
			uint8_t page = (uint8_t)pages[i];

			stm32_bootloader_uart_put(cfg->uart_dev, page);
			xor ^= page;
		}

		stm32_bootloader_uart_put(cfg->uart_dev, xor);

		ret = stm32_bootloader_uart_wait_ack(dev, cfg->mcu.erase_timeout_ms);
	}

	if (ret < 0) {
		LOG_ERR("Page erase failed: %d", ret);
	} else {
		LOG_DBG("Erased %zu pages", num_pages);
	}

out:
	k_mutex_unlock(&data->lock);
	return ret;
}

/* --------------------------------------------------------------------------
 * Driver API: Write / Read
 * -------------------------------------------------------------------------- */

/**
 * @brief Write data to target memory over USART.
 *
 * Sends data in chunks of @c CONFIG_STM32_BOOTLOADER_WRITE_CHUNK_SIZE bytes
 * using the AN3155 Write Memory command (0x31). An optional progress
 * callback is invoked after each chunk.
 *
 * @param dev       Driver device instance.
 * @param addr      Target start address (must be word-aligned per AN3155).
 * @param data_buf  Data buffer to write.
 * @param len       Number of bytes to write.
 * @param cb        Optional progress callback (may be NULL).
 * @param user_data User context passed to @p cb.
 *
 * @retval 0        Success.
 * @retval -EINVAL  @p data_buf is NULL or @p len is zero.
 * @retval -ENOTCONN No active bootloader session.
 * @retval <0       Other negative errno on failure.
 */
static int stm32_bootloader_uart_write(const struct device *dev, uint32_t addr,
				       const uint8_t *data_buf, size_t len,
				       stm32_bootloader_progress_cb_t cb, void *user_data)
{
	const struct stm32_bootloader_uart_config *cfg = dev->config;
	struct stm32_bootloader_uart_data *data = dev->data;
	size_t written = 0;
	int ret;

	if (data_buf == NULL || len == 0) {
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	if (!data->connected) {
		ret = -ENOTCONN;
		goto out;
	}

	while (written < len) {
		size_t chunk = MIN(len - written, CONFIG_STM32_BOOTLOADER_WRITE_CHUNK_SIZE);
		uint8_t n = (uint8_t)(chunk - 1); /* N: number of bytes - 1 */

		ret = stm32_bootloader_uart_send_cmd(dev, STM32_BOOTLOADER_UART_CMD_WM);
		if (ret < 0) {
			LOG_ERR("Write Memory command rejected: %d", ret);
			goto out;
		}

		ret = stm32_bootloader_uart_send_addr(dev, addr + written);
		if (ret < 0) {
			LOG_ERR("Write address rejected: %d", ret);
			goto out;
		}

		/* Send N, then N+1 data bytes, then checksum */
		uint8_t xor = n;

		stm32_bootloader_uart_put(cfg->uart_dev, n);

		for (size_t i = 0; i < chunk; i++) {
			stm32_bootloader_uart_put(cfg->uart_dev, data_buf[written + i]);
			xor ^= data_buf[written + i];
		}

		stm32_bootloader_uart_put(cfg->uart_dev, xor);

		ret = stm32_bootloader_uart_wait_ack(dev, cfg->timeout_ms);
		if (ret < 0) {
			LOG_ERR("Write data rejected at offset 0x%x: %d",
				(unsigned int)(addr + written), ret);
			goto out;
		}

		written += chunk;

		if (cb != NULL) {
			cb(written, len, user_data);
		}
	}

	LOG_DBG("Wrote %zu bytes to 0x%08x", len, addr);

out:
	k_mutex_unlock(&data->lock);
	return ret;
}

/**
 * @brief Read data from target memory over USART.
 *
 * Reads data in chunks of @c STM32_BOOTLOADER_MAX_DATA_SIZE bytes using
 * the AN3155 Read Memory command (0x11).
 *
 * @param dev  Driver device instance.
 * @param addr Target start address.
 * @param buf  Buffer to store the read data.
 * @param len  Number of bytes to read.
 *
 * @retval 0        Success.
 * @retval -EINVAL  @p buf is NULL or @p len is zero.
 * @retval -ENOTCONN No active bootloader session.
 * @retval <0       Other negative errno on failure.
 */
static int stm32_bootloader_uart_read(const struct device *dev, uint32_t addr, uint8_t *buf,
				      size_t len)
{
	const struct stm32_bootloader_uart_config *cfg = dev->config;
	struct stm32_bootloader_uart_data *data = dev->data;
	size_t total_read = 0;
	int ret;

	if (buf == NULL || len == 0) {
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	if (!data->connected) {
		ret = -ENOTCONN;
		goto out;
	}

	while (total_read < len) {
		size_t chunk = MIN(len - total_read, STM32_BOOTLOADER_MAX_DATA_SIZE);

		ret = stm32_bootloader_uart_send_cmd(dev, STM32_BOOTLOADER_UART_CMD_RM);
		if (ret < 0) {
			LOG_ERR("Read Memory command rejected: %d", ret);
			goto out;
		}

		ret = stm32_bootloader_uart_send_addr(dev, addr + total_read);
		if (ret < 0) {
			LOG_ERR("Read address rejected: %d", ret);
			goto out;
		}

		/* Send N (number of bytes - 1) and its complement */
		uint8_t n = (uint8_t)(chunk - 1);

		stm32_bootloader_uart_put(cfg->uart_dev, n);
		stm32_bootloader_uart_put(cfg->uart_dev, ~n);

		ret = stm32_bootloader_uart_wait_ack(dev, cfg->timeout_ms);
		if (ret < 0) {
			LOG_ERR("Read length rejected: %d", ret);
			goto out;
		}

		/* Receive data bytes */
		for (size_t i = 0; i < chunk; i++) {
			ret = stm32_bootloader_uart_get(cfg->uart_dev, &buf[total_read + i],
							cfg->timeout_ms);
			if (ret < 0) {
				LOG_ERR("Read data timeout at offset %zu", total_read + i);
				goto out;
			}
		}

		total_read += chunk;
	}

	LOG_DBG("Read %zu bytes from 0x%08x", len, addr);

out:
	k_mutex_unlock(&data->lock);
	return ret;
}

/* --------------------------------------------------------------------------
 * Driver API table
 * -------------------------------------------------------------------------- */

static DEVICE_API(stm32_bootloader, stm32_bootloader_uart_api) = {
	.enter = stm32_bootloader_uart_enter,
	.exit = stm32_bootloader_uart_exit,
	.go = stm32_bootloader_uart_go,
	.get_info = stm32_bootloader_uart_get_info,
	.mass_erase = stm32_bootloader_uart_mass_erase,
	.erase_pages = stm32_bootloader_uart_erase_pages,
	.write = stm32_bootloader_uart_write,
	.read = stm32_bootloader_uart_read,
};

/* --------------------------------------------------------------------------
 * Init / Device instantiation
 * -------------------------------------------------------------------------- */

/**
 * @brief Initialise a USART bootloader driver instance.
 *
 * Verifies that the UART device is ready, initialises the common
 * bootloader GPIOs, and prepares the driver mutex.
 *
 * @param dev Driver device instance.
 *
 * @retval 0       Success.
 * @retval -ENODEV UART device or a required GPIO is not ready.
 * @retval <0      Other negative errno on failure.
 */
static int stm32_bootloader_uart_init(const struct device *dev)
{
	const struct stm32_bootloader_uart_config *cfg = dev->config;
	struct stm32_bootloader_uart_data *data = dev->data;
	int ret;

	if (!device_is_ready(cfg->uart_dev)) {
		LOG_ERR("UART device not ready");
		return -ENODEV;
	}

	ret = stm32_bootloader_gpio_init(&cfg->gpio);
	if (ret < 0) {
		return ret;
	}

	k_mutex_init(&data->lock);
	data->connected = false;

	LOG_INF("STM32 USART bootloader driver initialized");
	return 0;
}

#define STM32_BOOTLOADER_OPTIONAL_GPIO(inst, gpio)                                                 \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, gpio),                                            \
		    (GPIO_DT_SPEC_INST_GET(inst, gpio)),                                           \
		    ({.port = NULL}))

#define STM32_BOOTLOADER_UART_DEFINE(inst)                                                         \
	static struct stm32_bootloader_uart_data stm32_bootloader_uart_data_##inst;                \
                                                                                                   \
	static const struct stm32_bootloader_uart_config stm32_bootloader_uart_config_##inst = {   \
		.gpio =                                                                            \
			{                                                                          \
				.boot0_gpio = GPIO_DT_SPEC_INST_GET(inst, boot0_gpios),            \
				.boot1_gpio = STM32_BOOTLOADER_OPTIONAL_GPIO(inst, boot1_gpios),   \
				.nrst_gpio = GPIO_DT_SPEC_INST_GET(inst, nrst_gpios),              \
				.reset_pulse_ms = DT_INST_PROP(inst, reset_pulse_ms),              \
				.boot_delay_ms = DT_INST_PROP(inst, boot_delay_ms),                \
			},                                                                         \
		.mcu =                                                                             \
			{                                                                          \
				.erase_timeout_ms = DT_INST_PROP(inst, erase_timeout_ms),          \
			},                                                                         \
		.uart_dev = DEVICE_DT_GET(DT_INST_BUS(inst)),                                      \
		.baudrate = DT_INST_PROP(inst, baudrate),                                          \
		.parity_none = DT_INST_PROP(inst, parity_none),                                    \
		.timeout_ms = DT_INST_PROP(inst, timeout_ms),                                      \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(                                                                     \
		inst, stm32_bootloader_uart_init, NULL, &stm32_bootloader_uart_data_##inst,        \
		&stm32_bootloader_uart_config_##inst, POST_KERNEL,                                 \
		CONFIG_STM32_BOOTLOADER_UART_INIT_PRIORITY, &stm32_bootloader_uart_api);

DT_INST_FOREACH_STATUS_OKAY(STM32_BOOTLOADER_UART_DEFINE)
