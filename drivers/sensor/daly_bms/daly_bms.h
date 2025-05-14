/*
 * Copyright (c) 2025 Alex Fabre
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_BMS_DALY_H_
#define ZEPHYR_DRIVERS_BMS_DALY_H_

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <stdint.h>
#include <string.h>

#ifdef CONFIG_DALY_BUS_UART
#include <zephyr/drivers/uart.h>
#include "daly_bms_uart.h"
#endif

#ifdef CONFIG_DALY_BUS_CAN
/* This 'can_bus' property is used to distinguish from the uart bus
 * and the can bus, as long as the 'can-controller' does not implement the
 *'bus: can' property.
 */
#include <zephyr/drivers/can.h>
#include "daly_bms_can.h"
#endif

/**
 * @brief All commands accessible by user
 */
enum daly_bms_command {
	SOC_OF_TOTAL_VOLTAGE_CURRENT = 0x90u,
	MAX_AND_MIN_VOLTAGE = 0x91u,
	MAX_AND_MIN_TEMP = 0x92u,
	CHARGE_AND_DISCHARGE_MOS_STATUS = 0x93u,
	STATUS_INFORMATION_1 = 0x94u,
	CELL_VOLTAGE_1_TO_48 = 0x95u,
	CELL_TEMP_1_TO_16 = 0x96u,
	CELL_BALANCE_STATE_1_TO_48 = 0x97u,
	BATTERY_FAILURE_STATUS = 0x98u,
};

enum daly_bms_state {
	STATIONARY = 0,
	CHARGE = 1,
	DISCHARGE = 2,
};

enum daly_bms_sensor_channel {
	/* External temperature inputs */
	SENSOR_CHAN_BMS_SOC = SENSOR_CHAN_PRIV_START,
	SENSOR_CHAN_BMS_STATE,
};

struct daly_bms_bus_cfg {
#ifdef CONFIG_DALY_BUS_UART
	const struct device *uart_dev;
	uart_irq_callback_user_data_t uart_irq_cb;
#endif
#ifdef CONFIG_DALY_BUS_CAN
	const struct device *can_dev;
	can_rx_callback_t can_irq_cb;
#endif
};

struct daly_bms_data {
	/* Sensor data */
	uint8_t state;
	uint16_t state_of_charge;

	struct k_mutex mutex;

#ifdef CONFIG_DALY_BUS_UART
	/* Number of bytes of the current frame received into uart_rx_buffer. */
	unsigned int nb_frame_bytes_received;
	/* UART Rx buffer: Each byte received is added into this buffer. */
	uint8_t uart_rx_buffer[UART_DATA_FRAME_LENGTH];
	/* UART data frame: Once the whole uart_rx_buffer is received the content is copied into
	 * this data frame representation.
	 */
	union {
		uint8_t uart_data_frame_bytes[UART_DATA_FRAME_LENGTH];
		struct daly_uart_data_frame uart_data_frame;
	};
#endif
#ifdef CONFIG_DALY_BUS_CAN
	struct daly_can_data_frame latest_can_data_received;
#endif
};

struct daly_bms_cfg {
	/* Bus config */
	int (*bus_init)(const struct device *dev);
	const struct daly_bms_bus_cfg bus_cfg;
	/* Request and read callbacks */
	int (*query_data)(const struct device *dev, enum daly_bms_command data_id);
	int (*read_data)(const struct device *dev, enum daly_bms_command data_id);
	/* Device ID */
	const unsigned int id;
	/* Host ID (By default, host ID is set to 'PC Address' which is '40' ) */
	const unsigned int host_id;
};

#endif /* ZEPHYR_DRIVERS_BMS_DALY_H_ */
