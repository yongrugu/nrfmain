/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <kernel.h>
#include <console.h>
#include <string.h>
#include <misc/printk.h>
#include <zephyr/types.h>
#include <irq.h>

#include <mpsl_timeslot.h>
#include <mpsl.h>

#define TIMESLOT_REQUEST_DISTANCE_US (1000000)
#define TIMESLOT_LENGTH_US           (200)

#define MPSL_IRQ_LOW_PRIO            (4)
#define STACKSIZE                    CONFIG_IDLE_STACK_SIZE
#define THREAD_PRIORITY              K_LOWEST_APPLICATION_THREAD_PRIO

static volatile int timeslot_counter;
static bool request_in_cb = true;

/* Timeslot requests */
static mpsl_timeslot_request_t timeslot_request_earliest = {
	.request_type = MPSL_TIMESLOT_REQ_TYPE_EARLIEST,
	.params.earliest.hfclk = MPSL_TIMESLOT_HFCLK_CFG_NO_GUARANTEE,
	.params.earliest.priority = MPSL_TIMESLOT_PRIORITY_NORMAL,
	.params.earliest.length_us = TIMESLOT_LENGTH_US,
	.params.earliest.timeout_us = 1000000
};
static mpsl_timeslot_request_t timeslot_request_normal = {
	.request_type = MPSL_TIMESLOT_REQ_TYPE_NORMAL,
	.params.normal.hfclk = MPSL_TIMESLOT_HFCLK_CFG_NO_GUARANTEE,
	.params.normal.priority = MPSL_TIMESLOT_PRIORITY_NORMAL,
	.params.normal.distance_us = TIMESLOT_REQUEST_DISTANCE_US,
	.params.normal.length_us = TIMESLOT_LENGTH_US
};

static mpsl_timeslot_signal_return_param_t signal_callback_return_param;

/* Message queue used for printing the signal type from timeslot callback */
K_MSGQ_DEFINE(callback_msgq, sizeof(u32_t), 10, 4);

static void error(void)
{
	printk("ERROR!\n");
	while (true) {
		/* Spin for ever */
		k_sleep(1000);
	}
}

static mpsl_timeslot_signal_return_param_t *mpsl_timeslot_callback(
	uint32_t signal_type)
{
	mpsl_timeslot_signal_return_param_t *p_ret_val = NULL;

	switch (signal_type) {
	case MPSL_TIMESLOT_SIGNAL_START:
		if (request_in_cb) {
			/* Request new timeslot when callback returns */
			signal_callback_return_param.params.request.p_next =
				&timeslot_request_normal;
			signal_callback_return_param.callback_action =
				MPSL_TIMESLOT_SIGNAL_ACTION_REQUEST;
		} else {
			/* No return action, so timeslot will be ended */
			signal_callback_return_param.params.request.p_next =
				NULL;
			signal_callback_return_param.callback_action =
				MPSL_TIMESLOT_SIGNAL_ACTION_NONE;
		}

		p_ret_val = &signal_callback_return_param;
		break;
	case MPSL_TIMESLOT_SIGNAL_SESSION_IDLE:
		break;
	case MPSL_TIMESLOT_SIGNAL_SESSION_CLOSED:
		break;
	case MPSL_TIMESLOT_SIGNAL_CANCELLED:
	case MPSL_TIMESLOT_SIGNAL_BLOCKED:
	case MPSL_TIMESLOT_SIGNAL_INVALID_RETURN:
	default:
		error();
		break;
	}

	/* Put callback info in the message queue. */
	int err = k_msgq_put(&callback_msgq, &signal_type, K_NO_WAIT);

	if (err) {
		error();
	}

	return p_ret_val;
}

static void mpsl_timeslot_demo(void)
{
	int err;
	char input_char;

	printk("-----------------------------------------------------\n");
	printk("Press a key to open session and request timeslots:\n");
	printk("* 'a' for a session where each timeslot makes a new request\n");
	printk("* 'b' for a session with a single timeslot request\n");
	input_char = console_getchar();
	printk("%c\n", input_char);

	if (input_char == 'a') {
		request_in_cb = true;
	} else if (input_char == 'b') {
		request_in_cb = false;
	} else {
		return;
	}

	err = mpsl_timeslot_session_open(mpsl_timeslot_callback);
	if (err) {
		error();
	}
	err = mpsl_timeslot_request(&timeslot_request_earliest);
	if (err) {
		error();
	}

	printk("Press any key to close the session.\n");
	console_getchar();

	err = mpsl_timeslot_session_close();
	if (err) {
		error();
	}
}

static void console_print_thread(void)
{
	u32_t signal_type = 0;

	while (1) {
		if (k_msgq_get(&callback_msgq, &signal_type, 1) == 0) {
			switch (signal_type) {
			case MPSL_TIMESLOT_SIGNAL_START:
				printk("Callback: Timeslot start\n");
				break;
			case MPSL_TIMESLOT_SIGNAL_SESSION_IDLE:
				printk("Callback: Session idle\n");
				break;
			case MPSL_TIMESLOT_SIGNAL_SESSION_CLOSED:
				printk("Callback: Session closed\n");
				break;
			default:
				printk("Callback: Other signal: %d\n",
					signal_type);
				break;
			}
		}
		k_sleep(10);
	}
}

void main(void)
{
	int err = console_init();

	if (err) {
		error();
	}

	printk("-----------------------------------------------------\n");
	printk("             Nordic MPSL Timeslot sample\n");

	while (1) {
		mpsl_timeslot_demo();
		k_sleep(1000);
	}
}

K_THREAD_DEFINE(console_print_thread_id, STACKSIZE, console_print_thread,
		NULL, NULL, NULL, THREAD_PRIORITY, 0, K_NO_WAIT);
