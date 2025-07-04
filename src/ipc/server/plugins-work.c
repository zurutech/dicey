/*
 * Copyright (c) 2024-2025 Zuru Tech HK Limited, All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _CRT_NONSTDC_NO_DEPRECATE 1
#define _XOPEN_SOURCE 700

#include "dicey_config.h"

#if DICEY_HAS_PLUGINS

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <uv.h>

#include <dicey/core/builders.h>
#include <dicey/core/errors.h>
#include <dicey/core/packet.h>
#include <dicey/core/type.h>
#include <dicey/core/value.h>
#include <dicey/core/views.h>
#include <dicey/ipc/plugins.h>
#include <dicey/ipc/server-api.h>
#include <dicey/ipc/server.h>

#include "sup/trace.h"
#include "sup/util.h"
#include "sup/uvtools.h"

#include "builtins/plugins/plugins.h"

#include "ipc/plugin-common.h"

#include "plugins-internal.h"
#include "registry-internal.h"
#include "server-internal.h"
#include "server-loopreq.h"

#define PLUGIN_CMD_SEL                                                                                                 \
    (struct dicey_selector) { .trait = DICEY_PLUGIN_TRAIT_NAME, .elem = PLUGIN_COMMAND_SIGNAL_NAME }

struct plugin_send_work_data {
    char *name;                                      // the name of the plugin
    struct dicey_server_plugin_work_builder builder; // a yet to complete work request builder
    dicey_server_plugin_on_work_done_fn *on_done;    // the callback to call when the work is done
    void *ctx;
};

struct plugin_quit_metadata {
    uv_sem_t *quit_sem;
    enum dicey_error *quit_err;
    int64_t *quit_status;
};

struct plugin_quit_request {
    struct plugin_quit_metadata md;

    char target[];
};

// the builders use the value pointers to check for borrowing. This means that this has to go into the heap
struct plugin_work_builder_state {
    struct dicey_server *owner;
    struct dicey_message_builder builder;
    struct dicey_value_builder tuple_builder;

    char name[];
};

const struct dicey_arg TUPLE_ARGS[] = {
    [0] = {
        .type = DICEY_TYPE_UNIT,
    },
    [1] = {
        .type = DICEY_TYPE_UINT64,
        .u64 = UINT64_MAX, // arbitrarily large value. Symbolically signifies this is the last ever command
    },
    [2] = {
        .type = DICEY_TYPE_BYTE,
        .byte = PLUGIN_COMMAND_HALT,
    },
};

const struct dicey_arg PLUGIN_CMD_ARG = {
    .type = DICEY_TYPE_TUPLE,
    .tuple = {
        .nitems = DICEY_LENOF(TUPLE_ARGS),
        .elems = TUPLE_ARGS,
    },
};

static enum dicey_error craft_quit_packet(
    struct dicey_packet *const dest,
    const char *const target,
    struct dicey_view_mut *const buffer
) {
    assert(dest && target && buffer);

    // craft the metaplugin path
    const char *const path = dicey_metaname_format_to(buffer, DICEY_METAPLUGIN_FORMAT, target);
    if (!path) {
        return TRACE(DICEY_ENOMEM);
    }

    struct dicey_message_builder builder = { 0 };

    enum dicey_error err = dicey_message_builder_init(&builder);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_begin(&builder, DICEY_OP_SIGNAL);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_path(&builder, path);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_selector(&builder, PLUGIN_CMD_SEL);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_value(&builder, PLUGIN_CMD_ARG);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_build(&builder, dest);
    if (err) {
        goto fail;
    }

    return DICEY_OK;

fail:
    dicey_message_builder_discard(&builder);

    return err;
}

static enum dicey_error craft_quit_request_in(
    struct dicey_view_mut view,
    const struct plugin_quit_metadata md,
    const char *const target
) {
    assert(dicey_view_mut_is_valid(view) && target);

    // write the metadata first
    ptrdiff_t result = dicey_view_mut_write_ptr(&view, &md, sizeof md);
    if (result < 0) {
        return (enum dicey_error) result;
    }

    assert(result == sizeof md);

    // write the name of the target
    result = dicey_view_mut_write_zstring(&view, target);
    if (result < 0) {
        return (enum dicey_error) result;
    }

    assert(result == dutl_zstring_size(target));

    return DICEY_OK;
}

static enum dicey_error plugin_work_request_complete(
    struct dicey_server *const server,
    struct dicey_server_plugin_work_builder *const wb,
    const uint64_t jid,
    struct dicey_packet *const dest
) {
    assert(server && wb && dest);

    struct plugin_work_builder_state *const wbs = wb->_state;
    assert(wbs);

    struct dicey_value_builder *const tuple = &wbs->tuple_builder;

    struct dicey_value_builder argument_builder = { 0 };
    enum dicey_error err = dicey_value_builder_next(tuple, &argument_builder);
    if (err) {
        return err;
    }

    // increase the JID later, on success
    err = dicey_value_builder_set(
        &argument_builder,
        (struct dicey_arg) {
            .type = DICEY_TYPE_UINT64,
            .u64 = jid,
        }
    );

    if (err) {
        return err;
    }

    err = dicey_value_builder_next(tuple, &argument_builder);
    if (err) {
        return err;
    }

    err = dicey_value_builder_set(
        &argument_builder,
        (struct dicey_arg) {
            .type = DICEY_TYPE_BYTE,
            .byte = PLUGIN_COMMAND_DO_WORK,
        }
    );

    if (err) {
        return err;
    }

    err = dicey_value_builder_tuple_end(tuple);
    if (err) {
        return err;
    }

    struct dicey_message_builder *const builder = &wbs->builder;

    err = dicey_message_builder_value_end(builder, tuple);
    if (err) {
        return err;
    }

    // because this runs in the loop, it's safe to use the scratchpad
    const char *const path = dicey_metaname_format_to(&server->scratchpad, DICEY_METAPLUGIN_FORMAT, wbs->name);
    if (!path) {
        return TRACE(DICEY_ENOMEM);
    }

    err = dicey_message_builder_set_path(builder, path);
    if (err) {
        return err;
    }

    err = dicey_message_builder_set_selector(builder, PLUGIN_CMD_SEL);
    if (err) {
        return err;
    }

    // it's fine to just return errors in this function because the builder is already managed as part of the work
    // builder
    return dicey_message_builder_build(builder, dest);
}

static void plugin_work_request_sync_cb(
    const uint64_t *const jid,
    const enum dicey_error err,
    const struct dicey_owning_value *const result,
    void *const ctx
) {
    struct dicey_work_request_sync_data *const data = ctx;
    assert(data && data->result && data->sem);

    if (jid && result) {
        if (!err) {
            *data->result = *result;
        }

        data->err = err;
    } else {
        // the plugin failed to respond (note: this code may be unreachable)
        data->err = TRACE(DICEY_ETIMEDOUT);
    }

    uv_sem_post(data->sem);
}

static void plugin_work_request_finish(
    struct plugin_work_request *const req,
    const enum dicey_error err,
    const struct dicey_owning_value *const value
) {
    assert(req && req->on_done);

    req->on_done(&req->jid, err, value, req->ctx);
}

static void plugin_work_request_fail(struct plugin_work_request *const req, const enum dicey_error err) {
    plugin_work_request_finish(req, err, NULL);
}

static void plugin_send_work_data_fail(struct plugin_send_work_data *const work, const enum dicey_error err) {
    if (work) {
        assert(work->on_done);

        work->on_done(NULL, err, NULL, work->ctx);

        dicey_server_plugin_work_builder_discard(&work->builder);
        free(work->name);
    }
}

static enum dicey_error plugin_quit_retrieve_request(
    struct plugin_quit_request *const data,
    struct plugin_quit_metadata *const md,
    char **const target
) {
    assert(data && md && target);

    // we want a view only over the metadata, because that's the only thing we need to read
    struct dicey_view view = dicey_view_from(data, sizeof *md);

    ptrdiff_t result = dicey_view_read_ptr(&view, md, sizeof *md);
    if (result < 0) {
        return (enum dicey_error) result;
    }

    assert(result == sizeof *md);

    // the target is a null terminated string, starting after the metadata.
    // this is not UB because the data is a char array, so we can safely read from it directly.
    // also, the payload is not mutable to begin with, so we can cast away the const
    *target = (char *) view.data;

    return DICEY_OK;
}

static enum dicey_error plugin_issue_quit(
    struct dicey_server *const server,
    struct dicey_client_data *const client,
    void *const req_data
) {
    assert(server && req_data);

    if (client) {
        return TRACE(DICEY_EACCES); // clients can't send ask plugins to quit
    }

    struct plugin_quit_metadata md = { 0 };
    char *target = NULL;

    enum dicey_error err = plugin_quit_retrieve_request(req_data, &md, &target);
    if (err) {
        // very bad, should never happen
        return err;
    }

    struct dicey_plugin_data *const plugin = dicey_server_plugin_find_by_name(server, target);
    if (!plugin) {
        err = TRACE(DICEY_EPEER_NOT_FOUND);

        goto fail;
    }

    if (plugin->state != PLUGIN_STATE_RUNNING) {
        err = TRACE(DICEY_EINVAL);

        goto fail;
    }

    // reuse the spawn metadata for the quit request
    plugin->spawn_md = (struct plugin_spawn_metadata) {
        .error = md.quit_err,
        .wait_sem = md.quit_sem,
        .retval = md.quit_status,
    };

    // send quitting message
    struct dicey_packet request = { 0 };
    err = craft_quit_packet(&request, target, &server->scratchpad);
    if (err) {
        goto fail;
    }

    err = dicey_server_raise_internal(server, request);
    if (err) {
        goto fail;
    }

    // mark the plugin as "quitting" and wait for it to finish, or kill it if it takes too long

    err = dicey_server_plugin_quitting(server, plugin);
    if (err) {
        goto fail;
    }

    return err;

fail:
    if (md.quit_sem) {
        uv_sem_post(md.quit_sem);
    }

    return err;
}

static enum dicey_error plugin_issue_work(
    struct dicey_server *const server,
    struct dicey_client_data *const client,
    void *const req_data
) {
    assert(server && req_data);

    if (client) {
        return TRACE(DICEY_EACCES); // clients can't send work to plugins
    }

    struct plugin_send_work_data req = { 0 };
    memcpy(&req, req_data, sizeof req);

    assert(req.name && req.on_done);

    struct dicey_packet packet = { 0 };
    enum dicey_error err = DICEY_OK;

    struct dicey_plugin_data *const target = dicey_server_plugin_find_by_name(server, req.name);
    if (!target) {
        err = TRACE(DICEY_ENOENT);

        goto fail;
    }

    err = plugin_work_request_complete(server, &req.builder, target->next_jid, &packet);
    if (err) {
        goto fail;
    }

    // get rid of the builder now that the packet has been crafted
    dicey_server_plugin_work_builder_discard(&req.builder);

    const struct plugin_work_request *const entry = plugin_work_list_append(
        &target->work_list,
        &(struct plugin_work_request) {
            .jid = target->next_jid,
            .on_done = req.on_done,
            .ctx = req.ctx,
        }
    );

    if (!entry) {
        err = TRACE(DICEY_ENOMEM);

        goto fail;
    }

    err = dicey_server_raise_internal(server, packet);
    if (err) {
        goto clean_prefail;
    }

    // only in case of success, increase the jid
    ++target->next_jid;

    return DICEY_OK;

clean_prefail:
    plugin_work_list_erase(target->work_list, entry);

fail:
    dicey_packet_deinit(&packet);
    plugin_send_work_data_fail(&req, err);

    return TRACE(err);
}

static enum dicey_error plugin_request_quit(
    struct dicey_server *const server,
    const char *const target,
    struct plugin_quit_metadata *const extra_md
) {
    assert(server && target);

    const size_t payload_size = sizeof(struct plugin_quit_request) + dutl_zstring_size(target);

    struct dicey_server_loop_request *const req = DICEY_SERVER_LOOP_REQ_NEW_WITH_BYTES(payload_size);
    if (!req) {
        return TRACE(DICEY_ENOMEM);
    }

    *req = (struct dicey_server_loop_request) {
        .cb = &plugin_issue_quit,
        .target = DICEY_SERVER_LOOP_REQ_NO_TARGET,
    };

    enum dicey_error err = craft_quit_request_in(
        DICEY_SERVER_LOOP_REQ_GET_PAYLOAD_AS_VIEW_MUT(*req, payload_size),
        extra_md ? *extra_md : (struct plugin_quit_metadata) { 0 },
        target
    );
    if (err) {
        free(req);

        return err;
    }

    err = dicey_server_submit_request(server, req);
    if (err) {
        free(req);
    }

    return err;
}

static enum dicey_error plugin_submit_work(
    struct dicey_server *const server,
    const struct plugin_send_work_data *const work_data
) {
    assert(server && work_data);

    struct dicey_server_loop_request *const req = DICEY_SERVER_LOOP_REQ_NEW(struct plugin_send_work_data);
    if (!req) {
        return TRACE(DICEY_ENOMEM);
    }

    *req = (struct dicey_server_loop_request) {
        .cb = &plugin_issue_work,
        .target = DICEY_SERVER_LOOP_REQ_NO_TARGET,
    };

    DICEY_SERVER_LOOP_SET_PAYLOAD(req, struct plugin_send_work_data, work_data);

    const enum dicey_error err = dicey_server_submit_request(server, req);
    if (err) {
        free(req);
    }

    return err;
}

static bool plugin_work_list_pop_job(
    struct plugin_work_list *const list,
    const uint64_t jid,
    struct plugin_work_request *const dest
) {
    assert(list && dest);

    for (const struct plugin_work_request *it = plugin_work_list_cbegin(list), *const end = plugin_work_list_cend(list);
         it != end;
         ++it) {
        if (it->jid == jid) {
            *dest = plugin_work_list_erase(list, it);

            return true;
        }
    }

    return false;
}

enum dicey_error dicey_server_plugin_quit(struct dicey_server *const server, const char *const name) {
    assert(server && name);

    // no callbacks, no nothing, just ask the plugin to quit and an event will appear on the global plugin callback
    return plugin_request_quit(server, name, NULL);
}

enum dicey_error dicey_server_plugin_quit_and_wait(
    struct dicey_server *const server,
    const char *const name,
    int64_t *const retval
) {
    assert(server && name);

    uv_sem_t quit_sem = { 0 };
    const int uverr = uv_sem_init(&quit_sem, 0);

    if (uverr) {
        return dicey_error_from_uv(uverr);
    }

    enum dicey_error quit_err = DICEY_OK;

    const enum dicey_error err = plugin_request_quit(
        server,
        name,
        &(struct plugin_quit_metadata) {
            .quit_sem = &quit_sem,
            .quit_err = &quit_err,
            .quit_status = retval,
        }
    );

    if (err) {
        uv_sem_destroy(&quit_sem);

        return err;
    }

    uv_sem_wait(&quit_sem);
    uv_sem_destroy(&quit_sem);

    return quit_err;
}

enum dicey_error dicey_server_plugin_report_work_done(
    struct dicey_server *const server,
    struct dicey_plugin_data *const plugin,
    const uint64_t jid,
    const struct dicey_owning_value *const value
) {
    assert(server && plugin && value);
    DICEY_UNUSED(server);

    // can never be too sure
    assert(plugin->state == PLUGIN_STATE_RUNNING);

    struct plugin_work_request work = { 0 };
    if (!plugin_work_list_pop_job(plugin->work_list, jid, &work)) {
        return TRACE(DICEY_ENOENT);
    }

    plugin_work_request_finish(&work, DICEY_OK, value);

    return DICEY_OK;
}

enum dicey_error dicey_server_plugin_send_work(
    struct dicey_server *const server,
    const char *const plugin,
    const struct dicey_arg payload,
    dicey_server_plugin_on_work_done_fn *const on_done,
    void *const ctx
) {
    assert(server && plugin && on_done && dicey_type_is_valid(payload.type));

    struct dicey_server_plugin_work_builder builder = { 0 };
    struct dicey_value_builder value = { 0 };

    enum dicey_error err = dicey_server_plugin_work_request_start(server, plugin, &builder, &value);
    if (err) {
        return err;
    }

    err = dicey_value_builder_set(&value, payload);
    if (err) {
        dicey_server_plugin_work_builder_discard(&builder);

        return err;
    }

    return dicey_server_plugin_work_request_submit(server, &builder, on_done, ctx);
}

// instead of further complicating the loop, this just reuses the async function like it's done in the client
enum dicey_error dicey_server_plugin_send_work_and_wait(
    struct dicey_server *const server,
    const char *const plugin,
    const struct dicey_arg payload,
    struct dicey_owning_value *const response
) {
    assert(server && plugin && response);

    uv_sem_t sync_sem = { 0 };

    enum dicey_error err = dicey_error_from_uv(uv_sem_init(&sync_sem, 0));
    if (err) {
        return err;
    }

    struct dicey_work_request_sync_data sync_data = {
        .sem = &sync_sem,
        .result = response,
    };

    err = dicey_server_plugin_send_work(server, plugin, payload, &plugin_work_request_sync_cb, &sync_data);

    if (err) {
        uv_sem_destroy(&sync_sem);

        return err;
    }

    uv_sem_wait(&sync_sem);
    uv_sem_destroy(&sync_sem);

    return sync_data.err;
}

void dicey_server_plugin_work_builder_discard(struct dicey_server_plugin_work_builder *const builder) {
    if (builder && builder->_state) {
        struct plugin_work_builder_state *const state = builder->_state;

        dicey_message_builder_discard(&state->builder);
        free(state);

        *builder = (struct dicey_server_plugin_work_builder) { 0 };
    }
}

enum dicey_error dicey_server_plugin_work_request_start(
    struct dicey_server *const server,
    const char *const plugin,
    struct dicey_server_plugin_work_builder *const wb,
    struct dicey_value_builder *const value
) {
    assert(server && plugin && wb && value);

    const size_t plugin_size = dutl_zstring_size(plugin);

    struct plugin_work_builder_state *const state = malloc(sizeof *state + plugin_size);
    if (!state) {
        return TRACE(DICEY_ENOMEM);
    }

    *state = (struct plugin_work_builder_state) {
        .owner = server,
    };

    memcpy(state->name, plugin, plugin_size);

    wb->_state = state;

    struct dicey_message_builder *const builder = &state->builder;

    enum dicey_error err = dicey_message_builder_init(builder);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_begin(builder, DICEY_OP_SIGNAL);
    if (err) {
        goto fail;
    }

    struct dicey_value_builder *const tuple_builder = &state->tuple_builder;

    err = dicey_message_builder_value_start(builder, tuple_builder);
    if (err) {
        goto fail;
    }

    err = dicey_value_builder_tuple_start(tuple_builder);
    if (err) {
        goto fail;
    }

    // here we return the value builder of slot #1 to the caller, which will be used to set the payload
    // we'll set the reminder later
    err = dicey_value_builder_next(tuple_builder, value);
    if (err) {
        goto fail;
    }

    return DICEY_OK;

fail:
    dicey_server_plugin_work_builder_discard(wb);

    return err;
}

void dicey_server_plugin_work_request_cancel(struct plugin_work_request *const elem) {
    plugin_work_request_fail(elem, DICEY_ECANCELLED);
}

enum dicey_error dicey_server_plugin_work_request_submit(
    struct dicey_server *const server,
    struct dicey_server_plugin_work_builder *const builder,
    dicey_server_plugin_on_work_done_fn *const on_done,
    void *const ctx
) {
    assert(server && builder && on_done);

    struct plugin_work_builder_state *const state = builder->_state;
    assert(state);

    const struct plugin_send_work_data work_data = {
        .name = state->name,
        .builder = *builder,
        .on_done = on_done,
        .ctx = ctx,
    };

    // the caller doesn't need to access this builder anymore, it's owned by the loop now
    *builder = (struct dicey_server_plugin_work_builder) { 0 };

    return plugin_submit_work(server, &work_data);
}

#else

#error "This file should not be built if plugins are disabled"

#endif // DICEY_HAS_PLUGINS
