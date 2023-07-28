/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef __SUPP_EVENTS_H__
#define __SUPP_EVENTS_H__

#include <zephyr/net/wifi_mgmt.h>

/* Connectivity Events */
#define _NET_MGMT_WPA_SUPP_LAYER NET_MGMT_LAYER_L2
#define _NET_MGMT_WPA_SUPP_CODE 0x157
#define _NET_MGMT_WPA_SUPP_BASE (NET_MGMT_LAYER(_NET_MGMT_WPA_SUPP_LAYER) | \
				NET_MGMT_LAYER_CODE(_NET_MGMT_WPA_SUPP_CODE) | \
				NET_MGMT_IFACE_BIT)
#define _NET_MGMT_WPA_SUPP_EVENT (NET_MGMT_EVENT_BIT | _NET_MGMT_WPA_SUPP_BASE)

enum net_event_wpa_supp_cmd {
	NET_EVENT_WPA_SUPP_CMD_READY = 1,
	NET_EVENT_WPA_SUPP_CMD_NOT_READY,
	NET_EVENT_WPA_SUPP_CMD_IFACE_ADDED,
	NET_EVENT_WPA_SUPP_CMD_IFACE_REMOVING,
	NET_EVENT_WPA_SUPP_CMD_IFACE_REMOVED,
	NET_EVENT_WIFI_CMD_MAX
};

#define NET_EVENT_WPA_SUPP_READY					\
	(_NET_MGMT_WPA_SUPP_EVENT | NET_EVENT_WPA_SUPP_CMD_READY)

#define NET_EVENT_WPA_SUPP_NOT_READY					\
	(_NET_MGMT_WPA_SUPP_EVENT | NET_EVENT_WPA_SUPP_CMD_NOT_READY)

#define NET_EVENT_WPA_SUPP_IFACE_ADDED					\
	(_NET_MGMT_WPA_SUPP_EVENT | NET_EVENT_WPA_SUPP_CMD_IFACE_ADDED)

#define NET_EVENT_WPA_SUPP_IFACE_REMOVED					\
	(_NET_MGMT_WPA_SUPP_EVENT | NET_EVENT_WPA_SUPP_CMD_IFACE_REMOVED)

#define NET_EVENT_WPA_SUPP_IFACE_REMOVING					\
	(_NET_MGMT_WPA_SUPP_EVENT | NET_EVENT_WPA_SUPP_CMD_IFACE_REMOVING)

int send_wifi_mgmt_event(const char *ifname, enum net_event_wifi_cmd event, int status);
int generate_supp_state_event(const char *ifname, enum net_event_wpa_supp_cmd event, int status);

#endif /* __SUPP_EVENTS_H__ */
