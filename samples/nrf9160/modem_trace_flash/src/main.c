/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>
#include <modem/nrf_modem_lib_trace.h>
#include <nrf_modem.h>
#include <nrf_modem_at.h>
#include <dk_buttons_and_leds.h>

#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(modem_trace_flash_sample, CONFIG_MODEM_TRACE_FLASH_SAMPLE_LOG_LEVEL);

LTE_LC_ON_CFUN(cfun_hook, on_cfun, NULL);

/* Callback for when modem functional mode is changed */
static void on_cfun(enum lte_lc_func_mode mode, void *context)
{
	LOG_INF("LTE mode changed to %d\n", mode);
}



static void print_traces(void)
{
	if (IS_ENABLED(MODEM_TRACE_FLASH_SAMPLE_UART)) {
		print_traces();
	} else {
		LOG_ERR("No chosen device found for nordic,modem-trace-uart");
	}
}

static void button_handler(uint32_t button_states, uint32_t has_changed)
{
	if (has_changed & button_states & DK_BTN1_MSK) {
		printk("Button 1 pressed - dumping traces to uart1\n");
		print_traces();
	}

	if (has_changed & button_states & DK_BTN2_MSK) {
		printk("Button 2 pressed - restarting application\n");
		sys_reboot(SYS_REBOOT_WARM);
	}
}

void nrf_modem_lib_trace_callback(enum nrf_modem_lib_trace_event evt)
{
	switch (evt) {
	case NRF_MODEM_LIB_TRACE_EVT_FULL:
		printk("Modem trace backend is full\n");
		break;
	default:
		printk("Received trace callback %d\n", evt);
		break;
	}
}

int main(void)
{
	int err;

	LOG_INF("Modem trace backend sample started\n");

	err = dk_buttons_init(button_handler);
	if (err) {
		LOG_ERR("Failed to initialize DK buttons library, error: %d\n", err);
	}

	nrf_modem_lib_init();

	err = nrf_modem_lib_trace_level_set(NRF_MODEM_LIB_TRACE_LEVEL_FULL);
	if (err) {
		LOG_ERR("Failed to enable modem traces");
	}

	LOG_INF("Connecting to network\n");

	err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_NORMAL);
	if (err) {
		LOG_ERR("Failed to change LTE mode, err %d\n", err);
		return 0;
	}

	/* Leave the modem on for 10 seconds */
	k_sleep(K_SECONDS(10));

	err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_POWER_OFF);
	if (err) {
		LOG_ERR("Failed to change LTE mode, err %d\n", err);
		return 0;
	}

	/* Give the modem some time to turn off and receive traces */
	k_sleep(K_SECONDS(5));

	err = nrf_modem_lib_trace_level_set(NRF_MODEM_LIB_TRACE_LEVEL_OFF);
	if (err) {
		LOG_ERR("Failed to turn off modem traces\n");
	} else {
		LOG_INF("Turned off modem traces\n");
	}

	/* Changing the trace level to off will produce some traces, so sleep long enough to
	 * receive those as well.
	 */
	k_sleep(K_SECONDS(1));

	LOG_INF("Press button 1 to print traces to UART");
	LOG_INF("Press button 2 to restart application (warm boot)");

	return 0;
}
