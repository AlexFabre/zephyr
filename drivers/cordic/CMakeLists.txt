# SPDX-License-Identifier: Apache-2.0

zephyr_syscall_header(${ZEPHYR_BASE}/include/zephyr/drivers/cordic.h)

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_CORDIC_STM32 cordic_stm32.c)
