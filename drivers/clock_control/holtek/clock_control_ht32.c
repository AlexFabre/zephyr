/*
 * Copyright (c) 2025 Alex Fabre <alex.fabre@rtone.fr>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT holtek_ht32_ckcu

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/sys/sys_io.h>

/* CKCU Register offsets (simplified) */
#define HT32_CKCU_GCFGR_OFFSET   0x000  /* Global Clock Configuration Register */
#define HT32_CKCU_GCCR_OFFSET    0x004  /* Global Clock Control Register */
#define HT32_CKCU_AHBCFGR_OFFSET 0x018  /* AHB Configuration Register */
#define HT32_CKCU_AHBCCR_OFFSET  0x01C  /* AHB Clock Control Register */
#define HT32_CKCU_APBCFGR_OFFSET 0x020  /* APB Configuration Register */
#define HT32_CKCU_APBCCR0_OFFSET 0x024  /* APB Clock Control Register 0 */
#define HT32_CKCU_APBCCR1_OFFSET 0x028  /* APB Clock Control Register 1 */
#define HT32_CKCU_CKST_OFFSET    0x034  /* Clock Source Status Register */

struct clock_control_ht32_config {
	uint32_t base;
};

static int clock_control_ht32_on(const struct device *dev,
				 clock_control_subsys_t sub_system)
{
	const struct clock_control_ht32_config *config = dev->config;
	uint32_t base = config->base;
	uint32_t offset = POINTER_TO_UINT(sub_system);

	/* Enable clock by setting the appropriate bit */
	sys_set_bits(base + offset, BIT(0));

	return 0;
}

static int clock_control_ht32_off(const struct device *dev,
				  clock_control_subsys_t sub_system)
{
	const struct clock_control_ht32_config *config = dev->config;
	uint32_t base = config->base;
	uint32_t offset = POINTER_TO_UINT(sub_system);

	/* Disable clock by clearing the appropriate bit */
	sys_clear_bits(base + offset, BIT(0));

	return 0;
}

static int clock_control_ht32_get_rate(const struct device *dev,
					clock_control_subsys_t sub_system,
					uint32_t *rate)
{
	/* For now, return the system clock frequency (48 MHz) */
	*rate = 48000000;

	return 0;
}

static int clock_control_ht32_init(const struct device *dev)
{
	/* Clock initialization would normally:
	 * 1. Configure PLL for 48 MHz operation
	 * 2. Set up AHB/APB dividers
	 * 3. Enable necessary peripheral clocks
	 *
	 * For now, we assume the bootloader or reset defaults
	 * have set up basic clocking.
	 */

	return 0;
}

static const struct clock_control_driver_api clock_control_ht32_api = {
	.on = clock_control_ht32_on,
	.off = clock_control_ht32_off,
	.get_rate = clock_control_ht32_get_rate,
};

#define CLOCK_CONTROL_HT32_INIT(n)						\
	static const struct clock_control_ht32_config				\
		clock_control_ht32_config_##n = {				\
		.base = DT_INST_REG_ADDR(n),					\
	};									\
										\
	DEVICE_DT_INST_DEFINE(n,						\
			      &clock_control_ht32_init,				\
			      NULL,						\
			      NULL,						\
			      &clock_control_ht32_config_##n,			\
			      PRE_KERNEL_1,					\
			      CONFIG_CLOCK_CONTROL_INIT_PRIORITY,		\
			      &clock_control_ht32_api);

DT_INST_FOREACH_STATUS_OKAY(CLOCK_CONTROL_HT32_INIT)
