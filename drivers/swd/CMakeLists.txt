# Copyright 2025 Alex Fabre
# SPDX-License-Identifier: Apache-2.0

if (CONFIG_SWD)

zephyr_library()

zephyr_library_sources(swd.c)

endif()