#
# Copyright (c) 2018 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

# This file creates variables with the magic numbers used for firmware metadata
# as comma-separated lists of numbers.

math(EXPR
  MAGIC_COMPATIBILITY
  "(${CONFIG_SB_INFO_VERSION}) |
   (${CONFIG_SB_HARDWARE_ID} << 8) |
   (${CONFIG_SB_CRYPTO_ALGORITHM} << 16) |
   (${CONFIG_SB_CUSTOM_COMPATIBILITY_ID} << 24)"
  )

set(FIRMWARE_INFO_MAGIC   "${CONFIG_SB_MAGIC_COMMON},${CONFIG_SB_MAGIC_FIRMWARE_INFO},${MAGIC_COMPATIBILITY}")
set(POINTER_MAGIC         "${CONFIG_SB_MAGIC_COMMON},${CONFIG_SB_MAGIC_POINTER},${MAGIC_COMPATIBILITY}")
set(VALIDATION_INFO_MAGIC "${CONFIG_SB_MAGIC_COMMON},${CONFIG_SB_MAGIC_VALIDATION_INFO},${MAGIC_COMPATIBILITY}")
