/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <stdio.h>
#include <zephyr/logging/log.h>
#include <date_time.h>
#include <nrf_modem_gnss.h>
#include <net/nrf_cloud.h>
#include <net/nrf_cloud_agps.h>
#include <net/nrf_cloud_pgps.h>
#include <net/nrf_cloud_cell_pos.h>
#include <net/nrf_cloud_rest.h>
#include "slm_util.h"
#include "slm_at_host.h"
#include "slm_at_gnss.h"

LOG_MODULE_REGISTER(slm_gnss, CONFIG_SLM_LOG_LEVEL);

#define SERVICE_INFO_GPS \
	"{\"state\":{\"reported\":{\"device\": {\"serviceInfo\":{\"ui\":[\"GPS\"]}}}}}"

/**@brief GNSS operations. */
enum slm_gnss_operation {
	GPS_STOP,
	GPS_START,
	nRF_CLOUD_DISCONNECT = GPS_STOP,
	nRF_CLOUD_CONNECT    = GPS_START,
	nRF_CLOUD_SEND,
	AGPS_STOP            = GPS_STOP,
	AGPS_START           = GPS_START,
	PGPS_STOP            = GPS_STOP,
	PGPS_START           = GPS_START,
	CELLPOS_STOP         = GPS_STOP,
	CELLPOS_START_SCELL  = GPS_START,
	CELLPOS_START_MCELL  = nRF_CLOUD_SEND
};

static struct k_work agps_req;
static struct k_work pgps_req;
static struct k_work fix_rep;
static struct k_work cell_pos_req;
static enum nrf_cloud_cell_pos_type cell_pos_type;

static bool nrf_cloud_initd;
static bool nrf_cloud_ready;
static bool location_signify;
static uint64_t ttft_start;
static enum {
	RUN_TYPE_NONE,
	RUN_TYPE_GPS,
	RUN_TYPE_AGPS,
	RUN_TYPE_PGPS,
	RUN_TYPE_CELL_POS
} run_type;
static enum {
	RUN_STATUS_STOPPED,
	RUN_STATUS_STARTED,
	RUN_STATUS_PERIODIC_WAKEUP,
	RUN_STATUS_SLEEP_AFTER_TIMEOUT,
	RUN_STATUS_SLEEP_AFTER_FIX,
	RUN_STATUS_MAX
} run_status;

static K_SEM_DEFINE(sem_date_time, 0, 1);

/** Definitins for %NCELLMEAS notification
 * %NCELLMEAS: status [,<cell_id>, <plmn>, <tac>, <timing_advance>, <current_earfcn>,
 * <current_phys_cell_id>, <current_rsrp>, <current_rsrq>,<measurement_time>,]
 * [,<n_earfcn>1, <n_phys_cell_id>1, <n_rsrp>1, <n_rsrq>1,<time_diff>1]
 * [,<n_earfcn>2, <n_phys_cell_id>2, <n_rsrp>2, <n_rsrq>2,<time_diff>2] ...
 * [,<n_earfcn>17, <n_phys_cell_id>17, <n_rsrp>17, <n_rsrq>17,<time_diff>17
 *
 * Max 17 ncell, but align with CONFIG_SLM_AT_MAX_PARAM
 * 11 number of parameters for current cell (including "%NCELLMEAS")
 * 5  number of parameters for one neighboring cell
 */
#define MAX_PARAM_CELL   11
#define MAX_PARAM_NCELL  5
/* Must support at least all params for current cell plus one ncell */
BUILD_ASSERT(CONFIG_SLM_AT_MAX_PARAM > (MAX_PARAM_CELL + MAX_PARAM_NCELL),
	     "CONFIG_SLM_AT_MAX_PARAM too small");
#define NCELL_CNT ((CONFIG_SLM_AT_MAX_PARAM - MAX_PARAM_CELL) / MAX_PARAM_NCELL)

static struct lte_lc_ncell neighbor_cells[NCELL_CNT];
static struct lte_lc_cells_info cell_data = {
	.neighbor_cells = neighbor_cells
};
static int ncell_meas_status;
static char device_id[NRF_CLOUD_CLIENT_ID_MAX_LEN];

#define REST_RX_BUF_SZ			1024
#define REST_LOCATION_REPORT_MS		5000

/* Buffer used for REST calls */
static char rx_buf[REST_RX_BUF_SZ];
/* nRF Cloud REST context */
struct nrf_cloud_rest_context rest_ctx = {
	.connect_socket = -1,
	.keep_alive = false,
	.timeout_ms = NRF_CLOUD_REST_TIMEOUT_NONE,
	.rx_buf = rx_buf,
	.rx_buf_len = sizeof(rx_buf),
	.fragment_size = 0,
	.auth = NULL
};

/* global variable defined in different files */
extern struct k_work_q slm_work_q;
extern struct at_param_list at_param_list;
extern char rsp_buf[CONFIG_SLM_SOCKET_RX_MAX * 2];

static bool is_gnss_activated(void)
{
	int gnss_support = 0;
	int cfun_mode = 0;

	/*parse %XSYSTEMMODE=<LTE_M_support>,<NB_IoT_support>,<GNSS_support>,<LTE_preference> */
	if (nrf_modem_at_scanf("AT%XSYSTEMMODE?",
			       "%XSYSTEMMODE: %*d,%*d,%d", &gnss_support) == 1) {
		if (gnss_support == 0) {
			return false;
		}
	}

	/*parse +CFUN: <fun> */
	if (nrf_modem_at_scanf("AT+CFUN?", "+CFUN: %d", &cfun_mode) == 1) {
		if (cfun_mode == 1 || cfun_mode == 31) {
			return true;
		}
	}

	return false;
}

static void gnss_status_notify(void)
{
	if (run_type == RUN_TYPE_AGPS) {
		sprintf(rsp_buf, "\r\n#XAGPS: 1,%d\r\n", run_status);
	} else if (run_type == RUN_TYPE_PGPS) {
		sprintf(rsp_buf, "\r\n#XPGPS: 1,%d\r\n", run_status);
	} else {
		sprintf(rsp_buf, "\r\n#XGPS: 1,%d\r\n", run_status);
	}
	rsp_send(rsp_buf, strlen(rsp_buf));
}

static int gnss_startup(int type)
{
	int ret;

	/* Set run_type first as modem send NRF_MODEM_GNSS_EVT_AGPS_REQ instantly */
	run_type = type;

	ret = nrf_modem_gnss_start();
	if (ret) {
		LOG_ERR("Failed to start GPS, error: %d", ret);
		run_type = RUN_TYPE_NONE;
	} else {
		ttft_start = k_uptime_get();
		run_status = RUN_STATUS_STARTED;
		gnss_status_notify();
	}

	return ret;
}

static int gnss_shutdown(void)
{
	int ret = nrf_modem_gnss_stop();

	LOG_INF("GNSS stop %d", ret);
	run_status = RUN_STATUS_STOPPED;
	gnss_status_notify();
	run_type = RUN_TYPE_NONE;

	if (nrf_cloud_ready) {
		(void)nrf_cloud_rest_disconnect(&rest_ctx);
	}

	return ret;
}

static int read_agps_req(struct nrf_modem_gnss_agps_data_frame *req)
{
	int err;

	err = nrf_modem_gnss_read((void *)req, sizeof(*req),
					NRF_MODEM_GNSS_DATA_AGPS_REQ);
	if (err) {
		LOG_ERR("Failed to read GNSS AGPS req, error %d", err);
		return -EAGAIN;
	}

	return 0;
}

static void agps_req_wk(struct k_work *work)
{
	int err;
	struct nrf_modem_gnss_agps_data_frame req;

	ARG_UNUSED(work);

	err = read_agps_req(&req);
	if (err) {
		return;
	}

	err = nrf_cloud_agps_request(&req);
	if (err) {
		LOG_ERR("Failed to request A-GPS data: %d", err);
	}
}

static void pgps_req_wk(struct k_work *work)
{
	int err;

	ARG_UNUSED(work);

	/* Indirect request of P-GPS data and periodic injection */
	err = nrf_cloud_pgps_notify_prediction();
	if (err) {
		LOG_ERR("Failed to request notify of prediction: %d", err);
	}
}

AT_MONITOR(ncell_meas, "NCELLMEAS", ncell_meas_mon, PAUSED);

static void ncell_meas_mon(const char *notify)
{
	int err;
	uint32_t param_count;

	ncell_meas_status = -1;
	at_params_list_clear(&at_param_list);
	err = at_parser_params_from_str(notify, NULL, &at_param_list);
	if (err) {
		goto exit;
	}

	/* parse status, 0: success 1: fail */
	err = at_params_int_get(&at_param_list, 1, &ncell_meas_status);
	if (err) {
		goto exit;
	}
	if (ncell_meas_status != 0) {
		LOG_ERR("NCELLMEAS failed");
		err = -EAGAIN;
		goto exit;
	}
	param_count = at_params_valid_count_get(&at_param_list);
	if (param_count < MAX_PARAM_CELL) { /* at least current cell */
		LOG_ERR("Missing param in NCELLMEAS notification");
		err = -EAGAIN;
		goto exit;
	}

	/* parse Cell ID */
	char cid[9] = {0};
	size_t size;

	size = sizeof(cid);
	err = util_string_get(&at_param_list, 2, cid, &size);
	if (err) {
		goto exit;
	}
	err = util_str_to_int(cid, 16, (int *)&cell_data.current_cell.id);
	if (err) {
		goto exit;
	}

	/* parse PLMN */
	char plmn[6] = {0};
	char mcc[4]  = {0};

	size = sizeof(plmn);
	err = util_string_get(&at_param_list, 3, plmn, &size);
	if (err) {
		goto exit;
	}
	strncpy(mcc, plmn, 3); /* MCC always 3-digit */
	err = util_str_to_int(mcc, 10, &cell_data.current_cell.mcc);
	if (err) {
		goto exit;
	}
	err = util_str_to_int(&plmn[3], 10, &cell_data.current_cell.mnc);
	if (err) {
		goto exit;
	}

	/* parse TAC */
	char tac[9] = {0};

	size = sizeof(tac);
	err = util_string_get(&at_param_list, 4, tac, &size);
	if (err) {
		goto exit;
	}
	err = util_str_to_int(tac, 16, (int *)&cell_data.current_cell.tac);
	if (err) {
		goto exit;
	}

	/* omit timing_advance */
	cell_data.current_cell.timing_advance = NRF_CLOUD_CELL_POS_OMIT_TIME_ADV;

	/* parse EARFCN */
	err = at_params_unsigned_int_get(&at_param_list, 6, &cell_data.current_cell.earfcn);
	if (err) {
		goto exit;
	}

	/* parse PCI */
	err = at_params_unsigned_short_get(&at_param_list, 7,
					   &cell_data.current_cell.phys_cell_id);
	if (err) {
		goto exit;
	}

	/* parse RSRP and RSRQ */
	err = at_params_short_get(&at_param_list, 8, &cell_data.current_cell.rsrp);
	if (err < 0) {
		goto exit;
	}
	err = at_params_short_get(&at_param_list, 9, &cell_data.current_cell.rsrq);
	if (err < 0) {
		goto exit;
	}

	/* omit measurement_time */

	cell_data.ncells_count = 0;
	for (int i = 0; i < NCELL_CNT; i++) {
		int offset = i * MAX_PARAM_NCELL + 11;

		if (param_count < (offset + MAX_PARAM_NCELL)) {
			break;
		}

		/* parse n_earfcn */
		err = at_params_unsigned_int_get(&at_param_list, offset,
						 &neighbor_cells[i].earfcn);
		if (err < 0) {
			goto exit;
		}

		/* parse n_phys_cell_id */
		err = at_params_unsigned_short_get(&at_param_list, offset + 1,
						   &neighbor_cells[i].phys_cell_id);
		if (err < 0) {
			goto exit;
		}

		/* parse n_rsrp */
		err = at_params_short_get(&at_param_list, offset + 2, &neighbor_cells[i].rsrp);
		if (err < 0) {
			goto exit;
		}

		/* parse n_rsrq */
		err = at_params_short_get(&at_param_list, offset + 3, &neighbor_cells[i].rsrq);
		if (err < 0) {
			goto exit;
		}

		/* omit time_diff */

		cell_data.ncells_count++;
	}

	err = 0;

exit:
	LOG_INF("NCELLMEAS notification parse (err: %d)", err);
}

static void cell_pos_req_wk(struct k_work *work)
{
	int err;

	ARG_UNUSED(work);

	if (cell_pos_type == CELL_POS_TYPE_SINGLE) {
		err = nrf_cloud_cell_pos_request(NULL, true, NULL);
		if (err) {
			LOG_ERR("Failed to request SCELL, error: %d", err);
		} else {
			LOG_INF("nRF Cloud SCELL requested");
		}
	} else {
		if (ncell_meas_status == 0 && cell_data.current_cell.id != 0) {
			err = nrf_cloud_cell_pos_request(&cell_data, true, NULL);
			if (err) {
				LOG_ERR("Failed to request MCELL, error: %d", err);
			} else {
				LOG_INF("nRF Cloud MCELL requested, with %d neighboring cells",
					cell_data.ncells_count);
			}
		} else {
			LOG_WRN("No request of MCELL");
			sprintf(rsp_buf, "\r\n#XCELLPOS: \r\n");
			rsp_send(rsp_buf, strlen(rsp_buf));
			run_type = RUN_TYPE_NONE;
		}
	}
}

static void pgps_event_handler(struct nrf_cloud_pgps_event *event)
{
	int err;

	switch (event->type) {
	/* P-GPS initialization beginning. */
	case PGPS_EVT_INIT:
		LOG_INF("PGPS_EVT_INIT");
		break;
	/* There are currently no P-GPS predictions available. */
	case PGPS_EVT_UNAVAILABLE:
		LOG_INF("PGPS_EVT_UNAVAILABLE");
		break;
	/* P-GPS predictions are being loaded from the cloud. */
	case PGPS_EVT_LOADING:
		LOG_INF("PGPS_EVT_LOADING");
		break;
	/* A P-GPS prediction is available now for the current date and time. */
	case PGPS_EVT_AVAILABLE: {
		struct nrf_modem_gnss_agps_data_frame req;

		LOG_INF("PGPS_EVT_AVAILABLE");
		/* read out previous NRF_MODEM_GNSS_EVT_AGPS_REQ */
		err = read_agps_req(&req);
		if (err) {
			/* All assistance elements as requested */
			err = nrf_cloud_pgps_inject(event->prediction, &req);
		} else {
			/* ephemerides assistance only */
			err = nrf_cloud_pgps_inject(event->prediction, NULL);
		}
		if (err) {
			LOG_ERR("Unable to send prediction to modem: %d", err);
			break;
		}
		err = nrf_cloud_pgps_preemptive_updates();
		if (err) {
			LOG_ERR("Preemptive updates error: %d", err);
		}
	} break;
	/* All P-GPS predictions are available. */
	case PGPS_EVT_READY:
		LOG_INF("PGPS_EVT_READY");
		break;
	default:
		break;
	}
}

static void on_gnss_evt_pvt(void)
{
	struct nrf_modem_gnss_pvt_data_frame pvt;
	int err;

	err = nrf_modem_gnss_read((void *)&pvt, sizeof(pvt), NRF_MODEM_GNSS_DATA_PVT);
	if (err) {
		LOG_ERR("Failed to read GNSS PVT data, error %d", err);
		return;
	}
	for (int i = 0; i < NRF_MODEM_GNSS_MAX_SATELLITES; ++i) {
		if (pvt.sv[i].sv) { /* SV number 0 indicates no satellite */
			LOG_DBG("SV:%3d sig: %d c/n0:%4d",
				pvt.sv[i].sv, pvt.sv[i].signal, pvt.sv[i].cn0);
		}
	}
}

static void fix_rep_wk(struct k_work *work)
{
	int err;
	struct nrf_modem_gnss_pvt_data_frame pvt;
	struct nrf_modem_gnss_nmea_data_frame nmea;
	int64_t ts_ms;
	static int64_t last_ts_ms;

	ARG_UNUSED(work);

	err = nrf_modem_gnss_read((void *)&pvt, sizeof(pvt), NRF_MODEM_GNSS_DATA_PVT);
	if (err) {
		LOG_ERR("Failed to read GNSS PVT data, error %d", err);
		return;
	}

	/* GIS accuracy: http://wiki.gis.com/wiki/index.php/Decimal_degrees, use default .6lf */
	sprintf(rsp_buf,
		"\r\n#XGPS: %lf,%lf,%f,%f,%f,%f,\"%04u-%02u-%02u %02u:%02u:%02u\"\r\n",
		pvt.latitude, pvt.longitude, pvt.altitude,
		pvt.accuracy, pvt.speed, pvt.heading,
		pvt.datetime.year, pvt.datetime.month, pvt.datetime.day,
		pvt.datetime.hour, pvt.datetime.minute, pvt.datetime.seconds);
	rsp_send(rsp_buf, strlen(rsp_buf));

	for (int i = 0; i < NRF_MODEM_GNSS_MAX_SATELLITES; ++i) {
		if (pvt.sv[i].sv) { /* SV number 0 indicates no satellite */
			LOG_INF("SV:%3d sig: %d c/n0:%4d el:%3d az:%3d in-fix: %d unhealthy: %d",
				pvt.sv[i].sv, pvt.sv[i].signal, pvt.sv[i].cn0,
				pvt.sv[i].elevation, pvt.sv[i].azimuth,
				(pvt.sv[i].flags & NRF_MODEM_GNSS_SV_FLAG_USED_IN_FIX) ? 1 : 0,
				(pvt.sv[i].flags & NRF_MODEM_GNSS_SV_FLAG_UNHEALTHY) ? 1 : 0);
		}
	}

	err = nrf_modem_gnss_read((void *)&nmea, sizeof(nmea), NRF_MODEM_GNSS_DATA_NMEA);
	if (err) {
		LOG_WRN("Failed to read GNSS NMEA data, error %d", err);
	} else {
		/* Report to nRF Cloud by best-effort */
		if (nrf_cloud_ready && location_signify) {
			err = date_time_now(&ts_ms);
			if (err) {
				err = nrf_cloud_rest_send_location(&rest_ctx, device_id,
								nmea.nmea_str, -1);
			} else if (last_ts_ms == 0 ||
				ts_ms > last_ts_ms + REST_LOCATION_REPORT_MS) {
				last_ts_ms = ts_ms;
				err = nrf_cloud_rest_send_location(&rest_ctx, device_id,
								nmea.nmea_str, ts_ms);
			}
			if (err) {
				LOG_WRN("Failed to send location, error %d", err);
			}
		}
		/* GGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,x.x,xxxx \r\n */
		sprintf(rsp_buf, "\r\n#XGPS: %s", nmea.nmea_str);
		rsp_send(rsp_buf, strlen(rsp_buf));
	}

	if (run_type == RUN_TYPE_PGPS) {
		struct tm gps_time = {
			.tm_year = pvt.datetime.year - 1900,
			.tm_mon  = pvt.datetime.month - 1,
			.tm_mday = pvt.datetime.day,
			.tm_hour = pvt.datetime.hour,
			.tm_min  = pvt.datetime.minute,
			.tm_sec  = pvt.datetime.seconds,
		};

		/* help date_time to save SNTP transactions */
		date_time_set(&gps_time);
		/* help nrf_cloud_pgps as most recent known location */
		nrf_cloud_pgps_set_location(pvt.latitude, pvt.longitude);
	}
}

static void on_gnss_evt_fix(void)
{
	if (ttft_start != 0) {
		uint64_t delta = k_uptime_delta(&ttft_start);

		LOG_INF("TTFF %d.%ds", (int)(delta/1000), (int)(delta%1000));
		ttft_start = 0;
	}

	k_work_submit_to_queue(&slm_work_q, &fix_rep);
}

static void on_gnss_evt_agps_req(void)
{
	if (run_type == RUN_TYPE_AGPS) {
		k_work_submit_to_queue(&slm_work_q, &agps_req);
	} else if (run_type == RUN_TYPE_PGPS) {
		/* Check whether prediction data available or not */
		k_work_submit_to_queue(&slm_work_q, &pgps_req);
	}
}

/* NOTE this event handler runs in interrupt context */
static void gnss_event_handler(int event)
{
	switch (event) {
	case NRF_MODEM_GNSS_EVT_PVT:
		LOG_DBG("GNSS_EVT_PVT");
		on_gnss_evt_pvt();
		break;
	case NRF_MODEM_GNSS_EVT_FIX:
		LOG_INF("GNSS_EVT_FIX");
		on_gnss_evt_fix();
		break;
	case NRF_MODEM_GNSS_EVT_NMEA:
		LOG_DBG("GNSS_EVT_NMEA");
		break;
	case NRF_MODEM_GNSS_EVT_AGPS_REQ:
		LOG_INF("GNSS_EVT_AGPS_REQ");
		on_gnss_evt_agps_req();
		break;
	case NRF_MODEM_GNSS_EVT_BLOCKED:
		LOG_INF("GNSS_EVT_BLOCKED");
		break;
	case NRF_MODEM_GNSS_EVT_UNBLOCKED:
		LOG_INF("GNSS_EVT_UNBLOCKED");
		break;
	case NRF_MODEM_GNSS_EVT_PERIODIC_WAKEUP:
		LOG_INF("GNSS_EVT_PERIODIC_WAKEUP");
		run_status = RUN_STATUS_PERIODIC_WAKEUP;
		gnss_status_notify();
		break;
	case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_TIMEOUT:
		LOG_INF("GNSS_EVT_SLEEP_AFTER_TIMEOUT");
		run_status = RUN_STATUS_SLEEP_AFTER_TIMEOUT;
		gnss_status_notify();
		break;
	case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_FIX:
		LOG_INF("GNSS_EVT_SLEEP_AFTER_FIX");
		run_status = RUN_STATUS_SLEEP_AFTER_FIX;
		gnss_status_notify();
		break;
	case NRF_MODEM_GNSS_EVT_REF_ALT_EXPIRED:
		LOG_INF("GNSS_EVT_REF_ALT_EXPIRED");
		break;
	default:
		break;
	}
}

static int do_cloud_send_msg(const char *message, int len)
{
	int err;
	struct nrf_cloud_tx_data msg = {
		.data.ptr = message,
		.data.len = len,
		.topic_type = NRF_CLOUD_TOPIC_MESSAGE,
		.qos = MQTT_QOS_0_AT_MOST_ONCE
	};

	err = nrf_cloud_send(&msg);
	if (err) {
		LOG_ERR("nrf_cloud_send failed, error: %d", err);
	}

	return err;
}

static void on_cloud_evt_ready(void)
{
	if (location_signify) {
		int err;
		struct nrf_cloud_tx_data msg = {
			.data.ptr = SERVICE_INFO_GPS,
			.data.len = strlen(SERVICE_INFO_GPS),
			.topic_type = NRF_CLOUD_TOPIC_STATE,
			.qos = MQTT_QOS_0_AT_MOST_ONCE
		};

		/* Update nRF Cloud with GPS service info signifying GPS capabilities. */
		err = nrf_cloud_send(&msg);
		if (err) {
			LOG_WRN("Failed to send message to cloud, error: %d", err);
		}
	}

	nrf_cloud_ready = true;
	sprintf(rsp_buf, "\r\n#XNRFCLOUD: %d,%d\r\n", nrf_cloud_ready, location_signify);
	rsp_send(rsp_buf, strlen(rsp_buf));
	at_monitor_resume(&ncell_meas);
}

static void on_cloud_evt_disconnected(void)
{
	nrf_cloud_ready = false;
	(void)nrf_cloud_rest_disconnect(&rest_ctx);
	sprintf(rsp_buf, "\r\n#XNRFCLOUD: %d,%d\r\n", nrf_cloud_ready, location_signify);
	rsp_send(rsp_buf, strlen(rsp_buf));
	at_monitor_pause(&ncell_meas);
}

static void on_cloud_evt_data_received(const struct nrf_cloud_data *const data)
{
	int err = 0;

	if (run_type == RUN_TYPE_AGPS) {
		err = nrf_cloud_agps_process(data->ptr, data->len);
		if (err) {
			LOG_INF("Unable to process A-GPS data, error: %d", err);
		}
	} else if (run_type == RUN_TYPE_PGPS) {
		err = nrf_cloud_pgps_process(data->ptr, data->len);
		if (err) {
			LOG_ERR("Unable to process P-GPS data, error: %d", err);
		}
	} else if (run_type == RUN_TYPE_CELL_POS) {
		struct nrf_cloud_cell_pos_result result;

		err = nrf_cloud_cell_pos_process(data->ptr, &result);
		if (err == 0) {
			if (ttft_start != 0) {
				LOG_INF("TTFF %ds", (int)k_uptime_delta(&ttft_start)/1000);
				ttft_start = 0;
			}
			sprintf(rsp_buf, "\r\n#XCELLPOS: %d,%lf,%lf,%d\r\n",
				result.type, result.lat, result.lon, result.unc);
			rsp_send(rsp_buf, strlen(rsp_buf));
			run_type = RUN_TYPE_NONE;
		} else if (err == 1) {
			LOG_WRN("No position found");
		} else if (err == -EFAULT) {
			LOG_ERR("Unable to determine location from cell data, error: %d",
				result.err);
		} else {
			LOG_ERR("Unable to process cell pos data, error: %d", err);
		}
	} else {
		LOG_DBG("Unexpected message received");
	}
}

static void cloud_event_handler(const struct nrf_cloud_evt *evt)
{
	switch (evt->type) {
	case NRF_CLOUD_EVT_TRANSPORT_CONNECTING:
		LOG_DBG("NRF_CLOUD_EVT_TRANSPORT_CONNECTING");
		if (evt->status != NRF_CLOUD_CONNECT_RES_SUCCESS) {
			LOG_ERR("Failed to connect to nRF Cloud, status: %d",
				(enum nrf_cloud_connect_result)evt->status);
		}
		break;
	case NRF_CLOUD_EVT_TRANSPORT_CONNECTED:
		LOG_INF("NRF_CLOUD_EVT_TRANSPORT_CONNECTED");
		break;
	case NRF_CLOUD_EVT_READY:
		LOG_INF("NRF_CLOUD_EVT_READY");
		on_cloud_evt_ready();
		break;
	case NRF_CLOUD_EVT_TRANSPORT_DISCONNECTED:
		LOG_INF("NRF_CLOUD_EVT_TRANSPORT_DISCONNECTED: status %d",
			(enum nrf_cloud_disconnect_status)evt->status);
		on_cloud_evt_disconnected();
		break;
	case NRF_CLOUD_EVT_ERROR:
		LOG_ERR("NRF_CLOUD_EVT_ERROR");
		break;
	case NRF_CLOUD_EVT_SENSOR_DATA_ACK:
		LOG_DBG("NRF_CLOUD_EVT_SENSOR_DATA_ACK");
		break;
	case NRF_CLOUD_EVT_RX_DATA:
		LOG_INF("NRF_CLOUD_EVT_RX_DATA");
		on_cloud_evt_data_received(&evt->data);
		break;
	case NRF_CLOUD_EVT_USER_ASSOCIATION_REQUEST:
		LOG_DBG("NRF_CLOUD_EVT_USER_ASSOCIATION_REQUEST");
		break;
	case NRF_CLOUD_EVT_USER_ASSOCIATED:
		LOG_DBG("NRF_CLOUD_EVT_USER_ASSOCIATED");
		break;
	case NRF_CLOUD_EVT_FOTA_DONE:
		LOG_DBG("NRF_CLOUD_EVT_FOTA_DONE");
		break;
	default:
		break;
	}
}

static void date_time_event_handler(const struct date_time_evt *evt)
{
	switch (evt->type) {
	case DATE_TIME_OBTAINED_MODEM:
	case DATE_TIME_OBTAINED_NTP:
	case DATE_TIME_OBTAINED_EXT:
		LOG_DBG("DATE_TIME OBTAINED");
		k_sem_give(&sem_date_time);
		break;
	case DATE_TIME_NOT_OBTAINED:
		LOG_INF("DATE_TIME_NOT_OBTAINED");
		break;
	default:
		break;
	}
}

static int nrf_cloud_datamode_callback(uint8_t op, const uint8_t *data, int len)
{
	int ret = 0;

	if (op == DATAMODE_SEND) {
		ret = do_cloud_send_msg(data, len);
		LOG_INF("datamode send: %d", ret);
		if (ret < 0) {
			(void)exit_datamode(ret);
		} else {
			(void)exit_datamode(0);
		}
	} else if (op == DATAMODE_EXIT) {
		LOG_DBG("datamode exit");
	}

	return ret;
}

/**@brief handle AT#XGPS commands
 *  AT#XGPS=<op>[,<interval>[,<timeout>]]
 *  AT#XGPS?
 *  AT#XGPS=?
 */
int handle_at_gps(enum at_cmd_type cmd_type)
{
	int err = -EINVAL;
	uint16_t op;
	uint16_t interval;
	uint16_t timeout;

	switch (cmd_type) {
	case AT_CMD_TYPE_SET_COMMAND:
		err = at_params_unsigned_short_get(&at_param_list, 1, &op);
		if (err < 0) {
			return err;
		}
		if (op == GPS_START && run_type == RUN_TYPE_NONE) {
			err = at_params_unsigned_short_get(&at_param_list, 2, &interval);
			if (err < 0) {
				return err;
			}
			/* GNSS API spec check */
			if (interval != 0 && interval != 1 &&
			   (interval < 10 || interval > 65535)) {
				return -EINVAL;
			}
			err = nrf_modem_gnss_fix_interval_set(interval);
			if (err) {
				LOG_ERR("Failed to set fix interval, error: %d", err);
				return err;
			}
			if (at_params_unsigned_short_get(&at_param_list, 3, &timeout) == 0) {
				err = nrf_modem_gnss_fix_retry_set(timeout);
				if (err) {
					LOG_ERR("Failed to set fix retry, error: %d", err);
					return err;
				}
			} /* else leave it to default or previously configured timeout */

			err = nrf_modem_gnss_nmea_mask_set(NRF_MODEM_GNSS_NMEA_GGA_MASK);
			if (err < 0) {
				LOG_ERR("Failed to set nmea mask, error: %d", err);
				return err;
			}
			err = gnss_startup(RUN_TYPE_GPS);
		} else if (op == GPS_STOP && run_type == RUN_TYPE_GPS) {
			err = gnss_shutdown();
		} else {
			err = -EINVAL;
		} break;

	case AT_CMD_TYPE_READ_COMMAND:
		sprintf(rsp_buf, "\r\n#XGPS: %d,%d\r\n", (int)is_gnss_activated(), run_status);
		rsp_send(rsp_buf, strlen(rsp_buf));
		err = 0;
		break;

	case AT_CMD_TYPE_TEST_COMMAND:
		sprintf(rsp_buf, "\r\n#XGPS: (%d,%d),<interval>,<timeout>\r\n",
			GPS_STOP, GPS_START);
		rsp_send(rsp_buf, strlen(rsp_buf));
		err = 0;
		break;

	default:
		break;
	}

	return err;
}

/**@brief handle AT#XNRFCLOUD commands
 *  AT#XNRFCLOUD=<op>[,<signify>]
 *  AT#XNRFCLOUD?
 *  AT#XNRFCLOUD=?
 */
int handle_at_nrf_cloud(enum at_cmd_type cmd_type)
{
	int err = -EINVAL;
	uint16_t op;
	uint16_t signify = 0;

	switch (cmd_type) {
	case AT_CMD_TYPE_SET_COMMAND:
		err = at_params_unsigned_short_get(&at_param_list, 1, &op);
		if (err < 0) {
			return err;
		}
		if (op == nRF_CLOUD_CONNECT && !nrf_cloud_ready) {
			location_signify = 0;
			if (at_params_valid_count_get(&at_param_list) > 2) {
				err = at_params_unsigned_short_get(&at_param_list, 2, &signify);
				if (err < 0) {
					return err;
				}
				location_signify = (signify > 0);
			}
			err = nrf_cloud_connect(NULL);
			if (err) {
				LOG_ERR("Cloud connection failed, error: %d", err);
			} else {
				/* A-GPS & P-GPS needs date_time, trigger to update current time */
				date_time_update_async(date_time_event_handler);
				if (k_sem_take(&sem_date_time, K_SECONDS(10)) != 0) {
					LOG_WRN("Failed to get current time");
				}
			}
		} else if (op == nRF_CLOUD_SEND && nrf_cloud_ready) {
			/* enter data mode */
			err = enter_datamode(nrf_cloud_datamode_callback);
		} else if (op == nRF_CLOUD_DISCONNECT && nrf_cloud_ready) {
			err = nrf_cloud_disconnect();
			if (err) {
				LOG_ERR("Cloud disconnection failed, error: %d", err);
			}
		} else {
			err = -EINVAL;
		} break;

	case AT_CMD_TYPE_READ_COMMAND: {
		sprintf(rsp_buf, "\r\n#XNRFCLOUD: %d,%d,%d,\"%s\"\r\n", nrf_cloud_ready,
			location_signify, CONFIG_NRF_CLOUD_SEC_TAG, device_id);
		rsp_send(rsp_buf, strlen(rsp_buf));
		err = 0;
	} break;

	case AT_CMD_TYPE_TEST_COMMAND:
		sprintf(rsp_buf, "\r\n#XNRFCLOUD: (%d,%d,%d),<signify>\r\n",
			nRF_CLOUD_DISCONNECT, nRF_CLOUD_CONNECT, nRF_CLOUD_SEND);
		rsp_send(rsp_buf, strlen(rsp_buf));
		err = 0;
		break;

	default:
		break;
	}

	return err;
}

/**@brief handle AT#XAGPS commands
 *  AT#XAGPS=<op>[,<interval>[,<timeout>]]
 *  AT#XAGPS?
 *  AT#XAGPS=?
 */
int handle_at_agps(enum at_cmd_type cmd_type)
{
	int err = -EINVAL;
	uint16_t op;
	uint16_t interval;
	uint16_t timeout;

	switch (cmd_type) {
	case AT_CMD_TYPE_SET_COMMAND:
		err = at_params_unsigned_short_get(&at_param_list, 1, &op);
		if (err < 0) {
			return err;
		}
		if (op == AGPS_START && nrf_cloud_ready && run_type == RUN_TYPE_NONE) {
			err = at_params_unsigned_short_get(&at_param_list, 2, &interval);
			if (err < 0) {
				return err;
			}
			/* GNSS API spec check */
			if (interval != 0 && interval != 1 &&
			   (interval < 10 || interval > 65535)) {
				return -EINVAL;
			}
#if defined(CONFIG_NRF_CLOUD_AGPS_FILTERED)
			err = nrf_modem_gnss_elevation_threshold_set(
						CONFIG_NRF_CLOUD_AGPS_ELEVATION_MASK);
			if (err) {
				LOG_ERR("Failed to set elevation threshold, error: %d", err);
				return err;
			}
#endif
			/* no scheduled downloads for peoridical tracking */
			if (interval >= 10) {
				err = nrf_modem_gnss_use_case_set(
						NRF_MODEM_GNSS_USE_CASE_MULTIPLE_HOT_START |
						NRF_MODEM_GNSS_USE_CASE_SCHED_DOWNLOAD_DISABLE);
				if (err) {
					LOG_ERR("Failed to set use case, error: %d", err);
					return err;
				}
			}
			err = nrf_modem_gnss_fix_interval_set(interval);
			if (err) {
				LOG_ERR("Failed to set fix interval, error: %d", err);
				return err;
			}
			if (at_params_unsigned_short_get(&at_param_list, 3, &timeout) == 0) {
				err = nrf_modem_gnss_fix_retry_set(timeout);
				if (err) {
					LOG_ERR("Failed to set fix retry, error: %d", err);
					return err;
				}
			} /* else leave it to default or previously configured timeout */

			err = nrf_modem_gnss_nmea_mask_set(NRF_MODEM_GNSS_NMEA_GGA_MASK);
			if (err < 0) {
				LOG_ERR("Failed to set nmea mask, error: %d", err);
				return err;
			}
			err = gnss_startup(RUN_TYPE_AGPS);
		} else if (op == AGPS_STOP && run_type == RUN_TYPE_AGPS) {
			err = gnss_shutdown();
		} else {
			err = -EINVAL;
		} break;

	case AT_CMD_TYPE_READ_COMMAND:
		sprintf(rsp_buf, "\r\n#XAGPS: %d,%d\r\n", (int)is_gnss_activated(), run_status);
		rsp_send(rsp_buf, strlen(rsp_buf));
		err = 0;
		break;

	case AT_CMD_TYPE_TEST_COMMAND:
		sprintf(rsp_buf, "\r\n#XAGPS: (%d,%d),<interval>,<timeout>\r\n",
			AGPS_STOP, AGPS_START);
		rsp_send(rsp_buf, strlen(rsp_buf));
		err = 0;
		break;

	default:
		break;
	}

	return err;
}

/**@brief handle AT#XPGPS commands
 *  AT#XPGPS=<op>[,<interval>[,<timeout>]]
 *  AT#XPGPS?
 *  AT#XPGPS=?
 */
int handle_at_pgps(enum at_cmd_type cmd_type)
{
	int err = -EINVAL;
	uint16_t op;
	uint16_t interval;
	uint16_t timeout;

	switch (cmd_type) {
	case AT_CMD_TYPE_SET_COMMAND:
		err = at_params_unsigned_short_get(&at_param_list, 1, &op);
		if (err < 0) {
			return err;
		}
		if (op == PGPS_START && nrf_cloud_ready && run_type == RUN_TYPE_NONE) {
			err = at_params_unsigned_short_get(&at_param_list, 2, &interval);
			if (err < 0) {
				return err;
			}
			/* GNSS API spec check, P-GPS is used in periodic mode only */
			if (interval < 10 || interval > 65535) {
				return -EINVAL;
			}
			/* no scheduled downloads for peoridical tracking */
			err = nrf_modem_gnss_use_case_set(
					NRF_MODEM_GNSS_USE_CASE_MULTIPLE_HOT_START |
					NRF_MODEM_GNSS_USE_CASE_SCHED_DOWNLOAD_DISABLE);
			if (err) {
				LOG_ERR("Failed to set use case, error: %d", err);
				return err;
			}
			err = nrf_modem_gnss_fix_interval_set(interval);
			if (err) {
				LOG_ERR("Failed to set fix interval, error: %d", err);
				return err;
			}
			if (at_params_unsigned_short_get(&at_param_list, 3, &timeout) == 0) {
				err = nrf_modem_gnss_fix_retry_set(timeout);
				if (err) {
					LOG_ERR("Failed to set fix retry, error: %d", err);
					return err;
				}
			} /* else leave it to default or previously configured timeout */

			struct nrf_cloud_pgps_init_param param = {
				.event_handler = pgps_event_handler,
				/* storage is defined by CONFIG_NRF_CLOUD_PGPS_STORAGE */
				.storage_base = 0u,
				.storage_size = 0u
			};

			err = nrf_cloud_pgps_init(&param);
			if (err) {
				LOG_ERR("Error from P-GPS init: %d", err);
				return err;
			}

			err = nrf_modem_gnss_nmea_mask_set(NRF_MODEM_GNSS_NMEA_GGA_MASK);
			if (err < 0) {
				LOG_ERR("Failed to set nmea mask, error: %d", err);
				return err;
			}
			err = gnss_startup(RUN_TYPE_PGPS);
		} else if (op == PGPS_STOP && run_type == RUN_TYPE_PGPS) {
			err = gnss_shutdown();
		} else {
			err = -EINVAL;
		} break;

	case AT_CMD_TYPE_READ_COMMAND:
		sprintf(rsp_buf, "\r\n#XPGPS: %d,%d\r\n", (int)is_gnss_activated(), run_status);
		rsp_send(rsp_buf, strlen(rsp_buf));
		err = 0;
		break;

	case AT_CMD_TYPE_TEST_COMMAND:
		sprintf(rsp_buf, "\r\n#XPGPS: (%d,%d),<interval>,<timeout>\r\n",
			PGPS_STOP, PGPS_START);
		rsp_send(rsp_buf, strlen(rsp_buf));
		err = 0;
		break;

	default:
		break;
	}

	return err;
}

/**@brief handle AT#XCELLPOS commands
 *  AT#XCELLPOS=<op>
 *  AT#XCELLPOS? READ command not supported
 *  AT#XCELLPOS=?
 */
int handle_at_cellpos(enum at_cmd_type cmd_type)
{
	int err = -EINVAL;
	uint16_t op;

	switch (cmd_type) {
	case AT_CMD_TYPE_SET_COMMAND:
		err = at_params_unsigned_short_get(&at_param_list, 1, &op);
		if (err < 0) {
			return err;
		}
		if ((op == CELLPOS_START_SCELL || op == CELLPOS_START_MCELL) &&
		    nrf_cloud_ready && run_type == RUN_TYPE_NONE) {
			if (op == CELLPOS_START_SCELL) {
				cell_pos_type = CELL_POS_TYPE_SINGLE;
			} else {
				cell_pos_type = CELL_POS_TYPE_MULTI;
			}
			ttft_start = k_uptime_get();
			k_work_submit_to_queue(&slm_work_q, &cell_pos_req);
			run_type = RUN_TYPE_CELL_POS;
		} else if (op == CELLPOS_STOP) {
			run_type = RUN_TYPE_NONE;
		} else {
			err = -EINVAL;
		} break;

	case AT_CMD_TYPE_READ_COMMAND:
		sprintf(rsp_buf, "\r\n#XCELLPOS: %d,%d\r\n", (int)is_gnss_activated(),
			(run_type == RUN_TYPE_CELL_POS) ? 1 : 0);
		rsp_send(rsp_buf, strlen(rsp_buf));
		err = 0;
		break;

	case AT_CMD_TYPE_TEST_COMMAND:
		sprintf(rsp_buf, "\r\n#XCELLPOS: (%d,%d,%d)\r\n",
			CELLPOS_STOP, CELLPOS_START_SCELL, CELLPOS_START_MCELL);
		rsp_send(rsp_buf, strlen(rsp_buf));
		err = 0;
		break;

	default:
		break;
	}

	return err;
}

/**@brief API to initialize GNSS AT commands handler
 */
int slm_at_gnss_init(void)
{
	int err = 0;
	struct nrf_cloud_init_param init_param = {
		.event_handler = cloud_event_handler
	};

	err = nrf_modem_gnss_event_handler_set(gnss_event_handler);
	if (err) {
		LOG_ERR("Could set GNSS event handler, error: %d", err);
		return err;
	}

	if (!nrf_cloud_initd) {
		err = nrf_cloud_init(&init_param);
		if (err) {
			LOG_ERR("Cloud could not be initialized, error: %d", err);
			return err;
		}

		nrf_cloud_initd = true;
	}

	k_work_init(&agps_req, agps_req_wk);
	k_work_init(&pgps_req, pgps_req_wk);
	k_work_init(&cell_pos_req, cell_pos_req_wk);
	k_work_init(&fix_rep, fix_rep_wk);
	nrf_cloud_client_id_get(device_id, sizeof(device_id));

	return err;
}

/**@brief API to uninitialize GNSS AT commands handler
 */
int slm_at_gnss_uninit(void)
{
	if (nrf_cloud_ready) {
		(void)nrf_cloud_disconnect();
	}
	(void)nrf_cloud_uninit();

	nrf_cloud_initd = false;

	return 0;
}
