/*
 * Copyright (c) 2025 Alex Fabre <alex.fabre@rtone.fr>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Support both UART and USART compatible strings */
#define DT_DRV_COMPAT holtek_ht32_uart
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT holtek_ht32_usart

#include <zephyr/kernel.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/sys_io.h>

/* UART Register offsets */
#define HT32_UART_DR_OFFSET      0x000  /* Data Register */
#define HT32_UART_CR_OFFSET      0x004  /* Control Register */
#define HT32_UART_FCR_OFFSET     0x008  /* FIFO Control Register */
#define HT32_UART_IER_OFFSET     0x00C  /* Interrupt Enable Register */
#define HT32_UART_SR_OFFSET      0x010  /* Status Register */
#define HT32_UART_TPR_OFFSET     0x014  /* Timing Parameter Register */
#define HT32_UART_DLR_OFFSET     0x024  /* Divisor Latch Register */

/* Control Register (CR) bits */
#define HT32_UART_CR_URTXEN      BIT(0)   /* TX Enable */
#define HT32_UART_CR_URRXEN      BIT(1)   /* RX Enable */

/* Status Register (SR) bits */
#define HT32_UART_SR_TXDE        BIT(0)   /* TX Data Register Empty */
#define HT32_UART_SR_TXFE        BIT(1)   /* TX FIFO Empty */
#define HT32_UART_SR_RXDR        BIT(2)   /* RX Data Ready */
#define HT32_UART_SR_RXDNE       BIT(3)   /* RX Data Not Empty */

struct uart_ht32_config {
	uint32_t base;
	uint32_t sys_clk_freq;
	uint32_t baud_rate;
};

struct uart_ht32_data {
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	uart_irq_callback_user_data_t callback;
	void *cb_data;
#endif
};

static int uart_ht32_poll_in(const struct device *dev, unsigned char *c)
{
	const struct uart_ht32_config *config = dev->config;
	uint32_t base = config->base;

	/* Check if data is available */
	if ((sys_read32(base + HT32_UART_SR_OFFSET) & HT32_UART_SR_RXDNE) == 0) {
		return -1;
	}

	*c = (unsigned char)sys_read32(base + HT32_UART_DR_OFFSET);

	return 0;
}

static void uart_ht32_poll_out(const struct device *dev, unsigned char c)
{
	const struct uart_ht32_config *config = dev->config;
	uint32_t base = config->base;

	/* Wait until TX FIFO is not full */
	while ((sys_read32(base + HT32_UART_SR_OFFSET) & HT32_UART_SR_TXDE) == 0) {
		/* Wait */
	}

	sys_write32((uint32_t)c, base + HT32_UART_DR_OFFSET);
}

static int uart_ht32_err_check(const struct device *dev)
{
	/* TODO: Implement error checking */
	return 0;
}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN

static int uart_ht32_fifo_fill(const struct device *dev,
			       const uint8_t *tx_data,
			       int size)
{
	const struct uart_ht32_config *config = dev->config;
	uint32_t base = config->base;
	int num_tx = 0;

	while ((size - num_tx > 0) &&
	       (sys_read32(base + HT32_UART_SR_OFFSET) & HT32_UART_SR_TXDE)) {
		sys_write32((uint32_t)tx_data[num_tx], base + HT32_UART_DR_OFFSET);
		num_tx++;
	}

	return num_tx;
}

static int uart_ht32_fifo_read(const struct device *dev,
			       uint8_t *rx_data,
			       const int size)
{
	const struct uart_ht32_config *config = dev->config;
	uint32_t base = config->base;
	int num_rx = 0;

	while ((size - num_rx > 0) &&
	       (sys_read32(base + HT32_UART_SR_OFFSET) & HT32_UART_SR_RXDNE)) {
		rx_data[num_rx] = (uint8_t)sys_read32(base + HT32_UART_DR_OFFSET);
		num_rx++;
	}

	return num_rx;
}

static void uart_ht32_irq_tx_enable(const struct device *dev)
{
	const struct uart_ht32_config *config = dev->config;
	uint32_t base = config->base;

	sys_set_bits(base + HT32_UART_IER_OFFSET, BIT(0)); /* Enable TXC interrupt */
}

static void uart_ht32_irq_tx_disable(const struct device *dev)
{
	const struct uart_ht32_config *config = dev->config;
	uint32_t base = config->base;

	sys_clear_bits(base + HT32_UART_IER_OFFSET, BIT(0)); /* Disable TXC interrupt */
}

static int uart_ht32_irq_tx_ready(const struct device *dev)
{
	const struct uart_ht32_config *config = dev->config;
	uint32_t base = config->base;

	return (sys_read32(base + HT32_UART_SR_OFFSET) & HT32_UART_SR_TXDE) != 0;
}

static void uart_ht32_irq_rx_enable(const struct device *dev)
{
	const struct uart_ht32_config *config = dev->config;
	uint32_t base = config->base;

	sys_set_bits(base + HT32_UART_IER_OFFSET, BIT(1)); /* Enable RXDR interrupt */
}

static void uart_ht32_irq_rx_disable(const struct device *dev)
{
	const struct uart_ht32_config *config = dev->config;
	uint32_t base = config->base;

	sys_clear_bits(base + HT32_UART_IER_OFFSET, BIT(1)); /* Disable RXDR interrupt */
}

static int uart_ht32_irq_tx_complete(const struct device *dev)
{
	const struct uart_ht32_config *config = dev->config;
	uint32_t base = config->base;

	return (sys_read32(base + HT32_UART_SR_OFFSET) & HT32_UART_SR_TXFE) != 0;
}

static int uart_ht32_irq_rx_ready(const struct device *dev)
{
	const struct uart_ht32_config *config = dev->config;
	uint32_t base = config->base;

	return (sys_read32(base + HT32_UART_SR_OFFSET) & HT32_UART_SR_RXDNE) != 0;
}

static void uart_ht32_irq_err_enable(const struct device *dev)
{
	/* TODO: Enable error interrupts */
}

static void uart_ht32_irq_err_disable(const struct device *dev)
{
	/* TODO: Disable error interrupts */
}

static int uart_ht32_irq_is_pending(const struct device *dev)
{
	return uart_ht32_irq_tx_ready(dev) || uart_ht32_irq_rx_ready(dev);
}

static int uart_ht32_irq_update(const struct device *dev)
{
	return 1;
}

static void uart_ht32_irq_callback_set(const struct device *dev,
				       uart_irq_callback_user_data_t cb,
				       void *cb_data)
{
	struct uart_ht32_data *data = dev->data;

	data->callback = cb;
	data->cb_data = cb_data;
}

static void uart_ht32_isr(const struct device *dev)
{
	struct uart_ht32_data *data = dev->data;

	if (data->callback) {
		data->callback(dev, data->cb_data);
	}
}

#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

static int uart_ht32_init(const struct device *dev)
{
	const struct uart_ht32_config *config = dev->config;
	uint32_t base = config->base;
	uint32_t divisor;

	/* Calculate baud rate divisor
	 * The formula is typically: divisor = sys_clk / (baud_rate * 16)
	 * This is a simplified calculation and may need adjustment based on
	 * the actual HT32 UART clock configuration
	 */
	divisor = config->sys_clk_freq / (config->baud_rate * 16);
	sys_write32(divisor, base + HT32_UART_DLR_OFFSET);

	/* Enable TX and RX */
	sys_write32(HT32_UART_CR_URTXEN | HT32_UART_CR_URRXEN,
		    base + HT32_UART_CR_OFFSET);

	return 0;
}

static const struct uart_driver_api uart_ht32_driver_api = {
	.poll_in = uart_ht32_poll_in,
	.poll_out = uart_ht32_poll_out,
	.err_check = uart_ht32_err_check,
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	.fifo_fill = uart_ht32_fifo_fill,
	.fifo_read = uart_ht32_fifo_read,
	.irq_tx_enable = uart_ht32_irq_tx_enable,
	.irq_tx_disable = uart_ht32_irq_tx_disable,
	.irq_tx_ready = uart_ht32_irq_tx_ready,
	.irq_rx_enable = uart_ht32_irq_rx_enable,
	.irq_rx_disable = uart_ht32_irq_rx_disable,
	.irq_tx_complete = uart_ht32_irq_tx_complete,
	.irq_rx_ready = uart_ht32_irq_rx_ready,
	.irq_err_enable = uart_ht32_irq_err_enable,
	.irq_err_disable = uart_ht32_irq_err_disable,
	.irq_is_pending = uart_ht32_irq_is_pending,
	.irq_update = uart_ht32_irq_update,
	.irq_callback_set = uart_ht32_irq_callback_set,
#endif
};

#define UART_HT32_INIT(n)							\
	static const struct uart_ht32_config uart_ht32_config_##n = {		\
		.base = DT_INST_REG_ADDR(n),					\
		.sys_clk_freq = DT_PROP_OR(DT_INST(n, holtek_ht32_uart),	\
					   clock_frequency, 48000000),		\
		.baud_rate = DT_INST_PROP_OR(n, current_speed, 115200),		\
	};									\
										\
	static struct uart_ht32_data uart_ht32_data_##n;			\
										\
	DEVICE_DT_INST_DEFINE(n, &uart_ht32_init,				\
			      NULL,						\
			      &uart_ht32_data_##n,				\
			      &uart_ht32_config_##n,				\
			      PRE_KERNEL_1,					\
			      CONFIG_SERIAL_INIT_PRIORITY,			\
			      &uart_ht32_driver_api);

DT_INST_FOREACH_STATUS_OKAY(UART_HT32_INIT)
