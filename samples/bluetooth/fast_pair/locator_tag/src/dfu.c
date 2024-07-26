/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <app_version.h>

#include <zephyr/kernel.h>

#include <zephyr/bluetooth/uuid.h>
#include <bluetooth/adv_prov.h>

#include <bluetooth/services/fast_pair/fast_pair.h>
#include <bluetooth/services/fast_pair/fmdn.h>

#include <zephyr/mgmt/mcumgr/grp/os_mgmt/os_mgmt.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>

#include "app_dfu.h"
#include "app_factory_reset.h"
#include "app_fp_adv.h"
#include "app_ui.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(fp_fmdn, LOG_LEVEL_INF);

/* DFU mode timeout in minutes. */
#define DFU_MODE_TIMEOUT (1)

/* UUID of the SMP service used for the DFU. */
#define BT_UUID_SMP_SVC_VAL \
	BT_UUID_128_ENCODE(0x8D53DC1D, 0x1DB7, 0x4CD3, 0x868B, 0x8A527460AA84)

/* UUID of the SMP characteristic used for the DFU. */
#define BT_UUID_SMP_CHAR_VAL	\
	BT_UUID_128_ENCODE(0xda2e7828, 0xfbce, 0x4e01, 0xae9e, 0x261174997c48)

#define BT_UUID_SMP_CHAR	BT_UUID_DECLARE_128(BT_UUID_SMP_CHAR_VAL)

static const struct bt_data data = BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_SMP_SVC_VAL);
static bool dfu_mode;

APP_FP_ADV_TRIGGER_REGISTER(fp_adv_trigger_dfu, "dfu");

static void dfu_mode_timeout_refresh_handle(struct k_work *w);
static K_WORK_DEFINE(dfu_mode_timeout_refresh, dfu_mode_timeout_refresh_handle);

static void dfu_mode_timeout_work_handle(struct k_work *w);
static K_WORK_DELAYABLE_DEFINE(dfu_mode_timeout_work, dfu_mode_timeout_work_handle);

BUILD_ASSERT((APP_VERSION_MAJOR == CONFIG_BT_FAST_PAIR_FMDN_DULT_FIRMWARE_VERSION_MAJOR) &&
	     (APP_VERSION_MINOR == CONFIG_BT_FAST_PAIR_FMDN_DULT_FIRMWARE_VERSION_MINOR) &&
	     (APP_PATCHLEVEL == CONFIG_BT_FAST_PAIR_FMDN_DULT_FIRMWARE_VERSION_REVISION),
	     "Firmware version mismatch. Update the DULT FW version in the Kconfig file "
	     "to be aligned with the VERSION file.");

static void dfu_mode_change(bool new_mode)
{
	enum app_fp_adv_mode current_fp_adv_mode = app_fp_adv_mode_get();

	if (dfu_mode == new_mode) {
		return;
	}

	LOG_INF("DFU: mode %sabled", new_mode ? "en" : "dis");

	dfu_mode = new_mode;

	app_fp_adv_request(&fp_adv_trigger_dfu, dfu_mode);

	/* Ensure that the advertising payload is updated if advertising is already enabled. */
	if ((current_fp_adv_mode == APP_FP_ADV_MODE_DISCOVERABLE) ||
	    (current_fp_adv_mode == APP_FP_ADV_MODE_NOT_DISCOVERABLE)) {
		app_fp_adv_refresh();
	}

	app_ui_state_change_indicate(APP_UI_STATE_DFU_MODE, dfu_mode);
}

static void dfu_factory_reset_prepare(void)
{
	dfu_mode_change(false);
}

APP_FACTORY_RESET_CALLBACKS_REGISTER(factory_reset_cbs, dfu_factory_reset_prepare, NULL);

bool app_dfu_bt_gatt_operation_allow(const struct bt_uuid *uuid)
{
	if (bt_uuid_cmp(uuid, BT_UUID_SMP_CHAR) != 0) {
		return true;
	}

	if (!dfu_mode) {
		LOG_WRN("DFU: SMP characteristic access denied, DFU mode is not active");
		return false;
	}

	return true;
}

/* Due to using legacy advertising set size, add SMP UUID to either AD or SD,
 * depending on the space availability related to the advertising mode.
 * Otherwise, the advertising set size would be exceeded and the advertising
 * would not start.
 * The SMP UUID can be added only to one of the data sets.
 */

static int get_ad_data(struct bt_data *ad, const struct bt_le_adv_prov_adv_state *state,
		    struct bt_le_adv_prov_feedback *fb)
{
	ARG_UNUSED(state);
	ARG_UNUSED(fb);

	enum app_fp_adv_mode current_fp_adv_mode = app_fp_adv_mode_get();

	if (!dfu_mode) {
		return -ENOENT;
	}

	if (current_fp_adv_mode != APP_FP_ADV_MODE_DISCOVERABLE) {
		return -ENOENT;
	}

	*ad = data;

	return 0;
}

static int get_sd_data(struct bt_data *sd, const struct bt_le_adv_prov_adv_state *state,
		    struct bt_le_adv_prov_feedback *fb)
{
	ARG_UNUSED(state);
	ARG_UNUSED(fb);

	enum app_fp_adv_mode current_fp_adv_mode = app_fp_adv_mode_get();

	if (!dfu_mode) {
		return -ENOENT;
	}

	if (current_fp_adv_mode != APP_FP_ADV_MODE_NOT_DISCOVERABLE) {
		return -ENOENT;
	}

	*sd = data;

	return 0;
}

/* Used in discoverable adv */
BT_LE_ADV_PROV_AD_PROVIDER_REGISTER(smp_ad, get_ad_data);

/* Used in the not-discoverable adv */
BT_LE_ADV_PROV_SD_PROVIDER_REGISTER(smp_sd, get_sd_data);

static void dfu_mode_timeout_refresh_handle(struct k_work *w)
{
	if (dfu_mode) {
		(void) k_work_reschedule(&dfu_mode_timeout_work, K_MINUTES(DFU_MODE_TIMEOUT));
	}
}

static enum mgmt_cb_return smp_cmd_recv(uint32_t event, enum mgmt_cb_return prev_status,
					int32_t *rc, uint16_t *group, bool *abort_more,
					void *data, size_t data_size)
{
	const struct mgmt_evt_op_cmd_arg *cmd_recv;

	if (event != MGMT_EVT_OP_CMD_RECV) {
		LOG_ERR("Spurious event in recv cb: %" PRIu32, event);
		*rc = MGMT_ERR_EUNKNOWN;
		return MGMT_CB_ERROR_RC;
	}

	LOG_DBG("MCUmgr SMP Command Recv Event");

	if (data_size != sizeof(*cmd_recv)) {
		LOG_ERR("Invalid data size in recv cb: %zu (expected: %zu)",
			data_size, sizeof(*cmd_recv));
		*rc = MGMT_ERR_EUNKNOWN;
		return MGMT_CB_ERROR_RC;
	}

	cmd_recv = data;

	/* Ignore commands not related to DFU over SMP. */
	if (!(cmd_recv->group == MGMT_GROUP_ID_IMAGE) &&
	    !((cmd_recv->group == MGMT_GROUP_ID_OS) && (cmd_recv->id == OS_MGMT_ID_RESET))) {
		return MGMT_CB_OK;
	}

	LOG_DBG("MCUmgr %s event", (cmd_recv->group == MGMT_GROUP_ID_IMAGE) ?
		"Image Management" : "OS Management Reset");

	(void) k_work_submit(&dfu_mode_timeout_refresh);

	return MGMT_CB_OK;
}

static struct mgmt_callback cmd_recv_cb = {
	.callback = smp_cmd_recv,
	.event_id = MGMT_EVT_OP_CMD_RECV,
};

static void dfu_mode_action_handle(void)
{
	if (dfu_mode) {
		LOG_INF("DFU: refreshing the DFU mode timeout");
	} else {
		LOG_INF("DFU: entering the DFU mode for %d minute(s)",
			DFU_MODE_TIMEOUT);
	}

	(void) k_work_reschedule(&dfu_mode_timeout_work, K_MINUTES(DFU_MODE_TIMEOUT));

	dfu_mode_change(true);
}

static void dfu_mode_timeout_work_handle(struct k_work *w)
{
	LOG_INF("DFU: timeout expired");

	dfu_mode_change(false);
}

void app_dfu_fw_version_log(void)
{
	LOG_INF("Firmware version: %s", CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION);
}

int app_dfu_init(void)
{
	mgmt_callback_register(&cmd_recv_cb);

	return 0;
}

static void dfu_mode_request_handle(enum app_ui_request request)
{
	/* It is assumed that the callback executes in the cooperative
	 * thread context as it interacts with the FMDN API.
	 */
	__ASSERT_NO_MSG(!k_is_preempt_thread());
	__ASSERT_NO_MSG(!k_is_in_isr());

	if (request == APP_UI_REQUEST_DFU_MODE_ENTER) {
		dfu_mode_action_handle();
	}
}

APP_UI_REQUEST_LISTENER_REGISTER(dfu_mode_request_handler, dfu_mode_request_handle);
