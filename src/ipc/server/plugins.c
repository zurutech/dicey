/*
 * Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
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
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>

#include <dicey/ipc/plugins.h>
#include <dicey/ipc/server.h>

#include "sup/trace.h"
#include "sup/util.h"
#include "sup/uvtools.h"
#include "sup/view-ops.h"

#include "ipc/plugin-macros.h"

#include "client-data.h"
#include "plugins.h"
#include "server-clients.h"
#include "server-loopreq.h"
#include "server.h"

#include "dicey_config.h"

// note: the double cast is necessary to avoid a warning with MSVC
#define CLIENT_DATA_IS_PLUGIN(CLIENTPTR) ((CLIENTPTR)->is_plugin)

// this is the time the plugin has to start up and handshake with the server before we consider it dead (one second)
#define PLUGIN_TIMEOUT_MS 1000U

#if defined(DICEY_IS_UNIX)
// on Unix, we have SIGKILL available in signal.h, which immediately terminates the process

#define KILL_SIGNAL SIGKILL

#else
// fallback on SIGTERM, which is mandated by the C standard
// on Windows you can't really trigger a SIGTERM, so we rely on libuv to issue a TerminateProcess instead

#define KILL_SIGNAL SIGTERM
#endif

struct dicey_plugin_data {
    struct dicey_client_data client; // must remain the first field for the cast to work

    uv_process_t process;

    // timer used for both the handshake timeout and process killing
    uv_timer_t process_timer;

    enum dicey_plugin_state state;
    struct dicey_plugin_info info;

    // semaphore used by dicey_server_spawn_plugin_and_wait to wait for the plugin to spawn and fully handshake
    uv_sem_t *spawn_wait_sem;
};

// the fixed, non variable-sized bits of the request
struct plugin_spawn_metadata {
    uv_sem_t *spawn_wait_sem;       // optional, only for blocking requests
    enum dicey_error *spawn_result; // optional, used to report the result of the spawn if it's a blocking request
};

struct plugin_spawn_request {
    struct plugin_spawn_metadata md;

    char path[];
};

struct dicey_plugin_data *as_plugin(struct dicey_client_data *const client) {
    return client && CLIENT_DATA_IS_PLUGIN(client) ? (struct dicey_plugin_data *) client : NULL;
}

static enum dicey_error plugin_data_cleanup(
    struct dicey_client_data *const client,
    dicey_client_data_after_cleanup_fn *const after_cleanup
) {
    if (client) {
        const struct dicey_plugin_data *const data = as_plugin(client);
        assert(data);

        // the strings were either NULL or strdup'd in dicey_plugin_data_set, so we can cast away the const safely

        free((char *) data->info.name);
        free((char *) data->info.path);

        // if the plugin was ever spawned, we need to close the process and the timer
        if (data->state != PLUGIN_STATE_INVALID) {
            uv_close((uv_handle_t *) &data->process, NULL);
            uv_close((uv_handle_t *) &data->process_timer, NULL);
        }
    }

    // continue the cleanup if there's an after_cleanup
    return after_cleanup ? after_cleanup(client) : DICEY_OK;
}

static enum dicey_error client_data_new_plugin(
    struct dicey_server *const server,
    struct dicey_plugin_data **const out_plugin,
    size_t *const out_id
) {
    assert(server && out_plugin && out_id);

    // the bucket in the server client array. Id is the index in the array
    struct dicey_client_data **client_bucket = NULL;
    size_t id = 0U;

    enum dicey_error err = dicey_server_reserve_id(server, &client_bucket, &id);
    if (err) {
        return err;
    }

    struct dicey_plugin_data *const new_plugin = calloc(1, sizeof(struct dicey_plugin_data));
    if (!new_plugin) {
        return err;
    }

    *client_bucket = dicey_client_data_init(&new_plugin->client, server, id);
    if (!*client_bucket) {
        free(new_plugin);

        return TRACE(DICEY_ENOMEM);
    }

    // set the cleanup callback
    new_plugin->client.cleanup_cb = &plugin_data_cleanup;

    *out_plugin = new_plugin;
    *out_id = id;

    return DICEY_OK;
}

static enum dicey_error craft_plugin_spawn_request_in(
    struct dicey_view_mut view,
    const struct plugin_spawn_metadata md,
    const char *const path
) {
    assert(dicey_view_mut_is_valid(view) && path);

    // write the metadata first
    ptrdiff_t result = dicey_view_mut_write_ptr(&view, &md, sizeof md);
    if (result < 0) {
        return (enum dicey_error) result;
    }

    assert(result == sizeof md);

    // write the path
    result = dicey_view_mut_write_zstring(&view, path);
    if (result < 0) {
        return (enum dicey_error) result;
    }
    assert(result == dutl_zstring_size(path));

    return DICEY_OK;
}

static size_t count_plugins(const struct dicey_client_list *const list) {
    assert(list);

    size_t count = 0U;

    struct dicey_client_data *const *it = dicey_client_list_begin(list), *const *const end =
                                                                             dicey_client_list_end(list);

    for (; it != end; ++it) {
        const struct dicey_client_data *const client = *it;
        if (client && CLIENT_DATA_IS_PLUGIN(client)) {
            ++count;
        }
    }

    return count;
}

static enum dicey_error info_dup_to(struct dicey_plugin_info src, struct dicey_plugin_info *const dst) {
    assert(dst);

    char *const name = strdup(src.name);
    if (!name) {
        return TRACE(DICEY_ENOMEM);
    }

    char *const path = strdup(src.path);
    if (!path) {
        free(name);

        return TRACE(DICEY_ENOMEM);
    }

    *dst = (struct dicey_plugin_info) {
        .name = name,
        .path = path,
    };

    return DICEY_OK;
}

static void plugin_raise_event(struct dicey_plugin_data *const plugin, const enum dicey_plugin_event_kind kind) {
    assert(plugin);

    struct dicey_server *const server = plugin->client.parent;
    assert(server);

    if (server->on_plugin_event) {
        server->on_plugin_event(
            server,
            (struct dicey_plugin_event) {
                .info = dicey_plugin_data_get_info(plugin),
                .kind = kind,
            }
        );
    }
}

static void plugin_change_state(struct dicey_plugin_data *const plugin, const enum dicey_plugin_state new_state) {
    assert(plugin);

    plugin->state = new_state;

    switch (new_state) {
    case PLUGIN_STATE_SPAWNED:
        plugin_raise_event(plugin, DICEY_PLUGIN_EVENT_SPAWNED);
        break;

    case PLUGIN_STATE_RUNNING:
        plugin_raise_event(plugin, DICEY_PLUGIN_EVENT_READY);
        break;

    case PLUGIN_STATE_QUITTING:
        plugin_raise_event(plugin, DICEY_PLUGIN_EVENT_QUITTING);
        break;

    case PLUGIN_STATE_FAILED:
        // in case we failed we issue a failed event and a quit event in sequence
        plugin_raise_event(plugin, DICEY_PLUGIN_EVENT_FAILED);

        DICEY_FALLTHROUGH;

    case PLUGIN_STATE_COMPLETE:
        plugin_raise_event(plugin, DICEY_PLUGIN_EVENT_QUIT);
        break;

    default:
        DICEY_UNREACHABLE();

        break;
    }
}

static void kill_unruly_child(struct dicey_plugin_data *const plugin) {
    // Our child misbehaved by failing to complete the handshake in time, either because it's too slow or because it's
    // not a plugin at all.
    // We can then kill it immediately, as prescribed by the plugin API:
    assert(plugin && uv_is_active((uv_handle_t *) &plugin->process));

    const int uverr = uv_process_kill(&plugin->process, KILL_SIGNAL);
    if (uverr < 0) {
        // we can't really do anything about it
        DICEY_UNREACHABLE();
    }
}

static void plugin_exit_cb(uv_process_t *const proc, const int64_t exit_status, const int term_signal) {
    assert(proc);

    // we know that this uv_process_t is from a dicey_plugin_data, so we can safely cast it using the containerof macro
    struct dicey_plugin_data *const plugin = DICEY_CONTAINEROF(proc, struct dicey_plugin_data, process);
    assert(plugin); // useless check, but it's here for clarity

    const bool failed = exit_status != EXIT_SUCCESS || term_signal;

    plugin_change_state(plugin, failed ? PLUGIN_STATE_FAILED : PLUGIN_STATE_COMPLETE);

    struct dicey_server *const server = plugin->client.parent;
    assert(server);
}

static void plugin_no_handshake_timeout(uv_timer_t *const timer) {
    assert(timer);

    struct dicey_plugin_data *const plugin = timer->data;
    assert(plugin);

    // if the plugin is still in the handshake state, we kill it
    if (plugin->state == PLUGIN_STATE_SPAWNED) {
        // the plugin has not yet completed the handshake, so we kill it
        kill_unruly_child(plugin);

        // no need stop the timer, it's a one-shot timer. It will be cleaned up when the plugin is cleaned up
    } else {
        // this should not have happened, because the timer should have been stopped when the plugin quit or the
        // handshake was completed
        DICEY_UNREACHABLE();
    }
}

static enum dicey_error plugin_spawn_retrieve_request(
    struct plugin_spawn_request *const data,
    struct plugin_spawn_metadata *const md,
    char **const path
) {
    assert(data && md && path);

    // we want a view only over the metadata, because that's the only thing we need to read
    struct dicey_view view = dicey_view_from(data, sizeof *md);

    ptrdiff_t result = dicey_view_read_ptr(&view, md, sizeof *md);
    if (result < 0) {
        return (enum dicey_error) result;
    }

    assert(result == sizeof *md);

    // the path is a null terminated string, starting after the metadata.
    // this is not UB because the data is a char array, so we can safely read from it directly.
    // also, the payload is not mutable to begin with, so we can cast away the const
    *path = (char *) view.data;

    return DICEY_OK;
}

static enum dicey_error spawn_child(struct dicey_server *const server, struct dicey_plugin_data *const plugin) {
    assert(server && plugin);

    uv_timer_t *const timer = &plugin->process_timer;

    timer->data = plugin; // we need to know which client to kill when the timer expires

    enum dicey_error err = dicey_error_from_uv(uv_timer_init(&server->loop, timer));
    if (err) {
        return err;
    }

    char *const path = dicey_plugin_data_get_info(plugin).path;
    assert(path);

    // start the timer immediately, so that it's already running when we spawn the child.
    // This avoids a race condition where the child starts up before the timer is started.
    err = dicey_error_from_uv(uv_timer_start(timer, &plugin_no_handshake_timeout, PLUGIN_TIMEOUT_MS, 0));
    if (err) {
        uv_close((uv_handle_t *) timer, NULL);

        return err;
    }

    // TODO: make stdin, stdout, stderr configurable
    uv_stdio_container_t child_stdio[] = {
        [0] = { .flags = UV_INHERIT_FD },
        [1] = { .flags = UV_INHERIT_FD },
        [2] = { .flags = UV_INHERIT_FD },
        [DICEY_PLUGIN_FD] = {
            .flags = UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE,
            .data.stream = (uv_stream_t *) plugin, // plugin starts with client, which can be cast to uv_pipe_t
        },
    };

    const uv_process_options_t options = {
        .exit_cb = &plugin_exit_cb,
        .file = path,
        .args = (char *[]) {path, NULL}, // we don't pass any argument by default
        .stdio = child_stdio,
        .stdio_count = DICEY_LENOF(child_stdio),
    };

    err = dicey_error_from_uv(uv_spawn(&server->loop, &plugin->process, &options));
    if (err) {
        uv_close((uv_handle_t *) &plugin->process_timer, NULL);

        return err;
    }

    plugin_change_state(plugin, PLUGIN_STATE_SPAWNED);

    return DICEY_OK;
}

static enum dicey_error plugin_spawn(
    struct dicey_server *const server,
    struct dicey_client_data *const client,
    void *const req_data
) {
    assert(server && req_data && !client);
    (void) client; // suppress unused variable warning with NDEBUG and MSVC

    struct plugin_spawn_metadata md = { 0 };
    char *path = NULL; // borrowed from req_data

    enum dicey_error err = plugin_spawn_retrieve_request(req_data, &md, &path);
    if (err) {
        return err;
    }

    assert(path);

    size_t id = 0U;
    struct dicey_plugin_data *new_plugin = NULL;

    // craft the new client data struct and reserve an ID
    err = client_data_new_plugin(server, &new_plugin, &id);
    if (err) {
        return err;
    }

    assert(new_plugin); // we just created it

    err = dicey_plugin_data_set_info(new_plugin, (struct dicey_plugin_info) { .name = NULL, .path = path });

    if (err) {
        goto quit;
    }

    err = spawn_child(server, new_plugin);

quit:
    if (err) {
        const enum dicey_error clean_err = dicey_server_cleanup_id(server, id);
        (void) clean_err;
        assert(!clean_err);
    }

    return err;
}

enum dicey_error plugin_submit_spawn(
    struct dicey_server *const server,
    const char *const path,
    const struct plugin_spawn_metadata *const extra_md
) {
    assert(server && path);

    const size_t payload_size = sizeof(struct plugin_spawn_metadata) + dutl_zstring_size(path);

    struct dicey_server_loop_request *const req = DICEY_SERVER_LOOP_REQ_NEW_WITH_BYTES(payload_size);
    if (!req) {
        return TRACE(DICEY_ENOMEM);
    }

    *req = (struct dicey_server_loop_request) {
        .cb = &plugin_spawn,
        .target = DICEY_SERVER_LOOP_REQ_NO_TARGET,
    };

    const enum dicey_error err = craft_plugin_spawn_request_in(
        DICEY_SERVER_LOOP_REQ_GET_PAYLOAD_AS_VIEW_MUT(*req, payload_size),
        extra_md ? *extra_md : (struct plugin_spawn_metadata) { 0 },
        path
    );

    if (err) {
        free(req);

        return err;
    }

    return dicey_server_submit_request(server, req);
}

struct dicey_plugin_info dicey_plugin_data_get_info(const struct dicey_plugin_data *const data) {
    assert(data);

    return data->info;
}

enum dicey_plugin_state dicey_plugin_data_get_state(const struct dicey_plugin_data *const data) {
    assert(data);

    return data->state;
}

enum dicey_error dicey_plugin_data_set_info(struct dicey_plugin_data *const data, const struct dicey_plugin_info info) {
    assert(info.name && info.path);

    const enum dicey_error err = info_dup_to(info, &data->info);
    if (err) {
        free(data);
    }

    return err;
}

enum dicey_error dicey_server_list_plugins(
    struct dicey_server *const server,
    struct dicey_plugin_info **const buf,
    size_t *const count
) {
    assert(server && buf && count);

    if (server->state != SERVER_STATE_RUNNING) {
        return TRACE(DICEY_EINVAL);
    }

    const size_t plugin_count = count_plugins(server->clients);

    // if the user provides a count, we assume that buf of size *count and the user doesn't want to allocate a new
    // buffer
    if (*count && plugin_count > *count) {
        assert(*buf);

        *count = plugin_count;

        return TRACE(DICEY_EOVERFLOW);
    }

    if (!*buf) {
        *buf = calloc(plugin_count, sizeof **buf);
        if (!*buf) {
            return TRACE(DICEY_ENOMEM);
        }
    }

    *count = plugin_count;

    struct dicey_plugin_info *plugins = *buf;

    struct dicey_client_data *const *it = dicey_client_list_begin(server->clients);
    struct dicey_client_data *const *const end = dicey_client_list_end(server->clients);

    for (; it != end; ++it) {
        const struct dicey_plugin_data *const plugin_data = as_plugin(*it);
        if (plugin_data) {
            assert(plugins < *buf + plugin_count);

            *plugins++ = plugin_data->info;
        }
    }

    return DICEY_OK;
}

enum dicey_error dicey_server_spawn_plugin(struct dicey_server *const server, const char *const path) {
    assert(server && path);

    if (server->state != SERVER_STATE_RUNNING) {
        return TRACE(DICEY_EINVAL);
    }

    return plugin_submit_spawn(server, path, NULL);
}

// we don't use the "generic" blocking request system here because we need to wait for the plugin to spawn and fully
// handshake, while dicey_server_blocking_request only waits for the request to be processed. We still use a semaphore
// though;
enum dicey_error dicey_server_spawn_plugin_and_wait(struct dicey_server *const server, const char *const path) {
    assert(server && path);

    if (server->state != SERVER_STATE_RUNNING) {
        return TRACE(DICEY_EINVAL);
    }

    uv_sem_t sync_sem = { 0 };
    const int uverr = uv_sem_init(&sync_sem, 0);
    if (uverr < 0) {
        return dicey_error_from_uv(uverr);
    }

    enum dicey_error sync_result = DICEY_OK;

    struct plugin_spawn_metadata md = {
        .spawn_wait_sem = &sync_sem,
        .spawn_result = &sync_result,
    };

    const enum dicey_error err = plugin_submit_spawn(server, path, &md);
    if (err) {
        uv_sem_destroy(&sync_sem);

        return err;
    }

    uv_sem_wait(&sync_sem);
    uv_sem_destroy(&sync_sem);

    return sync_result;
}

#else

#error "This file should not be built if plugins are disabled"

#endif // DICEY_HAS_PLUGINS
