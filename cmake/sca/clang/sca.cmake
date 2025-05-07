# SPDX-License-Identifier: Apache-2.0
#
# Copyright (c) 2025 Alex Fabre

# Enable Clang static analyzer
list(APPEND TOOLCHAIN_C_FLAGS -Wall -Wextra)
list(APPEND TOOLCHAIN_C_FLAGS -Wno-unused-command-line-argument)
list(APPEND TOOLCHAIN_C_FLAGS -Wno-unused-parameter)

# Add GCC analyzer user options
zephyr_get(CLANG_SCA_OPTS)

if(DEFINED CLANG_SCA_OPTS)
  foreach(analyzer_option IN LISTS CLANG_SCA_OPTS)
    list(APPEND TOOLCHAIN_C_FLAGS ${analyzer_option})
  endforeach()
endif()
