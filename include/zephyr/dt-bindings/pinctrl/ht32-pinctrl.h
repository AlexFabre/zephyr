/*
 * Copyright (c) 2025 Alex Fabre <alex.fabre@rtone.fr>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_PINCTRL_HT32_PINCTRL_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_PINCTRL_HT32_PINCTRL_H_

/**
 * @brief HT32 pin configuration
 *
 * Pin configuration is encoded as follows:
 * - bits [31:24]: Port (0=A, 1=B, 2=C, etc.)
 * - bits [23:16]: Pin number (0-15)
 * - bits [15:8]:  Alternate function (0-15)
 * - bits [7:0]:   Reserved
 */

#define HT32_PINCTRL(port, pin, af) \
	(((port) << 24) | ((pin) << 16) | ((af) << 8))

/* Port definitions */
#define HT32_PORT_A  0
#define HT32_PORT_B  1
#define HT32_PORT_C  2

/* Alternate functions (examples, actual values depend on MCU) */
#define HT32_AF_GPIO     0
#define HT32_AF_USART    1
#define HT32_AF_UART     2
#define HT32_AF_SPI      3
#define HT32_AF_I2C      4
#define HT32_AF_ADC      5

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_PINCTRL_HT32_PINCTRL_H_ */
