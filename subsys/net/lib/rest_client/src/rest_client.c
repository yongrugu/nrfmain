/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#if defined(CONFIG_POSIX_API)
#include <posix/arpa/inet.h>
#include <posix/unistd.h>
#include <posix/netdb.h>
#include <posix/sys/socket.h>
#else
#include <net/socket.h>
#endif
#include <net/tls_credentials.h>
#include <net/http_client.h>
#include <net/http_parser.h>

#include <logging/log.h>

#include <net/rest_client.h>

LOG_MODULE_REGISTER(rest_client, CONFIG_REST_CLIENT_LOG_LEVEL);

#define HTTP_PROTOCOL "HTTP/1.1"

static void rest_client_http_response_cb(struct http_response *rsp,
					  enum http_final_call final_data, 
					  void *user_data)
{
	struct rest_client_req_resp_context *rest_ctx = NULL;

	if (user_data) {
		rest_ctx = (struct rest_client_req_resp_context *)user_data;
	}

	/* If the entire HTTP response is not received in a single "recv" call
	 * then this could be called multiple times, with a different value in
	 * rsp->body_start. Only set rest_ctx->response once, the first time,
	 * which will be the start of the body.
	 */
	if (rest_ctx && !rest_ctx->response && rsp->body_found && rsp->body_start) {
		rest_ctx->response = rsp->body_start;
	}
	rest_ctx->total_response_len += rsp->data_len;

	if (final_data == HTTP_DATA_MORE) {
		LOG_DBG("Partial data received(%zd bytes)", rsp->data_len);
	} else if (final_data == HTTP_DATA_FINAL) {
		if (!rest_ctx) {
			LOG_WRN("REST context not provided");
			return;
		}
		rest_ctx->http_status_code = rsp->http_status_code;
		rest_ctx->response_len = rsp->content_length;

		LOG_DBG("HTTP: All data received (content/total: %d/%d), status: %u %s",
			rsp->content_length,
			rest_ctx->total_response_len,
			rsp->http_status_code,
			log_strdup(rsp->http_status));
	}
}

static int rest_client_sckt_tls_setup(int fd, const char *const tls_hostname,
				      const sec_tag_t sec_tag, int tls_peer_verify)
{
	int err;
	int verify = TLS_PEER_VERIFY_REQUIRED;
	const sec_tag_t tls_sec_tag[] = {
		sec_tag,
	};
	uint8_t cache;

	if (tls_peer_verify == TLS_PEER_VERIFY_REQUIRED ||
	    tls_peer_verify == TLS_PEER_VERIFY_OPTIONAL ||
	    tls_peer_verify == TLS_PEER_VERIFY_NONE) {
		    verify = tls_peer_verify;
	}

	err = setsockopt(fd, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
	if (err) {
		LOG_ERR("Failed to setup peer verification, error: %d", errno);
		return err;
	}

	err = setsockopt(fd, SOL_TLS, TLS_SEC_TAG_LIST, tls_sec_tag, sizeof(tls_sec_tag));
	if (err) {
		LOG_ERR("Failed to setup TLS sec tag, error: %d", errno);
		return err;
	}

	if (IS_ENABLED(CONFIG_REST_CLIENT_SCKT_TLS_SESSION_CACHE_IN_USE)) {
		cache = TLS_SESSION_CACHE_ENABLED;
	} else {
		cache = TLS_SESSION_CACHE_DISABLED;
	}

	err = setsockopt(fd, SOL_TLS, TLS_SESSION_CACHE, &cache, sizeof(cache));
	if (err) {
		LOG_ERR("Unable to set session cache, errno %d", errno);
		return err;
	}

	if (tls_hostname) {
		err = setsockopt(fd, SOL_TLS, TLS_HOSTNAME, tls_hostname, strlen(tls_hostname));
		if (err) {
			LOG_ERR("Failed to setup TLS hostname, error: %d", errno);
			return err;
		}
	}
	return 0;
}

static int rest_client_sckt_timeouts_set(int fd)
{
	int err;
	struct timeval timeout = { 0 };

	if (CONFIG_REST_CLIENT_SCKT_SEND_TIMEOUT > -1) {
		/* Send TO also affects TCP connect */
		timeout.tv_sec = CONFIG_REST_CLIENT_SCKT_SEND_TIMEOUT;
		err = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
		if (err) {
			LOG_ERR("Failed to set socket send timeout, error: %d", errno);
			return err;
		}
	}

	if (CONFIG_REST_CLIENT_SCKT_RECV_TIMEOUT > -1) {
		timeout.tv_sec = CONFIG_REST_CLIENT_SCKT_RECV_TIMEOUT;
		err = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
		if (err) {
			LOG_ERR("Failed to set socket recv timeout, error: %d", errno);
			return err;
		}
	}
	return 0;
}

static int rest_client_sckt_connect(int *const fd, const char *const hostname,
				     const uint16_t port_num,
				     const sec_tag_t sec_tag,
				     int tls_peer_verify)
{
	int ret;
	struct addrinfo *addr_info;
	char peer_addr[INET_ADDRSTRLEN];
	const char *const connect_addr = hostname;

	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_next = NULL,
	};
	int proto = 0;

	/* Make sure fd is always initialized when this function is called */
	*fd = -1;

	LOG_DBG("Doing getaddrinfo() with connect addr %s", log_strdup(connect_addr));

	ret = getaddrinfo(connect_addr, NULL, &hints, &addr_info);
	if (ret) {
		LOG_ERR("getaddrinfo() failed, error: %d", ret);
		return -EFAULT;
	}

	inet_ntop(AF_INET, &net_sin(addr_info->ai_addr)->sin_addr, peer_addr, INET_ADDRSTRLEN);
	LOG_DBG("getaddrinfo() %s", log_strdup(peer_addr));

	((struct sockaddr_in *)addr_info->ai_addr)->sin_port = htons(port_num);

	proto = (sec_tag == REST_CLIENT_NO_SEC) ? IPPROTO_TCP : IPPROTO_TLS_1_2;
	*fd = socket(AF_INET, SOCK_STREAM, proto);
	if (*fd == -1) {
		LOG_ERR("Failed to open socket, error: %d", errno);
		ret = -ENOTCONN;
		goto clean_up;
	}

	if (sec_tag >= 0) {
		ret = rest_client_sckt_tls_setup(*fd, hostname, sec_tag, tls_peer_verify);
		if (ret) {
			ret = -EACCES;
			goto clean_up;
		}
	}

	ret = rest_client_sckt_timeouts_set(*fd);
	if (ret) {
		LOG_ERR("Failed to set socket timeouts, error: %d", errno);
		ret = -EINVAL;
		goto clean_up;
	}

	LOG_DBG("Connecting to %s", log_strdup(connect_addr));

	ret = connect(*fd, addr_info->ai_addr, sizeof(struct sockaddr_in));
	if (ret) {
		LOG_ERR("Failed to connect socket, error: %d", errno);
		ret = -ECONNREFUSED;
		goto clean_up;
	}

	return ret;

clean_up:

	freeaddrinfo(addr_info);
	if (ret) {
		if (*fd > -1) {
			(void)close(*fd);
			*fd = -1;
		}
	}

	return ret;
}

static void rest_client_close_connection(struct rest_client_req_resp_context *const rest_ctx)
{
	int ret;

	if (!rest_ctx->keep_alive) {
		ret = close(rest_ctx->connect_socket);
		if (ret) {
			LOG_WRN("Failed to close socket, error: %d", errno);
		}
		rest_ctx->connect_socket = REST_CLIENT_SCKT_CONNECT;
	} else {
		rest_ctx->used_socket_is_alive = true;
		LOG_INF("Socket with id: %d was kept alive and wasn't closed",
			rest_ctx->connect_socket);
	}
}

static void rest_client_init_request(struct rest_client_req_resp_context *const rest_ctx,
				      struct http_request *const req)
{
	memset(req, 0, sizeof(struct http_request));

	req->host = rest_ctx->host;
	req->protocol = HTTP_PROTOCOL;

	req->response = rest_client_http_response_cb;
	req->method = rest_ctx->http_method;
}

static int rest_client_do_api_call(struct http_request *http_req,
				    struct rest_client_req_resp_context *const rest_ctx)
{
	int err = 0;

	if (rest_ctx->connect_socket < 0) {
		err = rest_client_sckt_connect(&rest_ctx->connect_socket,
						http_req->host,
						rest_ctx->port,
						rest_ctx->sec_tag, rest_ctx->tls_peer_verify);
		if (err) {
			return err;
		}
	}

	/* Assign the user provided receive buffer into the http request */
	http_req->recv_buf = rest_ctx->resp_buff;
	http_req->recv_buf_len = rest_ctx->resp_buff_len;

	memset(http_req->recv_buf, 0, http_req->recv_buf_len);

	/* Ensure receive buffer stays NULL terminated */
	--http_req->recv_buf_len;

	rest_ctx->response = NULL;
	rest_ctx->response_len = 0;
	rest_ctx->total_response_len = 0;
	rest_ctx->used_socket_id = rest_ctx->connect_socket;

	err = http_client_req(rest_ctx->connect_socket, http_req, rest_ctx->timeout_ms, rest_ctx);
	if (err < 0) {
		LOG_ERR("http_client_req() error: %d", err);
	} else if (rest_ctx->total_response_len >= rest_ctx->resp_buff_len) {
		/* 1 byte is reserved to NULL terminate the response */
		LOG_ERR("Receive buffer too small, %d bytes are required",
			rest_ctx->total_response_len + 1);
		err = -ENOBUFS;
	} else {
		err = 0;
	}

	rest_client_close_connection(rest_ctx);
	return err;
}

void rest_client_request_defaults_set(struct rest_client_req_resp_context *req_resp_ctx)
{
	__ASSERT_NO_MSG(req_resp_ctx != NULL);

	req_resp_ctx->connect_socket = REST_CLIENT_SCKT_CONNECT;
	req_resp_ctx->keep_alive = false;
	req_resp_ctx->sec_tag = REST_CLIENT_NO_SEC;
	req_resp_ctx->tls_peer_verify = REST_CLIENT_TLS_DEFAULT_PEER_VERIFY;
	req_resp_ctx->http_method = HTTP_GET;
	req_resp_ctx->timeout_ms = CONFIG_REST_CLIENT_REQUEST_TIMEOUT * 1000;
}

int rest_client_request(struct rest_client_req_resp_context *req_resp_ctx)
{
	__ASSERT_NO_MSG(req_resp_ctx != NULL);
	__ASSERT_NO_MSG(req_resp_ctx->host != NULL);
	__ASSERT_NO_MSG(req_resp_ctx->url != NULL);
	__ASSERT_NO_MSG(req_resp_ctx->resp_buff != NULL);
	__ASSERT_NO_MSG(req_resp_ctx->resp_buff_len > 0);

	struct http_request http_req;
	int ret;

	rest_client_init_request(req_resp_ctx, &http_req);

	http_req.url = req_resp_ctx->url;

	LOG_DBG("Requesting destination HOST: %s at port %d, URL: %s",
		log_strdup(req_resp_ctx->host), req_resp_ctx->port, log_strdup(http_req.url));

	http_req.header_fields = req_resp_ctx->header_fields;

	if (req_resp_ctx->body != NULL) {
		http_req.payload = req_resp_ctx->body;
		http_req.payload_len = strlen(http_req.payload);
		LOG_DBG("Payload: %s", log_strdup(http_req.payload));
	}

	ret = rest_client_do_api_call(&http_req, req_resp_ctx);
	if (ret) {
		LOG_ERR("rest_client_do_api_call() failed, err %d", ret);
		goto clean_up;
	}

	if (!req_resp_ctx->response || !req_resp_ctx->response_len) {
		LOG_WRN("No data in a response body");
		/* Make it as zero length string */
		*req_resp_ctx->resp_buff = '\0';
		req_resp_ctx->response = req_resp_ctx->resp_buff;
		req_resp_ctx->response_len = 0;
	}
	LOG_DBG("API call response len: http status: %d, %u bytes", req_resp_ctx->http_status_code,
		req_resp_ctx->response_len);

clean_up:
	if (req_resp_ctx->connect_socket != REST_CLIENT_SCKT_CONNECT) {
		/* Socket was not closed yet: */
		rest_client_close_connection(req_resp_ctx);
	}
	return ret;
}
