/*
 * Copyright (c) 2025, Alex Fabre
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_tps3851

#include <errno.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wdt_ti_tps3851, CONFIG_WDT_LOG_LEVEL);

/* Minimum WDI pulse duration */
#define WDI_PULSE_DURATION_NS (50)

struct ti_tps3851_config {
	struct gpio_dt_spec wdo_gpio;
	struct gpio_dt_spec wdi_gpio;
	bool wdi_off_level;
	struct gpio_dt_spec set1_gpio;
	int timeout;
};

struct ti_tps3851_data {
	struct gpio_callback wdo_cb;
	wdt_callback_t cb;
}

static void
wdt_ti_tps3851_wdo_int_callback(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
	const struct ti_tps3851_data *data = CONTAINER_OF(cb, struct ti_tps3851_data, wdo_cb);
	const struct device *wdt_dev = CONTAINER_OF(data, struct device, data);

	ARG_UNUSED(port);
	ARG_UNUSED(pins);

	LOG_DBG("WDO ISR");
	if (data->cb) {
		data->cb(wdt_dev, 0);
	}
}

static int ti_tps3851_init(const struct device *dev)
{
	const struct ti_tps3851_config *cfg = dev->config;
	struct ti_tps3851_data *data = dev->data;

	/* Configure GPIO used for watchdog signal (WDO) */
	if (!gpio_is_ready_dt(&cfg->wdo_gpio)) {
		LOG_ERR("WDO not ready");
		return -ENODEV;
	}

	if (gpio_pin_configure_dt(&cfg->wdo_gpio, GPIO_INPUT)) {
		LOG_ERR("Unable to set WDO input");
		return -EIO;
	}

	gpio_init_callback(&data->wdo_cb, wdt_ti_tps3851_wdo_int_callback, BIT(cfg->wdo_gpio.pin));

	if (gpio_add_callback(cfg->wdo_gpio.port, &data->wdo_cb)) {
		LOG_ERR("Unable to add WDO int cb");
		return -EINVAL;
	}

	if (gpio_pin_interrupt_configure_dt(&cfg->wdo_gpio, GPIO_INT_EDGE_FALLING)) {
		LOG_ERR("Unable to set WDO int mode");
		return -EINVAL;
	}

	/* Configure GPIO used for watchdog feed (WDI) */
	if (!gpio_is_ready_dt(&cfg->wdi_gpio)) {
		LOG_ERR("WDI not ready");
		return -ENODEV;
	}

	if (gpio_pin_configure_dt(&cfg->wdi_gpio, GPIO_OUTPUT_HIGH)) {
		LOG_ERR("Unable to set WDI output");
		return -EIO;
	}

	/* Configure GPIO used for enable/disable the watchdog (SET1) */
	if (!gpio_is_ready_dt(&cfg->set1_gpio)) {
		LOG_ERR("SET1 not ready");
		return -ENODEV;
	}

	if (gpio_pin_configure_dt(&cfg->set1_gpio, GPIO_OUTPUT_LOW)) {
		LOG_ERR("Unable to set SET1 output");
		return -EIO;
	}

	return 0;
}

static int ti_tps3851_setup(const struct device *dev, uint8_t options)
{
	const struct ti_tps3851_config *cfg = dev->config;

	if (gpio_pin_set_dt(&cfg->wdi_gpio, 1)) {
		LOG_ERR("Unable to set WDI");
		return -EIO;
	}

	if (gpio_pin_set_dt(&cfg->set1_gpio, 1)) {
		LOG_ERR("Unable to set SET1");
		return -EIO;
	}
}

static int ti_tps3851_disable(const struct device *dev)
{
	const struct ti_tps3851_config *cfg = dev->config;

	/* Grounding the SET1 pin disables the watchdog timer. */
	if (gpio_pin_set_dt(&cfg->set1_gpio, 0)) {
		LOG_ERR("Unable to reset SET1");
		return -EIO;
	}

	/* If the watchdog is disabled, WDI cannot be left
	 * unconnected and must be driven to either VDD or GND. */
	if (gpio_pin_set_dt(&cfg->wdi_gpio, &cfg->wdi_off_level)) {
		LOG_ERR("Unable to %s WDI", (&cfg->wdi_off_level) ? "set" : "reset");
		return -EIO;
	}
}

static int ti_tps3851_install_timeout(const struct device *dev, const struct wdt_timeout_cfg *cfg)
{
	const struct ti_tps3851_config *config = dev->config;

	if (cfg->window.max != config->timeout) {
		LOG_WRN("Max window timeout is fixed by hardware design to %d ms", config->timeout);
	}

	if (cfg->window.min != 0) {
		LOG_WRN("Min window timeout is fixed to 0 ms");
	}

	if (cfg->callback == NULL) {
		LOG_ERR("Watchdog callback can't be NULL");
		return -EINVAL;
	}

	data->cb = cfg->callback;

	return 0;
}

static int ti_tps3851_feed(const struct device *dev, int channel_id)
{
	const struct ti_tps3851_config *config = dev->config;

	/* A falling edge must occur at WDI before the timeout (tWD) expires. */
	if (gpio_pin_set_dt(&config->wdi_gpio, 0)) {
		LOG_ERR("Unable to reset WDI");
		return -EIO;
	}

	/* Minimum WDI pulse duration */
	k_sleep(K_NSEC(WDI_PULSE_DURATION_NS));

	/* Set WDI back to HIGH to get ready for next feed */
	if (gpio_pin_set_dt(&config->wdi_gpio, 0)) {
		LOG_ERR("Unable to set WDI");
		return -EIO;
	}
}

static DEVICE_API(wdt, ti_tps3851_api) = {
	.setup = ti_tps3851_setup,
	.disable = ti_tps3851_disable,
	.install_timeout = ti_tps3851_install_timeout,
	.feed = ti_tps3851_feed,
};

#define WDT_TI_TPS3851_INIT(n)                                                                     \
	static const struct ti_tps3851_config ti_tps3851_##n##config = {                           \
		.wdi_gpio = GPIO_DT_SPEC_INST_GET(n, wdi_gpios),                                   \
		.timeout = DT_INST_PROP(n, timeout_period),                                        \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, ti_tps3851_init, NULL, NULL, &ti_tps3851_##n##config,             \
			      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &ti_tps3851_api);

DT_INST_FOREACH_STATUS_OKAY(WDT_TI_TPS3851_INIT);
