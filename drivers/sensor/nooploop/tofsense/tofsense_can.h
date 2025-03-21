/*
 * Copyright (c) 2025 Alex Fabre
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_NOOPLOOP_TOFSENSE_CAN_H_
#define ZEPHYR_DRIVERS_SENSOR_NOOPLOOP_TOFSENSE_CAN_H_

/* TOFSense CAN receiving ID is 0x200+module ID */
#define CAN_TOFSENSE_RECEIVE_ID_BASE (0x200)

/* CAN data length in bytes */
#define CAN_DATA_FRAME_LENGTH (8)

/* CAN query data length in bytes */
#define CAN_QUERY_FRAME_LENGTH (8)

/* Data part of the CAN frame received from the module (All fields in little endian) */
struct can_data_field {
	struct tofsense_distance distance;
	uint16_t signal_strength;
	uint16_t reserved;
} __attribute__((packed));

_Static_assert(sizeof(struct can_data_field) == CAN_DATA_FRAME_LENGTH,
	       "struct 'can_data_field' has invalid size !");

/* Raw CAN frame sent to query the module in QUERY mode (All fields in little endian) */
struct tofsense_can_query_data_frame {
	uint16_t reserved_0;
	uint8_t reserved_1;
	uint8_t id;
	uint32_t reserved_2;
} __attribute__((packed));

_Static_assert(sizeof(struct tofsense_can_query_data_frame) == CAN_QUERY_FRAME_LENGTH,
	       "structure 'tofsense_can_query_data_frame' has invalid size !");

#endif /* ZEPHYR_DRIVERS_SENSOR_NOOPLOOP_TOFSENSE_CAN_H_ */
