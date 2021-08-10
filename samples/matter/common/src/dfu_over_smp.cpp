/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "dfu_over_smp.h"

#if !defined(CONFIG_MCUMGR_SMP_BT) || !defined(CONFIG_MCUMGR_CMD_IMG_MGMT) || !defined(CONFIG_MCUMGR_CMD_OS_MGMT)
#error "DFUOverSMP requires MCUMGR module configs enabled"
#endif

#include <dfu/mcuboot.h>
#include <img_mgmt/img_mgmt.h>
#include <mgmt/mcumgr/smp_bt.h>
#include <os_mgmt/os_mgmt.h>

#include <platform/CHIPDeviceLayer.h>

#include <support/logging/CHIPLogging.h>

DFUOverSMP DFUOverSMP::sDFUOverSMP;

void DFUOverSMP::Init(DFUOverSMPRestartAdvertisingHandler startAdvertisingCb)
{
	os_mgmt_register_group();
	img_mgmt_register_group();
	img_mgmt_set_upload_cb(UploadConfirmHandler, NULL);

	memset(&mBleConnCallbacks, 0, sizeof(mBleConnCallbacks));
	mBleConnCallbacks.disconnected = OnBleDisconnect;

	bt_conn_cb_register(&mBleConnCallbacks);

	restartAdvertisingCallback = startAdvertisingCb;
}

void DFUOverSMP::ConfirmNewImage()
{
	/* Check if the image is run in the REVERT mode and eventually
	 * confirm it to prevent reverting on the next boot. */
	if (mcuboot_swap_type() == BOOT_SWAP_TYPE_REVERT) {
		if (boot_write_img_confirmed()) {
			ChipLogError(DeviceLayer,
				     "Confirming firmware image failed, it will be reverted on the next boot.");
		} else {
			ChipLogProgress(DeviceLayer, "New firmware image confirmed.");
		}
	}
}

int DFUOverSMP::UploadConfirmHandler(uint32_t offset, uint32_t size, void *arg)
{
	/* For now just print update progress and confirm data chunk without any additional checks. */
	ChipLogProgress(DeviceLayer, "Software update progress %d B / %d B", offset, size);

	return 0;
}

void DFUOverSMP::StartServer()
{
	if (!mIsEnabled) {
		mIsEnabled = true;
		smp_bt_register();

		ChipLogProgress(DeviceLayer, "Enabled software update");

		/* Start SMP advertising only in case CHIPoBLE advertising is not working. */
		if (!chip::DeviceLayer::ConnectivityMgr().IsBLEAdvertisingEnabled())
			StartBLEAdvertising();
	} else {
		ChipLogProgress(DeviceLayer, "Software update is already enabled");
	}
}

void DFUOverSMP::StartBLEAdvertising()
{
	if (!mIsEnabled)
		return;

	const char *deviceName = bt_get_name();
	const uint8_t advFlags = BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR;

	bt_data ad[] = { BT_DATA(BT_DATA_FLAGS, &advFlags, sizeof(advFlags)),
			 BT_DATA(BT_DATA_NAME_COMPLETE, deviceName, static_cast<uint8_t>(strlen(deviceName))) };

	int rc;
	bt_le_adv_param advParams = BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_ONE_TIME,
							 BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, nullptr);

	rc = bt_le_adv_stop();
	if (rc) {
		ChipLogError(DeviceLayer, "SMP advertising stop failed (rc %d)", rc);
	}

	rc = bt_le_adv_start(&advParams, ad, ARRAY_SIZE(ad), NULL, 0);
	if (rc) {
		ChipLogError(DeviceLayer, "SMP advertising start failed (rc %d)", rc);
	} else {
		ChipLogProgress(DeviceLayer, "Started SMP service BLE advertising");
	}
}

void DFUOverSMP::OnBleDisconnect(struct bt_conn *conId, uint8_t reason)
{
	chip::DeviceLayer::PlatformMgr().LockChipStack();

	/* After BLE disconnect SMP advertising needs to be restarted. Before making it ensure that BLE disconnect was
	 * not triggered by closing CHIPoBLE service connection (in that case CHIPoBLE advertising needs to be
	 * restarted). */
	if (!chip::DeviceLayer::ConnectivityMgr().IsBLEAdvertisingEnabled() &&
	    chip::DeviceLayer::ConnectivityMgr().NumBLEConnections() == 0) {
		sDFUOverSMP.restartAdvertisingCallback();
	}

	chip::DeviceLayer::PlatformMgr().UnlockChipStack();
}
