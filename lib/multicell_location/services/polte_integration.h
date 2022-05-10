/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef POLTE_INTEGRATION_H_
#define POLTE_INTEGRATION_H_

#include <zephyr/zephyr.h>
#include <modem/lte_lc.h>
#include <net/multicell_location.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *location_service_get_certificate_polte(void);

int location_service_get_cell_location_polte(
	const struct lte_lc_cells_info *cell_data,
	char *const rcv_buf,
	const size_t rcv_buf_len,
	struct multicell_location *const location);

#ifdef __cplusplus
}
#endif

#endif /* POLTE_INTEGRATION_H_ */
