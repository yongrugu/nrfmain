/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>

void main(void)
{
	/* The only activity of this application is interaction with the APP
	 * core using serialized communication through the RP SER library.
	 * The necessary handlers are registered through RP SER interface
	 * and start at system boot.
	 */
	printk("Entropy sample started[NET Core].\n");
}
