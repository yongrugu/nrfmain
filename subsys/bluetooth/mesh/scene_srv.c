/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <stdio.h>
#include <bluetooth/mesh/models.h>
#include <sys/byteorder.h>
#include "model_utils.h"
#include "mesh/net.h"
#include "mesh/access.h"

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_MESH_DEBUG_MODEL)
#define LOG_MODULE_NAME bt_mesh_scene_srv
#include "common/log.h"

#define SCENE_PAGE_SIZE SETTINGS_MAX_VAL_LEN
/* Account for company ID in data: */
#define VND_MODEL_SCENE_DATA_OVERHEAD sizeof(uint16_t)

struct __packed scene_data {
	uint8_t len;
	uint8_t elem_idx;
	uint16_t id;
	uint8_t data[];
};

static sys_slist_t scene_servers;

static char *scene_path(char *buf, uint16_t scene, bool vnd, uint8_t page)
{
	sprintf(buf, "%x/%c%x", scene, vnd ? 'v' : 's', page);
	return buf;
}

static inline void update_page_count(struct bt_mesh_scene_srv *srv, bool vnd,
			       uint8_t page)
{
	if (vnd) {
		srv->vndpages = MAX(page + 1, srv->vndpages);
	} else {
		srv->sigpages = MAX(page + 1, srv->sigpages);
	}
}

static const struct bt_mesh_scene_entry *
entry_find(const struct bt_mesh_model *mod, bool vnd)
{
	const struct bt_mesh_scene_entry *it;

	if (vnd) {
		extern const struct bt_mesh_scene_entry
			_bt_mesh_scene_entry_vnd_list_start[];
		extern const struct bt_mesh_scene_entry
			_bt_mesh_scene_entry_vnd_list_end[];

		for (it = _bt_mesh_scene_entry_vnd_list_start;
		     it != _bt_mesh_scene_entry_vnd_list_end; it++) {
			if (it->id.vnd.id == mod->vnd.id &&
			    it->id.vnd.company == mod->vnd.company) {
				return it;
			}
		}
	} else {
		extern const struct bt_mesh_scene_entry
			_bt_mesh_scene_entry_sig_list_start[];
		extern const struct bt_mesh_scene_entry
			_bt_mesh_scene_entry_sig_list_end[];

		for (it = _bt_mesh_scene_entry_sig_list_start;
		     it != _bt_mesh_scene_entry_sig_list_end; it++) {
			if (it->id.sig == mod->id) {
				return it;
			}
		}
	}

	return NULL;
}

static struct bt_mesh_scene_srv *srv_find(uint16_t elem_idx)
{
	struct bt_mesh_scene_srv *srv;

	SYS_SLIST_FOR_EACH_CONTAINER(&scene_servers, srv, n) {
		/* Scene servers are added to the link list in reverse
		 * composition data order. The first scene server that isn't
		 * after this element will be the right one:
		 */
		if (srv->model->elem_idx <= elem_idx) {
			return srv;
		}
	}

	return NULL;
}

static uint16_t current_scene(const struct bt_mesh_scene_srv *srv, int64_t now)
{
	if (model_transition_is_active(&srv->transition) &&
	    srv->prev != srv->next && now < srv->transition_end) {
		if (now < srv->transition_end - srv->transition.time) {
			return srv->prev;
		} else {
			return BT_MESH_SCENE_NONE;
		}
	}

	return srv->next;
}

static uint16_t target_scene(const struct bt_mesh_scene_srv *srv, int64_t now)
{
	if (model_transition_is_active(&srv->transition) &&
	    srv->prev != srv->next && now < srv->transition_end) {
		return srv->next;
	}

	return BT_MESH_SCENE_NONE;
}

static void scene_status_encode(struct bt_mesh_scene_srv *srv,
				struct net_buf_simple *buf,
				enum bt_mesh_scene_status status)
{
	int64_t now = k_uptime_get();
	uint16_t current = current_scene(srv, now);
	uint16_t target = target_scene(srv, now);

	bt_mesh_model_msg_init(buf, BT_MESH_SCENE_OP_STATUS);
	net_buf_simple_add_u8(buf, status);
	if (target != BT_MESH_SCENE_NONE) {
		net_buf_simple_add_le16(buf, current);
		net_buf_simple_add_le16(buf, target);
		net_buf_simple_add_u8(buf, model_transition_encode(
						   srv->transition_end - now));
	} else {
		net_buf_simple_add_le16(buf, current);
	}
}

static int scene_status_send(struct bt_mesh_scene_srv *srv,
			     struct bt_mesh_msg_ctx *ctx,
			     enum bt_mesh_scene_status status)
{
	BT_MESH_MODEL_BUF_DEFINE(buf, BT_MESH_SCENE_OP_STATUS,
				 BT_MESH_SCENE_MSG_MAXLEN_STATUS);

	scene_status_encode(srv, &buf, status);

	return model_send(srv->model, ctx, &buf);
}

static void handle_get(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
		       struct net_buf_simple *buf)
{
	struct bt_mesh_scene_srv *srv = model->user_data;

	(void)scene_status_send(srv, ctx, BT_MESH_SCENE_SUCCESS);
}

static void scene_recall(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
			 struct net_buf_simple *buf, bool ack)
{
	struct bt_mesh_scene_srv *srv = model->user_data;
	struct bt_mesh_model_transition transition;
	enum bt_mesh_scene_status status;
	uint16_t scene;
	uint8_t tid;
	int err;

	scene = net_buf_simple_pull_le16(buf);
	tid = net_buf_simple_pull_u8(buf);
	if (buf->len == 2) {
		model_transition_buf_pull(buf, &transition);
	} else if (!buf->len) {
		bt_mesh_dtt_srv_transition_get(model, &transition);
	} else {
		return;
	}

	if (scene == BT_MESH_SCENE_NONE ||
	    model_transition_is_invalid(&transition)) {
		return; /* Prohibited */
	}

	if (tid_check_and_update(&srv->tid, tid, ctx)) {
		BT_DBG("Duplicate TID");
		scene_status_send(srv, ctx, BT_MESH_SCENE_SUCCESS);
		return;
	}

	err = bt_mesh_scene_srv_set(srv, scene, &transition);
	status = ((err == -ENOENT) ? BT_MESH_SCENE_NOT_FOUND :
				     BT_MESH_SCENE_SUCCESS);

	if (ack) {
		scene_status_send(srv, ctx, status);
	}

	if (status == BT_MESH_SCENE_SUCCESS) {
		/* Publish */
		scene_status_send(srv, NULL, status);
	}
}

static void handle_recall(struct bt_mesh_model *model,
			  struct bt_mesh_msg_ctx *ctx,
			  struct net_buf_simple *buf)
{
	scene_recall(model, ctx, buf, true);
}

static void handle_recall_unack(struct bt_mesh_model *model,
				struct bt_mesh_msg_ctx *ctx,
				struct net_buf_simple *buf)
{
	scene_recall(model, ctx, buf, false);
}

static int scene_register_status_send(struct bt_mesh_scene_srv *srv,
				      struct bt_mesh_msg_ctx *ctx,
				      enum bt_mesh_scene_status status)
{
	BT_MESH_MODEL_BUF_DEFINE(buf, BT_MESH_SCENE_OP_REGISTER_STATUS,
				 BT_MESH_SCENE_MSG_MINLEN_REGISTER_STATUS +
					 2 * CONFIG_BT_MESH_SCENES_MAX);
	bt_mesh_model_msg_init(&buf, BT_MESH_SCENE_OP_REGISTER_STATUS);
	net_buf_simple_add_u8(&buf, status);
	net_buf_simple_add_le16(&buf, current_scene(srv, k_uptime_get()));

	for (int i = 0; i < srv->count; i++) {
		net_buf_simple_add_le16(&buf, srv->all[i]);
	}

	return model_send(srv->model, ctx, &buf);
}

static void handle_register_get(struct bt_mesh_model *model,
				struct bt_mesh_msg_ctx *ctx,
				struct net_buf_simple *buf)
{
	struct bt_mesh_scene_srv *srv = model->user_data;

	scene_register_status_send(srv, ctx, BT_MESH_SCENE_SUCCESS);
}

const struct bt_mesh_model_op _bt_mesh_scene_srv_op[] = {
	{
		BT_MESH_SCENE_OP_GET,
		BT_MESH_SCENE_MSG_LEN_GET,
		handle_get,
	},
	{
		BT_MESH_SCENE_OP_RECALL,
		BT_MESH_SCENE_MSG_MINLEN_RECALL,
		handle_recall,
	},
	{
		BT_MESH_SCENE_OP_RECALL_UNACK,
		BT_MESH_SCENE_MSG_MINLEN_RECALL,
		handle_recall_unack,
	},
	{
		BT_MESH_SCENE_OP_REGISTER_GET,
		BT_MESH_SCENE_MSG_LEN_REGISTER_GET,
		handle_register_get,
	},
	BT_MESH_MODEL_OP_END,
};

static uint16_t *scene_find(struct bt_mesh_scene_srv *srv, uint16_t scene)
{
	for (int i = 0; i < srv->count; i++) {
		if (srv->all[i] == scene) {
			return &srv->all[i];
		}
	}

	return NULL;
}

static void entry_recover(struct bt_mesh_scene_srv *srv, bool vnd,
			  const struct scene_data *data)
{
	const struct bt_mesh_elem *elem = &bt_mesh_comp_get()->elem[data->elem_idx];
	const size_t overhead = vnd ? VND_MODEL_SCENE_DATA_OVERHEAD : 0;
	const struct bt_mesh_scene_entry *entry;
	struct bt_mesh_model *mod;

	if (vnd) {
		mod = bt_mesh_model_find_vnd(elem, sys_get_le16(data->data), data->id);
	} else {
		mod = bt_mesh_model_find(elem, data->id);
	}

	if (!mod) {
		BT_WARN("No model @%s", bt_hex(&data->elem_idx, vnd ? 5 : 3));
		return;
	}

	/* MeshMDL1.0.1, section 5.1.3.1.1:
	 * If a model is extending another model, the extending model shall determine
	 * the Stored with Scene behavior of that model.
	 */
	if (bt_mesh_model_is_extended(mod)) {
		return;
	}

	entry = entry_find(mod, vnd);
	if (!entry) {
		BT_WARN("No scene entry for %s", bt_hex(&data->elem_idx, vnd ? 5 : 3));
		return;
	}

	entry->recall(mod, &data->data[overhead], data->len - overhead,
		      &srv->transition);
}

static void page_recover(struct bt_mesh_scene_srv *srv, bool vnd,
			 const uint8_t buf[], size_t len)
{
	for (struct scene_data *data = (struct scene_data *)&buf[0];
	     data < (struct scene_data *)&buf[len];
	     data = (struct scene_data *)&data->data[data->len]) {
		entry_recover(srv, vnd, data);
	}
}

static ssize_t entry_store(struct bt_mesh_model *mod,
			   const struct bt_mesh_scene_entry *entry, bool vnd,
			   uint8_t buf[])
{
	struct scene_data *data = (struct scene_data *)buf;
	ssize_t size;

	data->elem_idx = mod->elem_idx;

	if (vnd) {
		data->id = mod->vnd.id;
		sys_put_le16(mod->vnd.company, data->data);
		size = entry->store(mod,
				    &data->data[VND_MODEL_SCENE_DATA_OVERHEAD]);
		data->len = size + VND_MODEL_SCENE_DATA_OVERHEAD;
	} else {
		data->id = mod->id;
		size = entry->store(mod, &data->data[0]);
		data->len = size;
	}

	if (size > entry->maxlen) {
		BT_ERR("Entry %s:%u:%u: data too large (%u bytes)",
		       vnd ? "vnd" : "sig", mod->elem_idx, mod->mod_idx, size);
		return -EINVAL;
	}

	if (size < 0) {
		BT_WARN("Failed storing %s:%u:%u (%d)", vnd ? "vnd" : "sig",
			mod->elem_idx, mod->mod_idx, size);
		return size;
	}

	if (size == 0) {
		/* Silently ignore this entry */
		return 0;
	}

	return sizeof(struct scene_data) + data->len;
}

/** Store a single page of the Scene.
 *
 *  To accommodate large scene data, each scene is stored in pages of up to 256
 *  bytes.
 */
static void page_store(struct bt_mesh_scene_srv *srv, uint16_t scene,
		       uint8_t page, bool vnd, uint8_t buf[], size_t len)
{
	char path[9];
	int err;

	scene_path(path, scene, vnd, page);
	update_page_count(srv, vnd, page);

	err = bt_mesh_model_data_store(srv->model, false, path, buf, len);
	if (err) {
		BT_ERR("Failed storing %s: %d", log_strdup(path), err);
	}
}

/** @brief Get the end of the Scene server's controlled elements.
 *
 *  A Scene Server controls all elements whose index is equal to or larger than
 *  its own, and smaller than the return value of this function.
 *
 *  @param[in] srv Scene Server to find control boundary of.
 *
 *  @return The element index of the next Scene Server, or the total element
 *          count.
 */
static uint16_t srv_elem_end(const struct bt_mesh_scene_srv *srv)
{
	uint16_t end = bt_mesh_elem_count();
	struct bt_mesh_scene_srv *it;

	/* As Scene Servers are added to the list in reverse order, we'll break
	 * when we find our scene server. When this happens, end will be the
	 * index of the previously checked Scene Server, which is the next Scene
	 * Server in the composition data.
	 */
	SYS_SLIST_FOR_EACH_CONTAINER(&scene_servers, it, n) {
		if (it == srv) {
			break;
		}

		end = srv->model->elem_idx;
	}

	return end;
}

static void scene_store_mod(struct bt_mesh_scene_srv *srv, uint16_t scene,
			    bool vnd)
{
	const size_t data_overhead = sizeof(struct scene_data) + (vnd ? 2 : 0);
	const struct bt_mesh_comp *comp = bt_mesh_comp_get();
	uint16_t elem_end = srv_elem_end(srv);
	uint8_t buf[SCENE_PAGE_SIZE];
	uint8_t page = 0;
	size_t len = 0;

	for (int i = srv->model->elem_idx; i < elem_end; i++) {
		const struct bt_mesh_elem *elem = &comp->elem[i];
		struct bt_mesh_model *models = vnd ? elem->vnd_models : elem->models;
		int model_count = vnd ? elem->vnd_model_count : elem->model_count;

		for (int j = 0; j < model_count; j++) {
			const struct bt_mesh_scene_entry *entry;
			struct bt_mesh_model *mod = &models[j];
			ssize_t size;

			if (mod == srv->model) {
				continue;
			}

			/* MeshMDL1.0.1, section 5.1.3.1.1:
			 * If a model is extending another model, the extending
			 * model shall determine the Stored with Scene behavior
			 * of that model.
			 */
			if (bt_mesh_model_is_extended(mod)) {
				continue;
			}

			entry = entry_find(mod, vnd);
			if (!entry) {
				continue;
			}

			if (len + data_overhead + entry->maxlen >= SCENE_PAGE_SIZE) {
				page_store(srv, scene, page++, vnd, buf, len);
				len = 0;
			}

			size = entry_store(mod, entry, vnd, &buf[len]);
			len += MAX(0, size);
		}
	}

	if (len) {
		page_store(srv, scene, page, vnd, buf, len);
	}
}

static enum bt_mesh_scene_status scene_store(struct bt_mesh_scene_srv *srv,
					     uint16_t scene)
{
	uint16_t *existing = scene_find(srv, scene);

	if (!existing) {
		if (srv->count == ARRAY_SIZE(srv->all)) {
			BT_ERR("Out of space");
			return BT_MESH_SCENE_REGISTER_FULL;
		}

		srv->all[srv->count++] = scene;
	}

	scene_store_mod(srv, scene, false);
	scene_store_mod(srv, scene, true);

	srv->next = scene;
	return BT_MESH_SCENE_SUCCESS;
}

static void scene_delete(struct bt_mesh_scene_srv *srv, uint16_t *scene)
{
	uint8_t path[9];

	BT_DBG("0x%x", *scene);

	for (int i = 0; i < srv->sigpages; i++) {
		scene_path(path, *scene, false, i);
		(void)bt_mesh_model_data_store(srv->model, false, path, NULL, 0);
	}

	for (int i = 0; i < srv->vndpages; i++) {
		scene_path(path, *scene, true, i);
		(void)bt_mesh_model_data_store(srv->model, false, path, NULL, 0);
	}

	int64_t now = k_uptime_get();
	uint16_t target = target_scene(srv, now);
	uint16_t current = current_scene(srv, now);

	if (target == *scene ||
	    (current == *scene && target == BT_MESH_SCENE_NONE)) {
		srv->next = BT_MESH_SCENE_NONE;
		srv->transition_end = 0U;
		srv->prev = BT_MESH_SCENE_NONE;
	} else if (current == *scene && target != *scene) {
		srv->prev = BT_MESH_SCENE_NONE;
	}

	*scene = srv->all[--srv->count];
}

static void handle_store(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
			 struct net_buf_simple *buf)
{
	struct bt_mesh_scene_srv *srv = model->user_data;
	enum bt_mesh_scene_status status;
	uint16_t scene_number;

	scene_number = net_buf_simple_pull_le16(buf);
	if (scene_number == BT_MESH_SCENE_NONE) {
		return;
	}

	status = scene_store(srv, scene_number);
	scene_register_status_send(srv, ctx, status);
}

static void handle_store_unack(struct bt_mesh_model *model,
			       struct bt_mesh_msg_ctx *ctx,
			       struct net_buf_simple *buf)
{
	struct bt_mesh_scene_srv *srv = model->user_data;
	uint16_t scene_number;

	scene_number = net_buf_simple_pull_le16(buf);
	if (scene_number == BT_MESH_SCENE_NONE) {
		return;
	}

	(void)scene_store(srv, scene_number);
}

static void handle_delete(struct bt_mesh_model *model,
			  struct bt_mesh_msg_ctx *ctx,
			  struct net_buf_simple *buf)
{
	struct bt_mesh_scene_srv *srv = model->user_data;
	enum bt_mesh_scene_status status;
	uint16_t *scene;

	scene = scene_find(srv, net_buf_simple_pull_le16(buf));
	if (scene != BT_MESH_SCENE_NONE) {
		scene_delete(srv, scene);
	}

	status = BT_MESH_SCENE_SUCCESS;

	scene_register_status_send(srv, ctx, status);
}

static void handle_delete_unack(struct bt_mesh_model *model,
				struct bt_mesh_msg_ctx *ctx,
				struct net_buf_simple *buf)
{
	struct bt_mesh_scene_srv *srv = model->user_data;
	uint16_t *scene;

	scene = scene_find(srv, net_buf_simple_pull_le16(buf));
	if (scene != BT_MESH_SCENE_NONE) {
		scene_delete(srv, scene);
	}
}

const struct bt_mesh_model_op _bt_mesh_scene_setup_srv_op[] = {
	{
		BT_MESH_SCENE_OP_STORE,
		BT_MESH_SCENE_MSG_LEN_STORE,
		handle_store,
	},
	{
		BT_MESH_SCENE_OP_STORE_UNACK,
		BT_MESH_SCENE_MSG_LEN_STORE,
		handle_store_unack,
	},
	{
		BT_MESH_SCENE_OP_DELETE,
		BT_MESH_SCENE_MSG_LEN_DELETE,
		handle_delete,
	},
	{
		BT_MESH_SCENE_OP_DELETE_UNACK,
		BT_MESH_SCENE_MSG_LEN_DELETE,
		handle_delete_unack,
	},
	BT_MESH_MODEL_OP_END,
};

static int scene_srv_pub_update(struct bt_mesh_model *model)
{
	struct bt_mesh_scene_srv *srv = model->user_data;

	scene_status_encode(srv, &srv->pub_msg, BT_MESH_SCENE_SUCCESS);
	return 0;
}

static int scene_srv_init(struct bt_mesh_model *model)
{
	struct bt_mesh_scene_srv *srv = model->user_data;

	sys_slist_prepend(&scene_servers, &srv->n);

	srv->model = model;
	net_buf_simple_init_with_data(&srv->pub_msg, srv->buf,
				      sizeof(srv->buf));
	srv->pub.msg = &srv->pub_msg;
	srv->pub.update = scene_srv_pub_update;
	return 0;
}

static int scene_srv_set(struct bt_mesh_model *model, const char *path,
			 size_t len_rd, settings_read_cb read_cb, void *cb_arg)
{
	struct bt_mesh_scene_srv *srv = model->user_data;
	uint8_t buf[SCENE_PAGE_SIZE];
	uint16_t scene;
	ssize_t size;
	uint8_t page;
	bool vnd;

	BT_DBG("path: %s", log_strdup(path));

	/* The entire model data tree is loaded in this callback. Depending on
	 * the path and whether we have started the mesh, we'll handle the data
	 * differently:
	 *
	 * - Path "XXXX/vYY": Scene XXXX vendor model page YY
	 * - Path "XXXX/sYY": Scene XXXX sig model page YY
	 */
	scene = strtol(path, NULL, 16);
	if (scene == BT_MESH_SCENE_NONE) {
		BT_ERR("Unknown data %s", log_strdup(path));
		return 0;
	}

	settings_name_next(path, &path);
	if (!path) {
		return 0;
	}

	vnd = path[0] == 'v';
	page = strtol(&path[1], NULL, 16);
	update_page_count(srv, vnd, page);

	/* Before starting the mesh, we'll just register that the scene exists:
	 * Once the mesh starts, we'll load the current scene, and end up in
	 * this callback again, but bt_mesh_is_provisioned() will be true.
	 */
	if (!bt_mesh_is_provisioned()) {
		if (scene_find(srv, scene)) {
			return 0;
		}

		if (srv->count == ARRAY_SIZE(srv->all)) {
			BT_WARN("No room for scene 0x%x", scene);
			return 0;
		}

		BT_DBG("Recovered scene 0x%x", scene);
		srv->all[srv->count++] = scene;
		return 0;
	}

	size = read_cb(cb_arg, &buf, sizeof(buf));
	if (size < 0) {
		BT_ERR("Failed loading scene 0x%x", scene);
		return -EINVAL;
	}

	BT_DBG("0x%x: %s", scene, bt_hex(buf, size));
	page_recover(srv, vnd, buf, size);
	return 0;
}

static void scene_srv_reset(struct bt_mesh_model *model)
{
	struct bt_mesh_scene_srv *srv = model->user_data;

	srv->next = BT_MESH_SCENE_NONE;

	while (srv->count) {
		scene_delete(srv, &srv->all[0]);
	}

	srv->prev = BT_MESH_SCENE_NONE;
	srv->transition_end = 0;
	srv->sigpages = 0;
	srv->vndpages = 0;
}

const struct bt_mesh_model_cb _bt_mesh_scene_srv_cb = {
	.init = scene_srv_init,
	.settings_set = scene_srv_set,
	.reset = scene_srv_reset,
};

static int scene_setup_srv_init(struct bt_mesh_model *model)
{
	struct bt_mesh_scene_srv *srv = model->user_data;

	if (!srv) {
		return -EINVAL;
	}

	srv->setup_mod = model;

	/* Model extensions:
	 * To simplify the model extension tree, we're flipping the
	 * relationship between the scene server and the scene
	 * setup server. In the specification, the scene setup
	 * server extends the scene server, which is the opposite of
	 * what we're doing here. This makes no difference for the mesh
	 * stack, but it makes it a lot easier to extend this model, as
	 * we won't have to support multiple extenders.
	 */
	bt_mesh_model_extend(srv->model, srv->setup_mod);

	return 0;
}

const struct bt_mesh_model_cb _bt_mesh_scene_setup_srv_cb = {
	.init = scene_setup_srv_init,
};

void bt_mesh_scene_invalidate(struct bt_mesh_model *mod)
{
	struct bt_mesh_scene_srv *srv = srv_find(mod->elem_idx);

	if (!srv) {
		return;
	}

	srv->prev = BT_MESH_SCENE_NONE;
	srv->transition_end = 0U;
	srv->next = BT_MESH_SCENE_NONE;
}

int bt_mesh_scene_srv_set(struct bt_mesh_scene_srv *srv, uint16_t scene,
			  struct bt_mesh_model_transition *transition)
{
	char path[25];

	if (scene == BT_MESH_SCENE_NONE ||
	    model_transition_is_invalid(transition)) {
		return -EINVAL;
	}

	if (!scene_find(srv, scene)) {
		BT_WARN("Unknown scene 0x%x", scene);
		return -ENOENT;
	}

	srv->prev = current_scene(srv, k_uptime_get());
	if (transition && model_transition_is_active(transition)) {
		srv->transition_end =
			k_uptime_get() + transition->delay + transition->time;

		srv->transition = *transition;
	} else {
		srv->transition_end = 0U;
		srv->transition.delay = 0U;
		srv->transition.time = 0U;
	}

	srv->next = scene;

	sprintf(path, "bt/mesh/s/%x/data/%x",
		(srv->model->elem_idx << 8) | srv->model->mod_idx, scene);

	BT_DBG("Loading %s", log_strdup(path));

	return settings_load_subtree(path);
}

int bt_mesh_scene_srv_pub(struct bt_mesh_scene_srv *srv,
			  struct bt_mesh_msg_ctx *ctx)
{
	return scene_status_send(srv, ctx, BT_MESH_SCENE_SUCCESS);
}

uint16_t
bt_mesh_scene_srv_current_scene_get(const struct bt_mesh_scene_srv *srv)
{
	return current_scene(srv, k_uptime_get());
}

uint16_t bt_mesh_scene_srv_target_scene_get(const struct bt_mesh_scene_srv *srv)
{
	return target_scene(srv, k_uptime_get());
}
