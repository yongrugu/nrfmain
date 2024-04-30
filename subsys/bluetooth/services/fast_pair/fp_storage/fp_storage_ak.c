/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/settings/settings.h>
#include <bluetooth/services/fast_pair/fast_pair.h>

#include "fp_common.h"
#include "fp_storage_ak.h"
#include "fp_storage_ak_bond.h"
#include "fp_storage_ak_priv.h"
#include "fp_storage_manager.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(fp_storage, CONFIG_FP_STORAGE_LOG_LEVEL);

#define BOND_AK_ID_UNKNOWN	(0)

/* Non-volatile bond information that is stored in the Settings subsystem. */
struct fp_bond_info_nv {
	uint8_t account_key_metadata_id;
	bt_addr_le_t addr;
};

struct fp_bond {
	const void *conn_ctx;
	bool bonded;
	struct fp_bond_info_nv nv;
};

static const struct fp_storage_ak_bond_bt_request_cb *bt_request_cb;
static struct fp_bond fp_bonds[FP_BONDS_ARRAY_LEN];

static struct fp_account_key account_key_list[ACCOUNT_KEY_CNT];
static uint8_t account_key_metadata[ACCOUNT_KEY_CNT];
static uint8_t account_key_count;

static uint8_t account_key_order[ACCOUNT_KEY_CNT];

static int settings_set_err;
static bool is_enabled;

static int settings_data_read(void *data, size_t data_len,
			      size_t read_len, settings_read_cb read_cb, void *cb_arg)
{
	int rc;

	if (read_len != data_len) {
		return -EINVAL;
	}

	rc = read_cb(cb_arg, data, data_len);
	if (rc < 0) {
		return rc;
	}

	if (rc != data_len) {
		return -EINVAL;
	}

	return 0;
}

static ssize_t index_from_settings_name_get(const char *name_suffix, size_t max_suffix_len)
{
	size_t name_suffix_len;

	name_suffix_len = strlen(name_suffix);

	if ((name_suffix_len < 1) || (name_suffix_len > max_suffix_len)) {
		return -EINVAL;
	}

	for (size_t i = 0; i < name_suffix_len; i++) {
		if (!isdigit(name_suffix[i])) {
			return -EINVAL;
		}
	}

	return atoi(name_suffix);
}

static int fp_settings_load_ak(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	int err;
	uint8_t id;
	uint8_t index;
	struct account_key_data data;
	ssize_t name_index;

	err = settings_data_read(&data, sizeof(data), len, read_cb, cb_arg);
	if (err) {
		return err;
	}

	id = ACCOUNT_KEY_METADATA_FIELD_GET(data.account_key_metadata, ID);
	if ((id < ACCOUNT_KEY_MIN_ID) || (id > ACCOUNT_KEY_MAX_ID)) {
		return -EINVAL;
	}

	index = account_key_id_to_idx(id);
	name_index = index_from_settings_name_get(&name[sizeof(SETTINGS_AK_NAME_PREFIX) - 1],
						  SETTINGS_AK_NAME_MAX_SUFFIX_LEN);
	if ((name_index < 0) || (index != name_index)) {
		return -EINVAL;
	}

	if (ACCOUNT_KEY_METADATA_FIELD_GET(account_key_metadata[index], ID) != 0) {
		return -EINVAL;
	}

	account_key_list[index] = data.account_key;
	account_key_metadata[index] = data.account_key_metadata;

	return 0;
}

static int fp_settings_load_ak_order(size_t len, settings_read_cb read_cb, void *cb_arg)
{
	return settings_data_read(account_key_order, sizeof(account_key_order), len, read_cb,
				  cb_arg);
}

static int fp_settings_load_bond(const char *name, size_t len, settings_read_cb read_cb,
				 void *cb_arg)
{
	int err;
	ssize_t index;
	struct fp_bond_info_nv bond_nv;

	err = settings_data_read(&bond_nv, sizeof(bond_nv), len, read_cb, cb_arg);
	if (err) {
		return err;
	}

	index = index_from_settings_name_get(&name[sizeof(SETTINGS_BOND_NAME_PREFIX) - 1],
					     SETTINGS_BOND_NAME_MAX_SUFFIX_LEN);
	if ((index < 0) || (index >= ARRAY_SIZE(fp_bonds))) {
		return -EINVAL;
	}

	fp_bonds[index].nv = bond_nv;

	LOG_DBG("Bond loaded successfully");
	return 0;
}

static int fp_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	int err = 0;

	if (!strncmp(name, SETTINGS_AK_NAME_PREFIX, sizeof(SETTINGS_AK_NAME_PREFIX) - 1)) {
		err = fp_settings_load_ak(name, len, read_cb, cb_arg);
	} else if (!strncmp(name, SETTINGS_AK_ORDER_KEY_NAME, sizeof(SETTINGS_AK_ORDER_KEY_NAME))) {
		err = fp_settings_load_ak_order(len, read_cb, cb_arg);
	} else if (IS_ENABLED(CONFIG_BT_FAST_PAIR_STORAGE_AK_BOND) &&
		   !strncmp(name, SETTINGS_BOND_NAME_PREFIX,
		   sizeof(SETTINGS_BOND_NAME_PREFIX) - 1)) {
		err = fp_settings_load_bond(name, len, read_cb, cb_arg);
	} else {
		err = -ENOENT;
	}

	/* The first reported settings set error will be remembered by the module.
	 * The error will then be returned when calling fp_storage_ak_init.
	 */
	if (err && !settings_set_err) {
		settings_set_err = err;
	}

	return 0;
}

static uint8_t bump_ak_id(uint8_t id)
{
	__ASSERT_NO_MSG((id >= ACCOUNT_KEY_MIN_ID) && (id <= ACCOUNT_KEY_MAX_ID));

	return (id < ACCOUNT_KEY_MIN_ID + ACCOUNT_KEY_CNT) ? (id + ACCOUNT_KEY_CNT) :
							     (id - ACCOUNT_KEY_CNT);
}

static uint8_t get_least_recent_key_id(void)
{
	/* The function can be used only if the Account Key List is full. */
	__ASSERT_NO_MSG(account_key_count == ACCOUNT_KEY_CNT);

	return account_key_order[ACCOUNT_KEY_CNT - 1];
}

static uint8_t next_account_key_id(void)
{
	if (account_key_count < ACCOUNT_KEY_CNT) {
		return ACCOUNT_KEY_MIN_ID + account_key_count;
	}

	return bump_ak_id(get_least_recent_key_id());
}

static void ak_order_update_ram(uint8_t used_id)
{
	bool id_found = false;
	size_t found_idx;

	for (size_t i = 0; i < account_key_count; i++) {
		if (account_key_order[i] == used_id) {
			id_found = true;
			found_idx = i;
			break;
		}
	}

	if (id_found) {
		for (size_t i = found_idx; i > 0; i--) {
			account_key_order[i] = account_key_order[i - 1];
		}
	} else {
		for (size_t i = account_key_count - 1; i > 0; i--) {
			account_key_order[i] = account_key_order[i - 1];
		}
	}
	account_key_order[0] = used_id;
}

static int validate_ak_order(void)
{
	int err;
	size_t ak_order_update_count = 0;

	for (size_t i = account_key_count; i < ACCOUNT_KEY_CNT; i++) {
		if (account_key_order[i] != 0) {
			return -EINVAL;
		}
	}

	for (int i = 0; i < account_key_count; i++) {
		uint8_t id = ACCOUNT_KEY_METADATA_FIELD_GET(account_key_metadata[i], ID);
		bool id_found = false;

		for (size_t j = 0; j < account_key_count; j++) {
			if (account_key_order[j] == id) {
				id_found = true;
				break;
			}
		}
		if (!id_found) {
			if (ak_order_update_count >= account_key_count) {
				__ASSERT_NO_MSG(false);
				return -EINVAL;
			}
			ak_order_update_ram(id);
			ak_order_update_count++;
			/* Start loop again to check whether after update no existent AK has been
			 * lost. Set iterator to -1 to start next iteration with iterator equal to 0
			 * after i++ operation.
			 */
			i = -1;
		}
	}

	if (ak_order_update_count > 0) {
		err = settings_save_one(SETTINGS_AK_ORDER_FULL_NAME, account_key_order,
					sizeof(account_key_order));
		if (err) {
			return err;
		}
	}

	return 0;
}

static int bond_name_gen(char *name, uint8_t index)
{
	int n;

	n = snprintf(name, SETTINGS_BOND_NAME_MAX_SIZE, "%s%u", SETTINGS_BOND_FULL_PREFIX, index);
	__ASSERT_NO_MSG(n < SETTINGS_BOND_NAME_MAX_SIZE);
	if (n < 0) {
		return n;
	}

	return 0;
}

static size_t fp_bond_idx_get(struct fp_bond *bond)
{
	return bond - fp_bonds;
}

static int fp_bond_settings_delete(struct fp_bond *bond)
{
	int err;
	char name[SETTINGS_BOND_NAME_MAX_SIZE];

	err = bond_name_gen(name, fp_bond_idx_get(bond));
	if (err) {
		return err;
	}

	err = settings_delete(name);
	if (err) {
		return err;
	}

	return 0;
}

static int fp_bond_delete(struct fp_bond *bond)
{
	int err;

	err = fp_bond_settings_delete(bond);
	if (err) {
		return err;
	}

	*bond = (struct fp_bond){0};

	return 0;
}

static int bond_remove_completely(struct fp_bond *bond)
{
	int err;

	if (bond->bonded) {
		/* Bond entry will be deleted in the fp_storage_ak_bond_delete function called
		 * after bond removal from Bluetooth stack.
		 */
		err = bt_request_cb->bond_remove(&bond->nv.addr);
		if (err) {
			LOG_ERR("Failed to remove Bluetooth bond (err %d)", err);
			return err;
		}
	} else {
		err = fp_bond_delete(bond);
		if (err) {
			LOG_ERR("Failed to delete Fast Pair bond (err %d)", err);
			return err;
		}
	}

	LOG_DBG("Bond removed successfully");

	return 0;
}

static void bonds_with_invalid_id_remove_completely(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(fp_bonds); i++) {
		int err;
		bool account_key_found = false;

		if (bt_addr_le_eq(&fp_bonds[i].nv.addr, BT_ADDR_LE_ANY) || !fp_bonds[i].bonded ||
		    (fp_bonds[i].nv.account_key_metadata_id == BOND_AK_ID_UNKNOWN)) {
			continue;
		}

		for (size_t j = 0; j < account_key_count; j++) {
			if (fp_bonds[i].nv.account_key_metadata_id ==
			    ACCOUNT_KEY_METADATA_FIELD_GET(account_key_metadata[j], ID)) {
				account_key_found = true;
				break;
			}
		}

		if (!account_key_found) {
			LOG_DBG("Removing bond associated with invalid Account Key");
			err = bond_remove_completely(&fp_bonds[i]);
			if (err) {
				LOG_ERR("Failed to remove bond (err %d)", err);
			}
		}
	}
}

static void bonds_conn_ctx_reset(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(fp_bonds); i++) {
		fp_bonds[i].conn_ctx = NULL;
	}
}

static void bt_bonds_find(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(fp_bonds); i++) {
		fp_bonds[i].bonded = bt_request_cb->is_addr_bonded(&fp_bonds[i].nv.addr);
		if (fp_bonds[i].bonded) {
			LOG_DBG("Loaded bond has been found in the Bluetooth bond list");
		}
	}
}

static void bond_duplicates_handle(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(fp_bonds); i++) {
		bool duplicate_found = false;
		int err;

		if (bt_addr_le_eq(&fp_bonds[i].nv.addr, BT_ADDR_LE_ANY)) {
			continue;
		}

		for (size_t j = i + 1; j < ARRAY_SIZE(fp_bonds); j++) {
			if (bt_addr_le_eq(&fp_bonds[j].nv.addr, &fp_bonds[i].nv.addr)) {
				/* Error during Fast Pair Procedure. */
				duplicate_found = true;
				LOG_DBG("Removing both duplicated bond entries at bootup");
				err = fp_bond_delete(&fp_bonds[j]);
				if (err) {
					LOG_ERR("Failed to delete bond (err %d)", err);
				}
			}
		}

		if (duplicate_found) {
			err = bond_remove_completely(&fp_bonds[i]);
			if (err) {
				LOG_ERR("Failed to remove bond (err %d)", err);
			}
		}
	}
}

static void invalid_bonds_purge(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(fp_bonds); i++) {
		int err;

		if (bt_addr_le_eq(&fp_bonds[i].nv.addr, BT_ADDR_LE_ANY)) {
			continue;
		}

		if (!fp_bonds[i].bonded) {
			LOG_DBG("Deleting not bonded entry");
			err = fp_bond_delete(&fp_bonds[i]);
			if (err) {
				LOG_ERR("Failed to delete bond (err %d)", err);
			}

			continue;
		}

		if (fp_bonds[i].nv.account_key_metadata_id == BOND_AK_ID_UNKNOWN) {
			/* Fast Pair Procedure unfinished. */
			err = bond_remove_completely(&fp_bonds[i]);
			if (err) {
				LOG_ERR("Failed to remove bond (err %d)", err);
			}
		}
	}
}

static void validate_bonds(void)
{
	/* Assert that zero-initialized addresses are equal to BT_ADDR_LE_ANY. */
	static const bt_addr_le_t addr;

	__ASSERT_NO_MSG(bt_addr_le_eq(&addr, BT_ADDR_LE_ANY));
	ARG_UNUSED(addr);

	/* If Fast Pair was disabled before the peer that performed the unsuccessful Fast Pair
	 * Procedure disconnected, there may be a conn_ctx not set to NULL. All conn_ctx should be
	 * set to NULL at this point.
	 */
	bonds_conn_ctx_reset();

	bt_bonds_find();

	bond_duplicates_handle();

	invalid_bonds_purge();

	bonds_with_invalid_id_remove_completely();
}

static int fp_settings_validate(void)
{
	uint8_t id;
	int first_zero_idx = -1;
	int err;

	if (settings_set_err) {
		return settings_set_err;
	}

	for (size_t i = 0; i < ACCOUNT_KEY_CNT; i++) {
		id = ACCOUNT_KEY_METADATA_FIELD_GET(account_key_metadata[i], ID);
		if (id == 0) {
			first_zero_idx = i;
			break;
		}

		if (account_key_id_to_idx(id) != i) {
			return -EINVAL;
		}
	}

	if (first_zero_idx != -1) {
		for (size_t i = 0; i < first_zero_idx; i++) {
			id = ACCOUNT_KEY_METADATA_FIELD_GET(account_key_metadata[i], ID);
			if (id != ACCOUNT_KEY_MIN_ID + i) {
				return -EINVAL;
			}
		}

		for (size_t i = first_zero_idx + 1; i < ACCOUNT_KEY_CNT; i++) {
			id = ACCOUNT_KEY_METADATA_FIELD_GET(account_key_metadata[i], ID);
			if (id != 0) {
				return -EINVAL;
			}
		}

		account_key_count = first_zero_idx;
	} else {
		account_key_count = ACCOUNT_KEY_CNT;
	}

	err = validate_ak_order();
	if (err) {
		return err;
	}

	if (IS_ENABLED(CONFIG_BT_FAST_PAIR_STORAGE_AK_BOND)) {
		validate_bonds();
	}

	return 0;
}

int fp_storage_ak_count(void)
{
	if (!is_enabled) {
		return -EACCES;
	}

	return account_key_count;
}

int fp_storage_ak_get(struct fp_account_key *buf, size_t *key_count)
{
	if (!is_enabled) {
		return -EACCES;
	}

	if (*key_count < account_key_count) {
		return -EINVAL;
	}

	memcpy(buf, account_key_list, account_key_count * sizeof(account_key_list[0]));
	*key_count = account_key_count;

	return 0;
}

int fp_storage_ak_find(struct fp_account_key *account_key,
		       fp_storage_ak_check_cb account_key_check_cb, void *context)
{
	if (!is_enabled) {
		return -EACCES;
	}

	if (!account_key_check_cb) {
		return -EINVAL;
	}

	for (size_t i = 0; i < account_key_count; i++) {
		if (account_key_check_cb(&account_key_list[i], context)) {
			int err;

			ak_order_update_ram(ACCOUNT_KEY_METADATA_FIELD_GET(account_key_metadata[i],
									   ID));
			err = settings_save_one(SETTINGS_AK_ORDER_FULL_NAME, account_key_order,
						sizeof(account_key_order));
			if (err) {
				LOG_ERR("Unable to save new Account Key order in Settings. "
					"Not propagating the error and keeping updated Account Key "
					"order in RAM. After the Settings error the Account Key "
					"order may change at reboot.");
			}

			if (account_key) {
				*account_key = account_key_list[i];
			}

			return 0;
		}
	}

	return -ESRCH;
}

static int ak_name_gen(char *name, uint8_t index)
{
	int n;

	n = snprintf(name, SETTINGS_AK_NAME_MAX_SIZE, "%s%u", SETTINGS_AK_FULL_PREFIX, index);
	__ASSERT_NO_MSG(n < SETTINGS_AK_NAME_MAX_SIZE);
	if (n < 0) {
		return n;
	}

	return 0;
}

static struct fp_bond *fp_bond_get(const void *conn_ctx)
{
	for (size_t i = 0; i < ARRAY_SIZE(fp_bonds); i++) {
		if (fp_bonds[i].conn_ctx == conn_ctx) {
			return &fp_bonds[i];
		}
	}

	return NULL;
}

static int fp_bond_settings_save(struct fp_bond *bond, struct fp_bond *bond_rollback_copy)
{
	int err;
	size_t idx;
	char name[SETTINGS_BOND_NAME_MAX_SIZE];

	idx = fp_bond_idx_get(bond);

	err = bond_name_gen(name, idx);
	if (err) {
		*bond = *bond_rollback_copy;
		return err;
	}

	err = settings_save_one(name, &bond->nv, sizeof(bond->nv));
	if (err) {
		*bond = *bond_rollback_copy;
		return err;
	}

	return 0;
}

int fp_storage_ak_save(const struct fp_account_key *account_key, const void *conn_ctx)
{
	if (!is_enabled) {
		return -EACCES;
	}

	uint8_t id;
	uint8_t index;
	struct account_key_data data;
	char name[SETTINGS_AK_NAME_MAX_SIZE];
	int err;

	struct fp_bond *bond;
	struct fp_bond bond_rollback_copy;
	bool ak_overwritten = false;

	for (size_t i = 0; i < account_key_count; i++) {
		if (!memcmp(account_key->key, account_key_list[i].key, FP_ACCOUNT_KEY_LEN)) {
			LOG_INF("Account Key already saved - skipping.");
			return 0;
		}
	}
	if (account_key_count == ACCOUNT_KEY_CNT) {
		LOG_INF("Account Key List full - erasing the least recently used Account Key.");
	}

	id = next_account_key_id();
	index = account_key_id_to_idx(id);

	data.account_key_metadata = 0;
	ACCOUNT_KEY_METADATA_FIELD_SET(data.account_key_metadata, ID, id);
	data.account_key = *account_key;

	err = ak_name_gen(name, index);
	if (err) {
		return err;
	}

	if (IS_ENABLED(CONFIG_BT_FAST_PAIR_STORAGE_AK_BOND)) {
		__ASSERT_NO_MSG(conn_ctx);

		bond = fp_bond_get(conn_ctx);
		if (!bond) {
			return -EINVAL;
		}

		__ASSERT(bond->bonded, "Account Key cannot be written by unbonded connection.");

		bond_rollback_copy = *bond;

		bond->nv.account_key_metadata_id = id;

		err = fp_bond_settings_save(bond, &bond_rollback_copy);
		if (err) {
			return err;
		}
	} else {
		ARG_UNUSED(conn_ctx);
	}

	err = settings_save_one(name, &data, sizeof(data));
	if (err) {
		return err;
	}

	account_key_list[index] = *account_key;
	account_key_metadata[index] = data.account_key_metadata;

	if (account_key_count < ACCOUNT_KEY_CNT) {
		account_key_count++;
	} else {
		ak_overwritten = true;
	}

	if (IS_ENABLED(CONFIG_BT_FAST_PAIR_STORAGE_AK_BOND) &&
	    bond->bonded) {
		/* Procedure finished successfully. Setting conn_ctx to NULL. */
		bond->conn_ctx = NULL;
	}

	ak_order_update_ram(id);
	err = settings_save_one(SETTINGS_AK_ORDER_FULL_NAME, account_key_order,
				sizeof(account_key_order));
	if (err) {
		LOG_ERR("Unable to save new Account Key order in Settings. "
			"Not propagating the error and keeping updated Account Key "
			"order in RAM. After the Settings error the Account Key "
			"order may change at reboot.");
	}

	if (IS_ENABLED(CONFIG_BT_FAST_PAIR_STORAGE_AK_BOND) && ak_overwritten) {
		/* Account Key overwritten. Remove bonds releted with overwritten Account Key. */
		bonds_with_invalid_id_remove_completely();
	}

	return 0;
}

static int ak_id_get(uint8_t *id, const struct fp_account_key *account_key)
{
	for (size_t i = 0; i < account_key_count; i++) {
		if (!memcmp(account_key_list[i].key, account_key->key, FP_ACCOUNT_KEY_LEN)) {
			*id = ACCOUNT_KEY_METADATA_FIELD_GET(account_key_metadata[i], ID);
			return 0;
		}
	}

	return -ESRCH;
}

static struct fp_bond *fp_bond_free_get(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(fp_bonds); i++) {
		if (bt_addr_le_eq(&fp_bonds[i].nv.addr, BT_ADDR_LE_ANY)) {
			return &fp_bonds[i];
		}
	}

	return NULL;
}

static struct fp_bond *bonded_duplicate_get(const void *conn_ctx, const bt_addr_le_t *addr)
{
	for (size_t i = 0; i < ARRAY_SIZE(fp_bonds); i++) {
		if ((fp_bonds[i].conn_ctx != conn_ctx) &&
		    fp_bonds[i].bonded &&
		    bt_addr_le_eq(&fp_bonds[i].nv.addr, addr)) {
			return &fp_bonds[i];
		}
	}

	return NULL;
}

static void bond_update_addr(const void *conn_ctx, const bt_addr_le_t *new_addr)
{
	int err;
	struct fp_bond *bond;
	struct fp_bond bond_rollback_copy;

	bond = fp_bond_get(conn_ctx);
	if (!bond) {
		return;
	}
	bond_rollback_copy = *bond;

	bond->nv.addr = *new_addr;

	err = fp_bond_settings_save(bond, &bond_rollback_copy);
	if (err) {
		return;
	}
}

static struct fp_bond *bonded_by_addr_get(const bt_addr_le_t *addr)
{
	for (size_t i = 0; i < ARRAY_SIZE(fp_bonds); i++) {
		if (fp_bonds[i].bonded &&
		    bt_addr_le_eq(&fp_bonds[i].nv.addr, addr)) {
			return &fp_bonds[i];
		}
	}

	return NULL;
}

void fp_storage_ak_bond_bt_request_cb_register(const struct fp_storage_ak_bond_bt_request_cb *cb)
{
	if (!IS_ENABLED(CONFIG_BT_FAST_PAIR_STORAGE_AK_BOND)) {
		__ASSERT_NO_MSG(false);
		return;
	}

	__ASSERT_NO_MSG(cb);

	bt_request_cb = cb;
}

int fp_storage_ak_bond_conn_create(const void *conn_ctx, const bt_addr_le_t *addr,
				   const struct fp_account_key *account_key)
{
	if (!IS_ENABLED(CONFIG_BT_FAST_PAIR_STORAGE_AK_BOND)) {
		__ASSERT_NO_MSG(false);
		return -ENOTSUP;
	}

	if (!is_enabled) {
		return -EACCES;
	}

	int err;
	uint8_t id;
	struct fp_bond *bond;
	struct fp_bond bond_rollback_copy = (struct fp_bond){0};

	if (account_key) {
		err = ak_id_get(&id, account_key);
		if (err) {
			return err;
		}
	} else {
		/* The actual ID will be determined during Account Key write. */
		id = BOND_AK_ID_UNKNOWN;
	}

	bond = fp_bond_free_get();
	if (!bond) {
		return -ENOMEM;
	}

	bond->conn_ctx = conn_ctx;
	bond->bonded = false;
	bond->nv.account_key_metadata_id = id;
	bond->nv.addr = *addr;

	err = fp_bond_settings_save(bond, &bond_rollback_copy);
	if (err) {
		return err;
	}

	return 0;
}

void fp_storage_ak_bond_conn_confirm(const void *conn_ctx, const bt_addr_le_t *addr)
{
	if (!IS_ENABLED(CONFIG_BT_FAST_PAIR_STORAGE_AK_BOND)) {
		__ASSERT_NO_MSG(false);
		return;
	}

	if (!is_enabled) {
		return;
	}

	struct fp_bond *bond;
	struct fp_bond *bond_duplicate;
	int err;

	bond = fp_bond_get(conn_ctx);
	if (!bond) {
		return;
	}

	bond->bonded = true;

	for (size_t i = 0; i < ARRAY_SIZE(fp_bonds); i++) {
		bond_duplicate = bonded_duplicate_get(conn_ctx, addr);
		if (bond_duplicate) {
			err = fp_bond_delete(bond_duplicate);
			if (err) {
				LOG_ERR("Failed to delete bond (err %d)", err);
				break;
			}

			LOG_DBG("Deleted duplicated bond");
		} else {
			break;
		}
	}

	if (bond->nv.account_key_metadata_id) {
		/* Account Key already associated (subsequent pairing procedure).
		 * Procedure finished successfully. Setting conn_ctx to NULL.
		 */
		bond->conn_ctx = NULL;
	}
}

void fp_storage_ak_bond_conn_addr_update(const void *conn_ctx, const bt_addr_le_t *new_addr)
{
	if (!IS_ENABLED(CONFIG_BT_FAST_PAIR_STORAGE_AK_BOND)) {
		__ASSERT_NO_MSG(false);
		return;
	}

	if (!is_enabled) {
		return;
	}

	bond_update_addr(conn_ctx, new_addr);
}

void fp_storage_ak_bond_conn_cancel(const void *conn_ctx)
{
	if (!IS_ENABLED(CONFIG_BT_FAST_PAIR_STORAGE_AK_BOND)) {
		__ASSERT_NO_MSG(false);
		return;
	}

	if (!is_enabled) {
		return;
	}

	struct fp_bond *bond;
	int err;

	bond = fp_bond_get(conn_ctx);
	if (!bond) {
		return;
	}

	/* If at this point the conn_ctx is not set to NULL, the procedure is incomplete. */
	err = bond_remove_completely(bond);
	if (err) {
		LOG_ERR("Failed to remove bond (err %d)", err);
	}
}

void fp_storage_ak_bond_delete(const bt_addr_le_t *addr)
{
	if (!IS_ENABLED(CONFIG_BT_FAST_PAIR_STORAGE_AK_BOND)) {
		__ASSERT_NO_MSG(false);
		return;
	}

	if (!is_enabled) {
		return;
	}

	struct fp_bond *bond;
	int err;

	LOG_DBG("Bluetooth bond deleted");
	bond = bonded_by_addr_get(addr);
	if (!bond) {
		return;
	}

	err = fp_bond_delete(bond);
	if (err) {
		LOG_ERR("Failed to delete bond (err %d)", err);
	}
}

void fp_storage_ak_ram_clear(void)
{
	memset(account_key_list, 0, sizeof(account_key_list));
	memset(account_key_metadata, 0, sizeof(account_key_metadata));
	account_key_count = 0;

	memset(account_key_order, 0, sizeof(account_key_order));

	if (IS_ENABLED(CONFIG_BT_FAST_PAIR_STORAGE_AK_BOND)) {
		memset(fp_bonds, 0, sizeof(fp_bonds));
		bt_request_cb = NULL;
	}

	settings_set_err = 0;

	is_enabled = false;
}

bool fp_storage_ak_has_account_key(void)
{
	return (fp_storage_ak_count() > 0);
}

static int fp_storage_ak_init(void)
{
	int err;

	if (is_enabled) {
		LOG_WRN("fp_storage_ak module already initialized");
		return 0;
	}

	if (IS_ENABLED(CONFIG_BT_FAST_PAIR_STORAGE_AK_BOND) && !bt_request_cb) {
		LOG_ERR("bt_request_cb not set");
		return -EINVAL;
	}

	err = fp_settings_validate();
	if (err) {
		return err;
	}

	is_enabled = true;

	return 0;
}

static int fp_storage_ak_uninit(void)
{
	if (!is_enabled) {
		LOG_WRN("fp_storage_ak module already uninitialized");
		return 0;
	}

	is_enabled = false;

	return 0;
}

static int fp_storage_ak_delete(uint8_t index)
{
	int err;
	char name[SETTINGS_AK_NAME_MAX_SIZE];

	err = ak_name_gen(name, index);
	if (err) {
		return err;
	}

	err = settings_delete(name);
	if (err) {
		return err;
	}

	return 0;
}

static int fp_storage_ak_reset(void)
{
	int err;
	bool was_enabled = is_enabled;
	const struct fp_storage_ak_bond_bt_request_cb *registered_bt_request_cb = bt_request_cb;

	if (was_enabled) {
		err = fp_storage_ak_uninit();
		if (err) {
			return err;
		}
	}

	for (uint8_t index = 0; index < ACCOUNT_KEY_CNT; index++) {
		err = fp_storage_ak_delete(index);
		if (err) {
			return err;
		}
	}

	if (IS_ENABLED(CONFIG_BT_FAST_PAIR_STORAGE_AK_BOND)) {
		for (uint8_t index = 0; index < ARRAY_SIZE(fp_bonds); index++) {
			if (bt_addr_le_eq(&fp_bonds[index].nv.addr, BT_ADDR_LE_ANY)) {
				continue;
			}

			err = bt_request_cb->bond_remove(&fp_bonds[index].nv.addr);
			if (err == -ESRCH) {
				/* If the fp_storage_factory_reset is interrupted by power down
				 * after calling bt_request_cb->bond_remove and before getting
				 * fp_storage_ak_bond_delete then, when whole storage is being
				 * enabled, the storage reset is resumed without going through
				 * regular storage validation and we may here try to call
				 * bt_request_cb->bond_remove on already unpaired bond.
				 */
				LOG_WRN("Failed to remove Bluetooth bond because it is already "
					"unpaired. This might happen in some edge");
			} else {
				LOG_ERR("Failed to remove Bluetooth bond (err %d)", err);
			}

			err = fp_bond_settings_delete(&fp_bonds[index]);
			if (err) {
				return err;
			}
		}
	}

	err = settings_delete(SETTINGS_AK_ORDER_FULL_NAME);
	if (err) {
		return err;
	}

	fp_storage_ak_ram_clear();
	if (was_enabled) {
		if (IS_ENABLED(CONFIG_BT_FAST_PAIR_STORAGE_AK_BOND)) {
			fp_storage_ak_bond_bt_request_cb_register(registered_bt_request_cb);
		}

		err = fp_storage_ak_init();
		if (err) {
			return err;
		}
	}

	return 0;
}

static void reset_prepare(void)
{
	/* intentionally left empty */
}

SETTINGS_STATIC_HANDLER_DEFINE(fp_storage_ak, SETTINGS_AK_SUBTREE_NAME, NULL, fp_settings_set,
			       NULL, NULL);

FP_STORAGE_MANAGER_MODULE_REGISTER(fp_storage_ak, fp_storage_ak_reset, reset_prepare,
				   fp_storage_ak_init, fp_storage_ak_uninit);
