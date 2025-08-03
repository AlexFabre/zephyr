/*
 * Copyright 2025 Alex Fabre
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_swd

#include <zephyr/drivers/swd.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(swd, CONFIG_SWD_LOG_LEVEL);

