/*
 * Copyright (c) 2025 Alex Fabre <alex.fabre@rtone.fr>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT holtek_ht32_gpio

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/irq.h>
#include <soc.h>
#include <zephyr/sys/sys_io.h>

#include <zephyr/drivers/gpio/gpio_utils.h>

/* GPIO Register offsets */
#define HT32_GPIO_DIRCR_OFFSET   0x000  /* Data Direction Control Register */
#define HT32_GPIO_INER_OFFSET    0x004  /* Input Function Enable Register */
#define HT32_GPIO_PUR_OFFSET     0x008  /* Pull-Up Selection Register */
#define HT32_GPIO_PDR_OFFSET     0x00C  /* Pull-Down Selection Register */
#define HT32_GPIO_ODR_OFFSET     0x010  /* Open Drain Selection Register */
#define HT32_GPIO_DRVR_OFFSET    0x014  /* Drive Current Selection Register */
#define HT32_GPIO_LOCKR_OFFSET   0x018  /* Lock Register */
#define HT32_GPIO_DINR_OFFSET    0x01C  /* Data Input Register */
#define HT32_GPIO_DOUTR_OFFSET   0x020  /* Data Output Register */
#define HT32_GPIO_SRR_OFFSET     0x024  /* Output Set and Reset Control Register */
#define HT32_GPIO_RR_OFFSET      0x028  /* Output Reset Control Register */

struct gpio_ht32_config {
	struct gpio_driver_config common;
	uint32_t base;
	uint8_t port_num;
};

struct gpio_ht32_data {
	struct gpio_driver_data common;
};

static int gpio_ht32_pin_configure(const struct device *dev,
				   gpio_pin_t pin,
				   gpio_flags_t flags)
{
	const struct gpio_ht32_config *config = dev->config;
	uint32_t base = config->base;

	if (pin >= 16) {
		return -EINVAL;
	}

	uint32_t pin_mask = BIT(pin);

	/* Configure direction */
	if ((flags & GPIO_OUTPUT) != 0) {
		/* Set as output */
		sys_set_bits(base + HT32_GPIO_DIRCR_OFFSET, pin_mask);

		/* Configure initial output value */
		if ((flags & GPIO_OUTPUT_INIT_HIGH) != 0) {
			sys_write32(pin_mask, base + HT32_GPIO_SRR_OFFSET);
		} else if ((flags & GPIO_OUTPUT_INIT_LOW) != 0) {
			sys_write32(pin_mask, base + HT32_GPIO_RR_OFFSET);
		}
	} else {
		/* Set as input */
		sys_clear_bits(base + HT32_GPIO_DIRCR_OFFSET, pin_mask);
		/* Enable input function */
		sys_set_bits(base + HT32_GPIO_INER_OFFSET, pin_mask);
	}

	/* Configure pull-up/pull-down */
	if ((flags & GPIO_PULL_UP) != 0) {
		sys_set_bits(base + HT32_GPIO_PUR_OFFSET, pin_mask);
		sys_clear_bits(base + HT32_GPIO_PDR_OFFSET, pin_mask);
	} else if ((flags & GPIO_PULL_DOWN) != 0) {
		sys_clear_bits(base + HT32_GPIO_PUR_OFFSET, pin_mask);
		sys_set_bits(base + HT32_GPIO_PDR_OFFSET, pin_mask);
	} else {
		/* No pull */
		sys_clear_bits(base + HT32_GPIO_PUR_OFFSET, pin_mask);
		sys_clear_bits(base + HT32_GPIO_PDR_OFFSET, pin_mask);
	}

	/* Configure open drain */
	if ((flags & GPIO_OPEN_DRAIN) != 0) {
		sys_set_bits(base + HT32_GPIO_ODR_OFFSET, pin_mask);
	} else {
		sys_clear_bits(base + HT32_GPIO_ODR_OFFSET, pin_mask);
	}

	return 0;
}

static int gpio_ht32_port_get_raw(const struct device *dev,
				  gpio_port_value_t *value)
{
	const struct gpio_ht32_config *config = dev->config;
	uint32_t base = config->base;

	*value = sys_read32(base + HT32_GPIO_DINR_OFFSET);

	return 0;
}

static int gpio_ht32_port_set_masked_raw(const struct device *dev,
					 gpio_port_pins_t mask,
					 gpio_port_value_t value)
{
	const struct gpio_ht32_config *config = dev->config;
	uint32_t base = config->base;
	uint32_t port_val;

	port_val = sys_read32(base + HT32_GPIO_DOUTR_OFFSET);
	port_val = (port_val & ~mask) | (value & mask);
	sys_write32(port_val, base + HT32_GPIO_DOUTR_OFFSET);

	return 0;
}

static int gpio_ht32_port_set_bits_raw(const struct device *dev,
				       gpio_port_pins_t pins)
{
	const struct gpio_ht32_config *config = dev->config;
	uint32_t base = config->base;

	/* Use SRR (Set and Reset Register) for atomic set */
	sys_write32(pins, base + HT32_GPIO_SRR_OFFSET);

	return 0;
}

static int gpio_ht32_port_clear_bits_raw(const struct device *dev,
					 gpio_port_pins_t pins)
{
	const struct gpio_ht32_config *config = dev->config;
	uint32_t base = config->base;

	/* Use RR (Reset Register) for atomic clear */
	sys_write32(pins, base + HT32_GPIO_RR_OFFSET);

	return 0;
}

static int gpio_ht32_port_toggle_bits(const struct device *dev,
				      gpio_port_pins_t pins)
{
	const struct gpio_ht32_config *config = dev->config;
	uint32_t base = config->base;
	uint32_t port_val;

	port_val = sys_read32(base + HT32_GPIO_DOUTR_OFFSET);
	sys_write32(port_val ^ pins, base + HT32_GPIO_DOUTR_OFFSET);

	return 0;
}

static int gpio_ht32_pin_interrupt_configure(const struct device *dev,
					     gpio_pin_t pin,
					     enum gpio_int_mode mode,
					     enum gpio_int_trig trig)
{
	/* TODO: Implement interrupt support using EXTI */
	return -ENOTSUP;
}

static int gpio_ht32_manage_callback(const struct device *dev,
				     struct gpio_callback *callback,
				     bool set)
{
	/* TODO: Implement callback management */
	return -ENOTSUP;
}

static int gpio_ht32_init(const struct device *dev)
{
	/* GPIO ports are always enabled on HT32 */
	/* Clock gating will be handled by clock control driver */
	return 0;
}

static const struct gpio_driver_api gpio_ht32_driver_api = {
	.pin_configure = gpio_ht32_pin_configure,
	.port_get_raw = gpio_ht32_port_get_raw,
	.port_set_masked_raw = gpio_ht32_port_set_masked_raw,
	.port_set_bits_raw = gpio_ht32_port_set_bits_raw,
	.port_clear_bits_raw = gpio_ht32_port_clear_bits_raw,
	.port_toggle_bits = gpio_ht32_port_toggle_bits,
	.pin_interrupt_configure = gpio_ht32_pin_interrupt_configure,
	.manage_callback = gpio_ht32_manage_callback,
};

#define GPIO_HT32_INIT(n)						\
	static const struct gpio_ht32_config gpio_ht32_config_##n = {	\
		.common = {						\
			.port_pin_mask = GPIO_PORT_PIN_MASK_FROM_DT_INST(n), \
		},							\
		.base = DT_INST_REG_ADDR(n),				\
		.port_num = n,						\
	};								\
									\
	static struct gpio_ht32_data gpio_ht32_data_##n;		\
									\
	DEVICE_DT_INST_DEFINE(n, gpio_ht32_init,			\
			      NULL,					\
			      &gpio_ht32_data_##n,			\
			      &gpio_ht32_config_##n,			\
			      PRE_KERNEL_1,				\
			      CONFIG_GPIO_INIT_PRIORITY,		\
			      &gpio_ht32_driver_api);

DT_INST_FOREACH_STATUS_OKAY(GPIO_HT32_INIT)
