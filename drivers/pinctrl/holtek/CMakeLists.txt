# Copyright (c) 2025 Alex Fabre <alex.fabre@rtone.fr>
# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_PINCTRL_HT32 pinctrl_ht32.c)
