/*
 * Copyright (c) 2026 Alex Fabre
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief STM32 USART bootloader protocol constants (AN3155).
 *
 * Defines synchronization bytes, response codes, command identifiers,
 * and special erase codes used by the AN3155 USART bootloader protocol.
 *
 * @defgroup stm32_bootloader STM32 system bootloader
 * @ingroup stm32_bootloader
 * @{
 */

#ifndef ZEPHYR_DRIVERS_MISC_STM32_BOOTLOADER_UART_H_
#define ZEPHYR_DRIVERS_MISC_STM32_BOOTLOADER_UART_H_

/** @name Synchronization and response bytes
 * @{
 */
/** USART synchronization byte sent to initiate communication. */
#define STM32_BOOTLOADER_UART_SYNC 0x7F
/** Acknowledge byte returned by the bootloader on success. */
#define STM32_BOOTLOADER_UART_ACK  0x79
/** Not-acknowledge byte returned by the bootloader on failure. */
#define STM32_BOOTLOADER_UART_NACK 0x1F
/** @} */

/** @name AN3155 bootloader command identifiers
 * @{
 */
/** Get supported commands and bootloader version. */
#define STM32_BOOTLOADER_UART_CMD_GET 0x00
/** Get Version & Read Protection Status. */
#define STM32_BOOTLOADER_UART_CMD_GV  0x01
/** Get chip ID (product identifier). */
#define STM32_BOOTLOADER_UART_CMD_GID 0x02
/** Read Memory command. */
#define STM32_BOOTLOADER_UART_CMD_RM  0x11
/** Go command (jump to a given address). */
#define STM32_BOOTLOADER_UART_CMD_GO  0x21
/** Write Memory command. */
#define STM32_BOOTLOADER_UART_CMD_WM  0x31
/** Standard Erase command (devices without extended erase). */
#define STM32_BOOTLOADER_UART_CMD_ER  0x43
/** Extended Erase command. */
#define STM32_BOOTLOADER_UART_CMD_EE  0x44
/** @} */

/** @name Special erase codes
 * @{
 */
/** Standard erase: mass-erase all pages. */
#define STM32_BOOTLOADER_UART_MASS_ERASE 0xFF
/** Extended erase: mass-erase all pages. */
#define STM32_BOOTLOADER_UART_EE_ERASE   0xFFFF
/** @} */

/*
 * Per-command data limits are defined as STM32_BOOTLOADER_MAX_DATA_SIZE in
 * the public header <zephyr/drivers/misc/stm32_bootloader/stm32_bootloader.h>.
 */

#endif /* ZEPHYR_DRIVERS_MISC_STM32_BOOTLOADER_UART_H_ */
