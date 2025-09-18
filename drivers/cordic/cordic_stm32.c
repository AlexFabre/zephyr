/*
 * Copyright (c) 2025 Alex Fabre <alex.fabre@rtone.fr>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/reset.h>
#include <zephyr/sys/byteorder.h>
#include <soc.h>

#define LOG_LEVEL CONFIG_CORDIC_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cordic_stm32);

#if DT_HAS_COMPAT_STATUS_OKAY(st_stm32_cordic)
#define DT_DRV_COMPAT st_stm32_cordic
#else
#error No STM32 CORDIC coprocessor in device tree
#endif

int get_capabilities