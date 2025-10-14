/*
 * Copyright (c) 2025 Alex Fabre <alex.fabre@rtone.fr>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_SOC_HOLTEK_HT32_COMMON_PINCTRL_SOC_H_
#define ZEPHYR_SOC_HOLTEK_HT32_COMMON_PINCTRL_SOC_H_

#include <zephyr/devicetree.h>
#include <zephyr/types.h>

/** @cond INTERNAL_HIDDEN */

/** Type for HT32 pin configuration */
typedef uint32_t pinctrl_soc_pin_t;

/**
 * @brief Utility macro to initialize pinctrl pin configuration.
 *
 * @param node_id Node identifier.
 * @param prop Property name.
 * @param idx Property entry index.
 */
#define Z_PINCTRL_STATE_PIN_INIT(node_id, prop, idx) \
	DT_PROP_BY_IDX(node_id, prop, idx),

/**
 * @brief Utility macro to initialize state pins contained in a given property.
 *
 * @param node_id Node identifier.
 * @param prop Property name describing state pins.
 */
#define Z_PINCTRL_STATE_PINS_INIT(node_id, prop) \
	{DT_FOREACH_PROP_ELEM(node_id, prop, Z_PINCTRL_STATE_PIN_INIT)}

/** @endcond */

#endif /* ZEPHYR_SOC_HOLTEK_HT32_COMMON_PINCTRL_SOC_H_ */
