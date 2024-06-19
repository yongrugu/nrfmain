#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

if(SB_CONFIG_ML_APP_INCLUDE_REMOTE_IMAGE AND DEFINED SB_CONFIG_ML_APP_REMOTE_BOARD)
  # Add remote project
  ExternalZephyrProject_Add(
    APPLICATION remote
    SOURCE_DIR ${APP_DIR}/remote
    BOARD ${SB_CONFIG_ML_APP_REMOTE_BOARD}
  )

if(SB_CONFIG_PARTITION_MANAGER)
  set_property(GLOBAL APPEND PROPERTY PM_DOMAINS CPUNET)
  set_property(GLOBAL APPEND PROPERTY PM_CPUNET_IMAGES remote)
  set_property(GLOBAL PROPERTY DOMAIN_APP_CPUNET remote)
  set(CPUNET_PM_DOMAIN_DYNAMIC_PARTITION remote CACHE INTERNAL "")
endif()

# Add a dependency so that the remote image will be built first.
sysbuild_add_dependencies(CONFIGURE ${DEFAULT_IMAGE} remote)
# Add dependency so that the remote image is flashed first.
sysbuild_add_dependencies(FLASH ${DEFAULT_IMAGE} remote)
sysbuild_add_dependencies(FLASH remote ipc_radio)

endif()
