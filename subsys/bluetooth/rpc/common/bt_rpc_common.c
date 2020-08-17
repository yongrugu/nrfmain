

#include <zephyr/types.h>
#include <zephyr.h>
#include <device.h>

#include "nrf_rpc_cbor.h"

#include "bt_rpc_common.h"

#include <logging/log.h>

LOG_MODULE_REGISTER(BT_RPC, CONFIG_BT_RPC_LOG_LEVEL);


NRF_RPC_GROUP_DEFINE(bt_rpc_grp, "bt_rpc", NULL, NULL, NULL);


#ifdef CONFIG_BT_RPC_INITIALIZE_NRF_RPC


static void err_handler(const struct nrf_rpc_err_report *report)
{
	printk("nRF RPC error %d ocurred. See nRF RPC logs for more details.",
	       report->code);
	k_oops();
}


static int serialization_init(struct device *dev)
{
	ARG_UNUSED(dev);

	int err;

	printk("Init begin\n"); // TODO: Log instead of printk

	err = nrf_rpc_init(err_handler);
	if (err) {
		return -EINVAL;
	}

	printk("Init done\n");

	return 0;
}


SYS_INIT(serialization_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);




#endif /* CONFIG_BT_RPC_INITIALIZE_NRF_RPC */

enum {
	CHECK_ENTRY_FLAGS,
	CHECK_ENTRY_UINT,
	CHECK_ENTRY_STR,
};

struct check_list_entry {
	uint8_t type;
	uint8_t size;
	union
	{
		uint32_t value;
		const char *str_value;
	};
	const char* name;
};

#define DEF_
#define _GET_DEF2(a, b, c, ...) c
#define _GET_DEF1(...) _GET_DEF2(__VA_ARGS__)
#define _GET_DEF(x) (_GET_DEF1(DEF_##x, DEF_##x, x))

#define _GET_DEF_STR(_value) #_value

#if defined(CONFIG_BT_RPC_HOST)

typedef uint8_t check_list_entry_t;
typedef char str_check_list_entry_t;

#define CHECK_FLAGS(a, b, c, d, e, f, g, h) \
	(IS_ENABLED(a) ? 1 : 0) | \
	(IS_ENABLED(b) ? 2 : 0) | \
	(IS_ENABLED(c) ? 4 : 0) | \
	(IS_ENABLED(d) ? 8 : 0) | \
	(IS_ENABLED(e) ? 16 : 0) | \
	(IS_ENABLED(f) ? 32 : 0) | \
	(IS_ENABLED(g) ? 64 : 0) | \
	(IS_ENABLED(h) ? 128 : 0)

#define CHECK_UINT8(_value) _GET_DEF(_value)

#define CHECK_UINT16(_value) _GET_DEF(_value) & 0xFF, \
	(_GET_DEF(_value) >> 8) & 0xFF

#define CHECK_UINT8_PAIR(_value1, _value2) _GET_DEF(_value2), _GET_DEF(_value1)

#define CHECK_UINT16_PAIR(_value1, _value2) _GET_DEF(_value2) & 0xFF, \
	(_GET_DEF(_value2) >> 8) & 0xFF, \
	_GET_DEF(_value1) & 0xFF, \
	(_GET_DEF(_value1) >> 8) & 0xFF

#define CHECK_STR_BEGIN()

#define CHECK_STR(_value) _GET_DEF_STR(_value) "\0"

#define CHECK_STR_END()

#else

typedef struct check_list_entry check_list_entry_t;
typedef struct check_list_entry str_check_list_entry_t;

#define CHECK_FLAGS(a, b, c, d, e, f, g, h) \
	{ \
		.type = CHECK_ENTRY_FLAGS, \
		.size = 1, \
		.value = (IS_ENABLED(a) ? 1 : 0) | \
			 (IS_ENABLED(b) ? 2 : 0) | \
			 (IS_ENABLED(c) ? 4 : 0) | \
			 (IS_ENABLED(d) ? 8 : 0) | \
			 (IS_ENABLED(e) ? 16 : 0) | \
			 (IS_ENABLED(f) ? 32 : 0) | \
			 (IS_ENABLED(g) ? 64 : 0) | \
			 (IS_ENABLED(h) ? 128 : 0), \
		.name = #a "\0" #b "\0" #c "\0" #d "\0" \
			#e "\0" #f "\0" #g "\0" #h "\0" \
	}

#define CHECK_UINT8(_value)  \
	{ \
		.type = CHECK_ENTRY_UINT, \
		.size = 1, \
		.value = _GET_DEF(_value), \
		.name = #_value, \
	}

#define CHECK_UINT16(_value)  \
	{ \
		.type = CHECK_ENTRY_UINT, \
		.size = 2, \
		.value = _GET_DEF(_value), \
		.name = #_value, \
	}

#define CHECK_UINT8_PAIR(_value1, _value2)  \
	{ \
		.type = CHECK_ENTRY_UINT, \
		.size = 1, \
		.value = _GET_DEF(_value1), \
		.name = #_value1 " (net: " #_value2 ")", \
	}, \
	{ \
		.type = CHECK_ENTRY_UINT, \
		.size = 1, \
		.value = _GET_DEF(_value2), \
		.name = #_value2 " (net: " #_value1 ")", \
	}

#define CHECK_UINT16_PAIR(_value1, _value2) \
	{ \
		.type = CHECK_ENTRY_UINT, \
		.size = 2, \
		.value = _GET_DEF(_value1), \
		.name = #_value1 " (net: " #_value2 ")", \
	}, \
	{ \
		.type = CHECK_ENTRY_UINT, \
		.size = 2, \
		.value = _GET_DEF(_value2), \
		.name = #_value2 " (net: " #_value1 ")", \
	}

#define CHECK_STR_BEGIN() {

#define CHECK_STR(_value) \
	{ \
		.type = CHECK_ENTRY_STR, \
		.size = 0, \
		.str_value = _GET_DEF_STR(_value), \
		.name = #_value, \
	}, \

#define CHECK_STR_END() }

#endif

/*
 * Default values that will be put into the check table if specific
 * configuration is not defined. Each define MUST end with a comma.
 * Mostly, it is the maximum integer value of a specific size.
 */
#define DEF_CONFIG_BT_MAX_CONN 0xFF,
#define DEF_CONFIG_BT_ID_MAX 0xFF,
#define DEF_CONFIG_BT_EXT_ADV_MAX_ADV_SET 0xFF,

static const check_list_entry_t check_table[] = {
	CHECK_FLAGS(
		1,
		CONFIG_BT_CENTRAL,
		CONFIG_BT_PERIPHERAL,
		CONFIG_BT_WHITELIST,
		CONFIG_BT_USER_PHY_UPDATE,
		CONFIG_BT_USER_DATA_LEN_UPDATE,
		CONFIG_BT_PRIVACY,
		CONFIG_BT_SCAN_WITH_IDENTITY),
	CHECK_FLAGS(
		CONFIG_BT_REMOTE_VERSION,
		CONFIG_BT_SMP,
		CONFIG_BT_BREDR,
		CONFIG_BT_REMOTE_INFO,
		CONFIG_BT_FIXED_PASSKEY,
		CONFIG_BT_SMP_APP_PAIRING_ACCEPT,
		CONFIG_BT_EXT_ADV,
		0),
	CHECK_UINT8(CONFIG_BT_MAX_CONN),
	CHECK_UINT8(CONFIG_BT_ID_MAX),
	CHECK_UINT8(CONFIG_BT_EXT_ADV_MAX_ADV_SET),
	CHECK_UINT16_PAIR(CONFIG_CBKPROXY_OUT_SLOTS, CONFIG_CBKPROXY_IN_SLOTS),
};

static const str_check_list_entry_t str_check_table[] =
	CHECK_STR_BEGIN()
	CHECK_STR(CONFIG_BT_DEVICE_NAME)
	CHECK_STR_END();

#if defined(CONFIG_BT_RPC_HOST)

void bt_rpc_get_check_table(uint8_t *data, size_t size)
{
	size_t str_copy_bytes = sizeof(str_check_table);

	memset(data, 0, size);

	if (size < sizeof(check_table)) {
		return;
	} else if (size < sizeof(check_table) + str_copy_bytes) {
		str_copy_bytes = size - sizeof(check_table);
	}

	memcpy(data, check_table, sizeof(check_table));
	memcpy(&data[sizeof(check_table)], str_check_table, str_copy_bytes);
	
	LOG_DBG("Check table size: %d+%d=%d (copied %d)", sizeof(check_table),
		sizeof(str_check_table),
		sizeof(check_table) + sizeof(str_check_table),
		sizeof(check_table) + str_copy_bytes);
}

#else

static bool validate_flags(const check_list_entry_t *entry, uint8_t flags)
{
	size_t i;
	uint8_t host;
	uint8_t client;
	const char* name;

	if (entry->value != flags) {
		name = entry->name;

		for (i = 0; i < 8; i++) {
			host = ((flags >> i) & 1);
			client = ((entry->value >> i) & 1);
			if (host != client) {
				LOG_ERR("Missmatched %s: net=%d, app=%d", name, host, client);
			}
			name += strlen(name) + 1;
		}

		return false;
	} else {
		return true;
	}
}

static bool validate_uint(const check_list_entry_t *entry, const uint8_t *data)
{
	uint32_t value = 0;
	size_t i;

	for (i = 0; i < entry->size; i++) {
		value <<= 8;
		value |= data[entry->size - i - 1];
	}

	if (value != entry->value) {
		LOG_ERR("Missmatched %s: net=%d, app=%d", entry->name, value, entry->value);
		return false;
	} else {
		return true;
	}
}

static bool validate_str(const check_list_entry_t *entry, uint8_t **data, size_t *size)
{
	size_t client_len;
	size_t host_len;
	const char* client = entry->str_value;
	const char* host = (const char*)(*data);

	if (*size == 0) {
		LOG_ERR("Missmatched BT RPC config.");
		return false;
	}

	host_len = strlen(host) + 1;
	*data += host_len;
	*size -= host_len;
	client_len = strlen(client) + 1;

	if (client_len != host_len || memcmp(client, host, client_len) != 0) {
		if (host[0] != '"') {
			host = "#undef";
		}
		if (client[0] != '"') {
			client = "#undef";
		}
		LOG_ERR("Missmatched %s: net=%s, app=%s", entry->name, host, client);
		return false;
	}

	return true;
}

static bool check_table_part(uint8_t **data, size_t *size,
			     const check_list_entry_t *table, size_t items)
{
	size_t i;
	const check_list_entry_t *entry;
	bool ok = true;

	for (i = 0; i < items; i++) {
		entry = &table[i];
		if (*size < entry->size) {
			LOG_ERR("Missmatched BT RPC config.");
			return false;
		}
		switch (entry->type)
		{
		case CHECK_ENTRY_FLAGS:
			ok = validate_flags(entry, (*data)[0]) && ok;
			break;

		case CHECK_ENTRY_UINT:
			ok = validate_uint(entry, (*data)) && ok;
			break;

		case CHECK_ENTRY_STR:
			ok = validate_str(entry, data, size) && ok;
			break;
		}
		*data += entry->size;
		*size -= entry->size;
	}

	return ok;
}

bool bt_rpc_validate_check_table(uint8_t *data, size_t size)
{
	bool ok = true;

	if (data[0] == 0) {
		LOG_ERR("Missmatched BT RPC config.");
		return false;
	}
	
	data[size - 1] = 0;

	ok = check_table_part(&data, &size, check_table, ARRAY_SIZE(check_table));
	ok = check_table_part(&data, &size, str_check_table, ARRAY_SIZE(str_check_table)) && ok;

	if (size != 1) {
		LOG_ERR("Missmatched BT RPC config.");
		return false;
	}
	
	if (ok) {
		LOG_INF("Matching configuration");
	}

	return ok;
}

size_t bt_rpc_calc_check_table_size()
{
	size_t i;
	size_t size = 0;
	size_t str_size = 0;

	for (i = 0; i < ARRAY_SIZE(check_table); i++) {
		size += check_table[i].size;
	}

	for (i = 0; i < ARRAY_SIZE(str_check_table); i++) {
		str_size += strlen(str_check_table[i].str_value) + 1;
	}
	str_size++;

	LOG_DBG("Check table size: %d+%d=%d", size, str_size, size + str_size);

	return size + str_size;
}

#endif
