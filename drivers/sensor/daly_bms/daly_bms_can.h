/*
 * Copyright (c) 2025 Alex Fabre
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_BMS_DALY_CAN_H_
#define ZEPHYR_DRIVERS_BMS_DALY_CAN_H_

/* DALY BMS CAN ID length in bytes
 * @note Daly BMS uses the CAN ID in a custom
 * manner where some information is embedded in the ID.
 * The CAN frame IF contains four information: message
 * priority, data id, destination address, origin address.
 *
 * bits position | 28 - 24  | 23 - 16  | 15 - 8      | 7 - 0  |
 * descritpion   | priority | data id  | destination | origin |
 */
#define DALY_BMS_CAN_ID_LENGTH (4)

/* DALY has a custom 'priority' field in the CAN ID.
 * By default this priority is set to 0x18.
 * Because the priority is the MSB part of the ID frame,
 * it's value cannot exeed 0x1F. */
#define DALY_BMS_CAN_ID_PRIORITY_DEFAULT_VALUE (0x18)
#define DALY_BMS_CAN_ID_PRIORITY_MASK          (0x1F)

#define DALY_BMS_CAN_ID(priority, data_id, destination, origin)                                    \
	(((priority) & DALY_BMS_CAN_ID_PRIORITY_MASK) << 24) | (((data_id) & 0xFF) << 16) |        \
		(((destination) & 0xFF) << 8) | ((origin) & 0xFF)

/* CAN data length in bytes */
#define DALY_BMS_CAN_DATA_LENGTH (8)
struct daly_can_id {
	uint32_t origin_address: 8;
	uint32_t destination_address: 8;
	uint32_t data_id: 8;
	uint32_t priority: 5; /* Because a CAN ID cannot exceed 29bit in extended mode, this
				 priority field is limited to 0x1F */
} __packed;

BUILD_ASSERT(sizeof(struct daly_can_id) == DALY_BMS_CAN_ID_LENGTH,
	     "struct 'daly_can_id' has invalid size !");

struct daly_can_data_frame {
	uint8_t data[8];
} __packed;

BUILD_ASSERT(sizeof(struct daly_can_data_frame) == DALY_BMS_CAN_DATA_LENGTH,
	     "struct 'daly_can_data_frame' has invalid size !");

/**
 * @brief SOC of total voltage current command frame
 */
struct daly_can_command_0x90 {
	/* Byte0~Byte1:Cumulative total voltage (0.1 V) */
	uint16_t cumulative_total_voltage;
	/* Byte2~Byte3:Gather total voltage (0.1 V) */
	uint16_t gather_total_voltage;
	/* Byte4~Byte5:Current (30000 Offset ,0.1A) */
	uint16_t current;
	/* Byte6~Byte7:SOC (0.1%) */
	uint16_t SOC;
} __packed;

BUILD_ASSERT(sizeof(struct daly_can_command_0x90) <= DALY_BMS_CAN_DATA_LENGTH,
	     "struct 'daly_can_command_0x90' has invalid size !");

/**
 * @brief Maximum & Minimum voltage command frame
 */
struct daly_can_command_0x91 {
	/* Byte0~Byte1:Maximum cell voltage value (mV) */
	uint16_t max_voltage_value;
	/* Byte2:No of cell with Maximum voltage */
	uint8_t max_voltage_cell_number;
	/* Byte3~byte4: Minimum cell voltage value (mV) */
	uint16_t min_voltage_value;
	/* Byte5:No of cell with Minimum voltage */
	uint8_t min_voltage_cell_number;
} __packed;

BUILD_ASSERT(sizeof(struct daly_can_command_0x91) <= DALY_BMS_CAN_DATA_LENGTH,
	     "struct 'daly_can_command_0x91' has invalid size !");

/**
 * @brief Maximum & Minimum temperature command frame
 */
struct daly_can_command_0x92 {
	/* Byte0: Maximum temperature value (40 Offset, °C) */
	uint8_t max_temp_value;
	/* Byte1: Maximum temperature cell No */
	uint8_t max_temp_cell_number;
	/* Byte2: Minimum temperature value (40 Offset, °C) */
	uint8_t min_temp_value;
	/* Byte3: Minimum temperature cell No */
	uint8_t min_temp_cell_number;
} __packed;

BUILD_ASSERT(sizeof(struct daly_can_command_0x92) <= DALY_BMS_CAN_DATA_LENGTH,
	     "struct 'daly_can_command_0x92' has invalid size !");

/**
 * @brief Charge & discharge MOS status
 */
struct daly_can_command_0x93 {
	/* Byte0:State (0 stationary 1 charge 2 discharge) */
	uint8_t state;
	/* Byte1:Charge MOS state */
	uint8_t charge_MOS_state;
	/* Byte2:Discharge MOS status */
	uint8_t discharge_MOS_state;
	/* Byte3:BMS life (0~255 cycles) */
	uint8_t BMS_life;
	/* Byte4~Byte7:Remain capacity (mAH) */
	uint32_t remaining_capacity;
} __packed;

BUILD_ASSERT(sizeof(struct daly_can_command_0x93) <= DALY_BMS_CAN_DATA_LENGTH,
	     "struct 'daly_can_command_0x93' has invalid size !");

#endif /* ZEPHYR_DRIVERS_BMS_DALY_CAN_H_ */
