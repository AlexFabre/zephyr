/*
 * Copyright (c) 2025 Alex Fabre
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Daly protocol:
 * https://www.dalybms.com/news/daly-three-communication-protocols-explanation/
 *
 */

#define DT_DRV_COMPAT daly_bms

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/sensor.h>

#include "daly_bms.h"

LOG_MODULE_REGISTER(DalyBMS, CONFIG_SENSOR_LOG_LEVEL);

#ifdef CONFIG_DALY_BMS_BUS_UART

/**
 * @brief Flush the UART receive buffer.
 *
 * Reads and discards all data currently in the UART receive buffer.
 * Used to clear any unwanted or residual data before starting a new
 * communication session with the sensor.
 *
 * @param uart_dev Pointer to the UART device structure.
 */
static void daly_bms_uart_clear(const struct device *uart_dev)
{
	uint8_t c;

	while (uart_fifo_read(uart_dev, &c, 1) > 0) {
		;
	}
}

/**
 * @brief Compute the checksum of a TOFSense data frame.
 *
 * This function calculates the checksum for a given data frame by summing up
 * all bytes except the last one. The checksum is used to verify the integrity
 * of the data received from the sensor.
 *
 * @param data Pointer to the data array containing the frame.
 * @param data_size Size of the data array, including the checksum byte.
 *
 * @return The computed checksum as an 8-bit unsigned integer.
 */
static uint8_t daly_bms_uart_checksum(const uint8_t *data, const size_t data_size)
{
	uint8_t sum = 0;

	for (size_t i = 0; i < (data_size - 1); i++) {
		sum += data[i];
	}

	return sum;
}

int daly_bms_uart_init(const struct device *dev)
{
	const struct daly_bms_cfg *cfg = dev->config;
	const struct device *const uart_dev = cfg->bus_cfg.uart_dev;

	int ret;

	if (!device_is_ready(uart_dev)) {
		/* Not ready, do not use */
		LOG_ERR("UART: Device %s not ready.\n", uart_dev->name);
		return -ENODEV;
	}

	uart_irq_rx_disable(uart_dev);
	uart_irq_tx_disable(uart_dev);

	daly_bms_uart_clear(uart_dev);

	LOG_INF("Initializing sensor %u in UART mode", cfg->id);

	ret = uart_irq_callback_user_data_set(uart_dev, cfg->bus_cfg.uart_irq_cb, (void *)dev);

	if (ret < 0) {
		if (ret == -ENOTSUP) {
			LOG_ERR("Interrupt-driven UART API support not enabled");
		} else if (ret == -ENOSYS) {
			LOG_ERR("UART device does not support interrupt-driven API");
		} else {
			LOG_ERR("Error setting UART callback: %d", ret);
		}
		return ret;
	}

	uart_irq_rx_enable(uart_dev);

	return 0;
}

static int daly_bms_uart_query_data(const struct device *dev, enum daly_bms_command data_id)
{
	const struct daly_bms_cfg *cfg = dev->config;

	union {
		uint8_t byte_array[UART_QUERY_FRAME_LENGTH];
		struct daly_bms_uart_query_data_frame field;
	} query = {
		.field.header = UART_FRAME_HEADER_BYTE,
		.field.function_mark = UART_QUERY_OUTPUT_PROTOCOL_BYTE,
		.field.id = cfg->id,
		.field.sum_check =
			daly_bms_uart_checksum(query.byte_array, UART_QUERY_FRAME_LENGTH),
	};

	for (int i = 0; i < UART_QUERY_FRAME_LENGTH; i++) {
		uart_poll_out(cfg->bus_cfg.uart_dev, query.byte_array[i]);
	}

	return 0;
}

static int daly_bms_uart_read_data(const struct device *dev, enum daly_bms_command data_id)
{
	const struct daly_bms_cfg *cfg = dev->config;
	struct daly_bms_data *data = dev->data;

	int64_t_t start = k_uptime_get();
	int64_t duration = 0;

	while (duration <= cfg->communication_timeout) {

		if (data->uart_data_frame_bytes[0] == UART_FRAME_HEADER_BYTE) {
			break;
		}

		duration = (k_uptime_get() - start);
	}

	if (duration > cfg->communication_timeout) {
		/* In active mode, the sensor autonomously sends it's values.
		 * This case handles when no (new) data frame was received in given time.
		 */
		LOG_ERR("No data received from sensor %u after %u ms", cfg->id,
			cfg->communication_timeout);
		return -ENODATA;
	}

	uint8_t checksum;

	checksum = daly_bms_uart_checksum(data->uart_data_frame_bytes, UART_DATA_FRAME_LENGTH);
	if (checksum != data->uart_data_frame_bytes[UART_DATA_CHECKSUM_INDEX]) {
		LOG_ERR("Sensor %d, checksum mismatch: calculated 0x%X != data checksum 0x%X",
			cfg->id, checksum, data->uart_data_frame_bytes[UART_DATA_CHECKSUM_INDEX]);
		LOG_HEXDUMP_ERR(data->uart_data_frame_bytes, UART_DATA_FRAME_LENGTH, "Rx data");

		return -EBADMSG;
	}

	k_mutex_lock(&data->mutex, K_FOREVER);

	data->system_time = data->uart_data_frame.data.system_time;
	data->distance_mm = data->uart_data_frame.data.distance.value_mm;
	data->distance_status = data->uart_data_frame.data.distance.status;
	data->signal_strength = data->uart_data_frame.data.signal_strength;

	k_mutex_unlock(&data->mutex);

	/* Once the data frame has been fetched, it gets cleared. It let us detect if no new data
	 * frame is received upon next fetch request.
	 */
	BUFFER_CLEAR(data->uart_data_frame_bytes);

	return 0;
}

static void daly_bms_uart_isr(const struct device *uart_dev, void *user_data)
{
	const struct device *dev = user_data;
	struct daly_bms_data *data = dev->data;

	if (uart_dev == NULL) {
		LOG_ERR("UART device is NULL");
		return;
	}

	if (!uart_irq_update(uart_dev)) {
		LOG_ERR("Unable to start processing interrupts");
		return;
	}

	if (uart_irq_rx_ready(uart_dev)) {
		data->nb_frame_bytes_received += uart_fifo_read(
			uart_dev, &data->uart_rx_buffer[data->nb_frame_bytes_received],
			UART_DATA_FRAME_LENGTH - data->nb_frame_bytes_received);

		/* The first byte should be UART_FRAME_HEADER_BYTE for a valid read.
		 * If we do not read UART_FRAME_HEADER_BYTE on what we think is the
		 * first byte, then reset the number of bytes read until we do.
		 */
		if ((data->uart_rx_buffer[0] != UART_FRAME_HEADER_BYTE) &
		    (data->nb_frame_bytes_received == 1)) {
			LOG_DBG("First byte is not a valid header (0x%2X). Resetting # of bytes "
				"read.",
				UART_FRAME_HEADER_BYTE);
			data->nb_frame_bytes_received = 0;
			BUFFER_CLEAR(data->uart_rx_buffer);
		}

		if (data->nb_frame_bytes_received == UART_DATA_FRAME_LENGTH) {

			BUFFER_COPY(data->uart_data_frame_bytes, data->uart_rx_buffer);

			LOG_HEXDUMP_DBG(data->uart_rx_buffer, UART_DATA_FRAME_LENGTH, "Rx data");

			daly_bms_uart_clear(uart_dev);
			data->nb_frame_bytes_received = 0;
			BUFFER_CLEAR(data->uart_rx_buffer);
		}
	}
}

#endif /* CONFIG_DALY_BMS_BUS_UART */

#ifdef CONFIG_DALY_BMS_BUS_CAN

static int daly_bms_can_query_data(const struct device *dev, enum daly_bms_command data_id)
{
	const struct daly_bms_cfg *cfg = dev->config;

	LOG_DBG("CAN query data 0x%x to sensor %d", data_id, cfg->id);

	struct can_frame frame = {
		.id = DALY_BMS_CAN_ID(DALY_BMS_CAN_ID_PRIORITY_DEFAULT_VALUE, data_id, cfg->id,
				      cfg->host_id),
		.dlc = DALY_BMS_CAN_DATA_LENGTH,
		.data = {},
	};

	return can_send(cfg->bus_cfg.can_dev, &frame, K_MSEC(30), NULL, NULL);
}

static int daly_bms_can_read_data(const struct device *dev, enum daly_bms_command data_id)
{
	struct daly_bms_data *data = dev->data;

	k_mutex_lock(&data->mutex, K_FOREVER);

	switch (data_id) {
	case SOC_OF_TOTAL_VOLTAGE_CURRENT:
		struct daly_can_command_0x90 *pdata = &data->latest_can_data_received;
		data->state_of_charge = pdata.SOC;
		break;
	case CHARGE_AND_DISCHARGE_MOS_STATUS:
		struct daly_can_command_0x93 *pdata = &data->latest_can_data_received;
		data->state = pdata.state;
		break;
		/* To be completed... */
	}

	k_mutex_unlock(&data->mutex);
	return 0;
}

static void daly_bms_can_isr(const struct device *can_dev, struct can_frame *frame, void *user_data)
{
	const struct device *dev = user_data;
	struct daly_bms_data *data = dev->data;
	const struct daly_bms_cfg *cfg = dev->config;

	if (can_dev == NULL) {
		LOG_ERR("CAN device is NULL");
		return;
	}

	memcpy(&data->latest_can_data_received, &frame->data, DALY_BMS_CAN_DATA_LENGTH);

	LOG_DBG("CAN id: 0x%x data: 0x%x %x %x %x %x %x %x %x", cfg->id, frame->data[0],
		frame->data[1], frame->data[2], frame->data[3], frame->data[4], frame->data[5],
		frame->data[6], frame->data[7]);
}

int daly_bms_can_bus_init(const struct device *dev)
{
	const struct daly_bms_cfg *cfg = dev->config;
	const struct device *const can_dev = cfg->bus_cfg.can_dev;
	int ret = 0;

	LOG_INF("Initializing daly bms %u in CAN mode", cfg->id);

	const struct can_filter daly_bms_filter = {
		.id = DALY_BMS_CAN_ID(DALY_BMS_CAN_ID_PRIORITY_DEFAULT_VALUE, 0xFF, cfg->host_id,
				      cfg->id),
		.mask = DALY_BMS_CAN_ID(DALY_BMS_CAN_ID_PRIORITY_DEFAULT_VALUE, 0x00, cfg->host_id,
					cfg->id),
		,
		.flags = 0U,
	};

	if (!device_is_ready(can_dev)) {
		LOG_ERR("CAN: Device %s not ready.\n", can_dev->name);
		return -ENODEV;
	}

	ret = can_start(can_dev);
	if ((ret != 0) && (ret != -EALREADY)) {
		LOG_ERR("Error starting CAN controller [%d]", ret);
		return ret;
	}

	ret = can_add_rx_filter(can_dev, cfg->bus_cfg.can_irq_cb, (void *)dev, &daly_bms_filter);
	if (ret == -ENOSPC) {
		LOG_ERR("Error, no filter available!\n");
		return 0;
	}

	return 0;
}
#endif /* CONFIG_DALY_BMS_BUS_CAN */

static inline int daly_bms_poll_data(const struct device *dev, enum daly_bms_command data_id)
{
	const struct daly_bms_cfg *cfg = dev->config;
	int ret = 0;

	ret = cfg->query_data(dev, data_id);

	if (ret) {
		LOG_ERR("Sensor %d, query send failed", cfg->id);
		return ret;
	}

	return cfg->read_data(dev, data_id);
}

static int daly_bms_channel_get(const struct device *dev, enum sensor_channel chan,
				struct sensor_value *val)
{
	struct daly_bms_data *data = dev->data;

	k_mutex_lock(&data->mutex, K_FOREVER);

	switch (chan) {
	case SENSOR_CHAN_BMS_SOC:
		val->val1 = data->state_of_charge;
		val->val2 = 0;
		break;
	case SENSOR_CHAN_BMS_STATE:
		val->val1 = data->state;
		val->val2 = 0;
		break;
	default:
		return -ENOTSUP;
		break;
	}

	k_mutex_unlock(&data->mutex);

	return 0;
}

static int daly_bms_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	if (chan == SENSOR_CHAN_BMS_SOC) {
		return daly_bms_poll_data(dev, SOC_OF_TOTAL_VOLTAGE_CURRENT);
	}

	return -ENOTSUP;
}

static DEVICE_API(sensor, daly_bms_api_funcs) = {
	.sample_fetch = daly_bms_sample_fetch,
	.channel_get = daly_bms_channel_get,
};

static int daly_bms_init(const struct device *dev)
{
	struct daly_bms_data *data = dev->data;
	const struct daly_bms_cfg *cfg = dev->config;

	k_mutex_init(&data->mutex);

	return cfg->bus_init(dev);
}

/*
 * Device configuration macro, shared by DALY_BMS_CONFIG_UART() and
 * DALY_BMS_CONFIG_CAN().
 */

#define DALY_BMS_CONFIG_COMMON(inst)                                                               \
	.id = DT_INST_PROP(inst, id), .host_id = DT_INST_PROP(inst, host_id)

/*
 * Device creation macro, shared by DALY_BMS_DEFINE_UART() and
 * DALY_BMS_DEFINE_CAN().
 */

#define DALY_BMS_DEVICE_INIT(inst)                                                                 \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, daly_bms_init, NULL, &daly_bms_data_##inst,             \
				     &daly_bms_cfg_##inst, POST_KERNEL,                            \
				     CONFIG_SENSOR_INIT_PRIORITY, &daly_bms_api_funcs);

/*
 * Instantiation macros used when a device is on an UART bus.
 */

#define DALY_BMS_CONFIG_UART(inst)                                                                 \
	{                                                                                          \
		.bus_cfg.uart_dev = DEVICE_DT_GET(DT_INST_BUS(inst)),                              \
		.bus_cfg.uart_irq_cb = daly_bms_uart_isr,                                          \
		.bus_init = daly_bms_uart_init,                                                    \
		.query_data = daly_bms_uart_query_data,                                            \
		.read_data = daly_bms_uart_read_data,                                              \
		DALY_BMS_CONFIG_COMMON(inst),                                                      \
	}

#define DALY_BMS_DEFINE_UART(inst)                                                                 \
	static struct daly_bms_data daly_bms_data_##inst;                                          \
	static const struct daly_bms_cfg daly_bms_cfg_##inst = DALY_BMS_CONFIG_UART(inst);         \
	DALY_BMS_DEVICE_INIT(inst)

/*
 * Instantiation macros used when a device is on an CAN bus.
 */

#define DALY_BMS_CONFIG_CAN(inst)                                                                  \
	{                                                                                          \
		.bus_cfg.can_dev = DEVICE_DT_GET(DT_INST_PARENT(inst)),                            \
		.bus_cfg.can_irq_cb = daly_bms_can_isr,                                            \
		.bus_init = daly_bms_can_bus_init,                                                 \
		.query_data = daly_bms_can_query_data,                                             \
		.read_data = daly_bms_can_read_data,                                               \
		DALY_BMS_CONFIG_COMMON(inst),                                                      \
	}

#define DALY_BMS_DEFINE_CAN(inst)                                                                  \
	static struct daly_bms_data daly_bms_data_##inst;                                          \
	static const struct daly_bms_cfg daly_bms_cfg_##inst = DALY_BMS_CONFIG_CAN(inst);          \
	DALY_BMS_DEVICE_INIT(inst)

#define DALY_BMS_DEFINE(inst)                                                                      \
	COND_CODE_1(DT_INST_ON_BUS(inst, uart), (DALY_BMS_DEFINE_UART(inst)),                      \
		    (DALY_BMS_DEFINE_CAN(inst)))

DT_INST_FOREACH_STATUS_OKAY(DALY_BMS_DEFINE)
