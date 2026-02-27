# SPDX-License-Identifier: Apache-2.0

zephyr_library()

# Transport-specific driver sources
add_subdirectory_ifdef(CONFIG_STM32_BOOTLOADER_UART usart)

# Shared sources 
zephyr_library_sources_ifdef(CONFIG_STM32_BOOTLOADER stm32_bootloader_common.c)
