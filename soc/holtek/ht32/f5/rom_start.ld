/*
 * Copyright (c) 2025 Alex Fabre <alex.fabre@rtone.fr>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Linker script for Holtek HT32F5xxxx
 */

ENTRY(z_arm_reset)

SECTIONS
{
	/*
	 * The first section is intended to place the vector table.
	 * Since the Cortex-M0+ vector table must be aligned at 256-byte boundaries,
	 * .vectors section should be placed first.
	 */
}
