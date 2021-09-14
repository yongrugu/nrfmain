/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file rest_here_wlan.h
 *
 * @brief Here REST API for WLAN positioning.
 */

#ifndef REST_HERE_WLAN_H
#define REST_HERE_WLAN_H

#include "rest_wlan_services.h"

/**
 * @brief Here wlan positioning request.
 *
 * @param[in]     request Data to be provided in API call.
 * @param[in,out] result  Parsed results of API response.
 *
 * @retval 0 If successful.
 *          Otherwise, a (negative) error code is returned.
 */
int here_rest_wlan_pos_get(const struct rest_wlan_pos_request *request,
			   struct rest_wlan_pos_result *result);

#endif /* REST_HERE_WLAN_H */
