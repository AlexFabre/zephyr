# SPDX-License-Identifier: Apache-2.0

board_runner_args(jlink "--device=HT32F52341" "--speed=4000")
board_runner_args(pyocd "--target=ht32f52341")

include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)
