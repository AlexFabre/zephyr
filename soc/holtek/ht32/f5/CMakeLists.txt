# Copyright (c) 2025 Alex Fabre <alex.fabre@rtone.fr>
# SPDX-License-Identifier: Apache-2.0

zephyr_sources(soc.c)
zephyr_sources(vector_table.S)
zephyr_linker_sources(ROM_START SORT_KEY 0x0 rom_start.ld)
