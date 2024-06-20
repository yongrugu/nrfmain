# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

if(CONFIG_BOARD_THINGY91X_NRF9151 OR CONFIG_BOARD_THINGY91X_NRF9151_NS)
  # for the nRF91, the onboard CMSIS-DAP probe can be used
  board_runner_args(pyocd "--target=nrf91" "--frequency=4000000")
  include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)

  board_runner_args(jlink "--device=nRF9160_xxAA" "--speed=4000")
  board_runner_args(nrfutil "--nrf-family=NRF91")
elseif(BOARD_THINGY91X_NRF5340_CPUAPP OR BOARD_THINGY91X_NRF5340_CPUAPP_NS)
  board_runner_args(jlink "--device=nrf5340_xxaa_app" "--speed=4000")
  board_runner_args(nrfutil "--nrf-family=NRF53")
elseif(BOARD_THINGY91X_NRF5340_CPUNET)
  board_runner_args(jlink "--device=nrf5340_xxaa_net" "--speed=4000")
  board_runner_args(nrfutil "--nrf-family=NRF53")
endif()

include(${ZEPHYR_BASE}/boards/common/nrfutil.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
