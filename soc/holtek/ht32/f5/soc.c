/*
 * Copyright (c) 2025 Alex Fabre <alex.fabre@rtone.fr>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief System module to support early initialization
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <soc.h>

/**
 * @brief Perform basic hardware initialization at boot.
 *
 * This needs to be run from the very beginning.
 * So the init priority has to be 0 (zero).
 *
 * @return 0
 */
static int holtek_ht32_init(void)
{
	/* Install default handler that simply resets the CPU
	 * if configured in the kernel, NOP otherwise
	 */
	NMI_INIT();

	return 0;
}

SYS_INIT(holtek_ht32_init, PRE_KERNEL_1, 0);
