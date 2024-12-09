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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>

#include <dicey/core/errors.h>
#include <dicey/core/views.h>
#include <dicey/ipc/builtins/plugins.h>
#include <dicey/ipc/plugins.h>
#include <dicey/ipc/server.h>

#include "sup/trace.h"
#include "sup/util.h"
#include "sup/uvtools.h"
#include "sup/view-ops.h"

#include "ipc/plugin-macros.h"

#include "client-data.h"
#include "plugins-internal.h"
#include "registry-internal.h"
#include "server-clients.h"
#include "server-loopreq.h"
#include "server.h"

#include "dicey_config.h"

// NOTE: this file is a little bit too spaghettey. In the future it would be smart to use the same task system the
// client uses for single server clients too

/*
 * Plugin exit flows
 * =================
 *
 * # Kick/client closes pipe:
 * kick(client) -> close(pipe(S)) -> client_data_cleanup(client) -> plugin_cleanup(client) -> term(child) + Timer ->
 * exit_cb(child) -> plugin_deinit(client) -> client_data_deinit(client) ↳ term_timeout(child) -> kill(child) ⬏
 *
 * # Quit
 * exit(client) -> exit_cb(child) -> close(pipe(S)) ->  client_data_cleanup(client) -> plugin_cleanup(client) ->
 * plugin_deinit(client) -> client_data_deinit(client)
 */

// note: the double cast is necessary to avoid a warning with MSVC
#define CLIENT_DATA_IS_PLUGIN(CLIENTPTR) ((CLIENTPTR)->is_plugin)

#define METAPLUGIN_FORMAT DICEY_SERVER_PLUGINS_PATH "/%s"

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

#define TERM_SIGNAL SIGTERM

// the fixed, non variable-sized bits of the request
struct plugin_spawn_metadata {
    uv_sem_t *spawn_wait_sem;       // optional, only for blocking requests
    enum dicey_error *spawn_result; // optional, used to report the result of the spawn if it's a blocking request
};

struct dicey_plugin_data {
    struct dicey_client_data client; // must remain the first field for the cast to work

    uv_process_t process;

    // timer used for both the handshake timeout and process killing
    uv_timer_t process_timer;

    enum dicey_plugin_state state;
    struct dicey_plugin_info info;

    // spawn metadata
    struct plugin_spawn_metadata spawn_md;
};

struct plugin_spawn_request {
    struct plugin_spawn_metadata md;

    char path[];
};

static struct dicey_plugin_data *as_plugin(struct dicey_client_data *const client) {
    return client && CLIENT_DATA_IS_PLUGIN(client) ? (struct dicey_plugin_data *) client : NULL;
}

static enum dicey_error plugin_object_delete(struct dicey_registry *const registry, const char *const name) {
    assert(registry && name);

    const char *const metaplugin_name = dicey_registry_format_metaname(registry, METAPLUGIN_FORMAT, name);
    if (!metaplugin_name) {
        return TRACE(DICEY_ENOMEM); // extraordinarily unlikely
    }

    return dicey_registry_delete_object(registry, metaplugin_name);
}

enum dicey_error plugin_deinit(struct dicey_plugin_data *const data) {
    enum dicey_error err = DICEY_OK;

    if (data) {
        // deregister the plugin from the registry. Set the error for later
        err = plugin_object_delete(&data->client.parent->registry, data->info.name);

        // the strings were either NULL or strdup'd in dicey_plugin_data_set, so we can cast away the const safely

        free((char *) data->info.name);
        free((char *) data->info.path);

        // if the plugin was ever spawned, we need to close the process and the timer
        if (data->state != PLUGIN_STATE_INVALID) {
            uv_close((uv_handle_t *) &data->process, NULL);
            uv_close((uv_handle_t *) &data->process_timer, NULL);
        }
    }

    return err;
}

static void kill_child(struct dicey_plugin_data *const plugin, const int signal) {
    assert(plugin && uv_is_active((uv_handle_t *) &plugin->process));

    const int uverr = uv_process_kill(&plugin->process, signal);
    if (uverr < 0) {
        // we can't really do anything about it
        DICEY_UNREACHABLE();
    }
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

    case PLUGIN_STATE_DISCONNECTED:
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

// if TERM_SIGNAL is equal to KILL_SIGNAL, we can't really ask the process to quit nicely
#define TERM_IS_KILL (TERM_SIGNAL == KILL_SIGNAL)

#if !TERM_IS_KILL
// disconnect policy: send SIGTERM and wait for the process to exit

static void plugin_terminate_timedout(uv_timer_t *const timer) {
    assert(timer);

    struct dicey_plugin_data *const plugin = DICEY_CONTAINEROF(timer, struct dicey_plugin_data, process_timer);
    assert(plugin && plugin->state == PLUGIN_STATE_DISCONNECTED);

    // the plugin did not answer to a SIGTERM, so we must kill it with SIGKILL
    kill_child(plugin, KILL_SIGNAL);

    // exit_cb should fire now
}

#endif // TERM_SIGNAL == KILL_SIGNAL

struct plugin_terminate_state {
    dicey_client_data_after_cleanup_fn *after_cleanup;
};

enum dicey_error plugin_cleanup_disconnect_data(struct dicey_plugin_data *const data) {
    assert(data);

    dicey_client_data_after_cleanup_fn *after_cleanup = NULL;
    struct plugin_terminate_state *const tstate = data->process_timer.data;
    if (tstate) {
        after_cleanup = tstate->after_cleanup;
        assert(after_cleanup); // if we've allocated a struct at least I hope we've got a callback

        free(tstate);
    }

    // if we're in this function:
    // 1. the pipe was closed somehow (probably a kick or a close() on the client end)
    // 2. the server, cleaning up the client, noticed there was still a child lingering and killed it
    // 3. the child died after we sent the SIGTERM/KILL
    // 4. the exit_cb callback was fired and called this function
    // this finishes up the cleanup
    enum dicey_error err = plugin_deinit(data);

    if (after_cleanup) {
        const enum dicey_error after_err = after_cleanup((struct dicey_client_data *) data);

        if (after_err) {
            err = after_err;
        }
    }

    return err;
}

static enum dicey_error plugin_kill_disconnected(
    struct dicey_plugin_data *const data,
    dicey_client_data_after_cleanup_fn *const after_cleanup
) {
    assert(data);

    if (after_cleanup) {
        // this is ugly, but necessary: we must store the callback somewhere
        // execution will resume later in exit_cb when this is cleaned up
        // note: we can't put a function pointer into a void pointer, legally
        struct plugin_terminate_state *const state = malloc(sizeof *state);
        if (!state) {
            // don't really know what to do now, but any OS worth its salt will have killed us by now
            return TRACE(DICEY_ENOMEM);
        }

        *state = (struct plugin_terminate_state) {
            .after_cleanup = after_cleanup,
        };

        // put the data in the timer handle
        data->process_timer.data = state;
    }

    plugin_change_state(data, PLUGIN_STATE_DISCONNECTED);

    // send SIGTERM to the child
    kill_child(data, TERM_SIGNAL);

#if TERM_IS_KILL
    return DICEY_OK; // exit_cb will follow later for cleanup
#else
    // use the timer to wait a while and then wipe everything
    // again, if this fails there's very little that can be done
    // this is a race between the process quitting and the timer: exit_cb will stop the timer if needed
    return dicey_error_from_uv(uv_timer_start(&data->process_timer, &plugin_terminate_timedout, PLUGIN_TIMEOUT_MS, 0));
#endif // TERM_IS_KILL
}

// cleans up the plugin data in general. called by the server when the _pipe_ is closed
static enum dicey_error plugin_data_cleanup(
    struct dicey_client_data *const client,
    dicey_client_data_after_cleanup_fn *const after_cleanup
) {
    if (client) {
        struct dicey_plugin_data *const data = as_plugin(client);
        assert(data);

        switch (data->state) {
        case PLUGIN_STATE_RUNNING:
            // The server shut down the pipe and the child must now be reaped nicely on POSIX, and not so nicely on
            // Windows cleanup will resume later in the exit_cb
            return plugin_kill_disconnected(data, after_cleanup);

        case PLUGIN_STATE_FAILED:
        case PLUGIN_STATE_COMPLETE:
            {
                // the server process is dead already. Just cleanup the plugin data.
                enum dicey_error err = plugin_deinit(data);

                // continue the cleanup if there's an after_cleanup
                if (after_cleanup) {
                    const enum dicey_error after_err = after_cleanup(client);
                    if (after_err) {
                        // if there was an error in the after_cleanup, we return that instead of the previous error
                        // if after_err is DICEY_OK, we return the previous error if any
                        err = after_err;
                    }
                }

                return err;
            }

        default:
            DICEY_UNREACHABLE(
            ); // we should never reach this state. We should halt only from failed, complete or running

            return DICEY_EAGAIN; // literally a random error
        }
    }

    return DICEY_OK;
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

static enum dicey_error info_dup_to(struct dicey_plugin_info *const dst, struct dicey_plugin_info src) {
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

static void plugin_exit_cb(uv_process_t *const proc, const int64_t exit_status, const int term_signal) {
    assert(proc);

    // we know that this uv_process_t is from a dicey_plugin_data, so we can safely cast it using the containerof macro
    struct dicey_plugin_data *const plugin = DICEY_CONTAINEROF(proc, struct dicey_plugin_data, process);
    assert(plugin); // useless check, but it's here for clarity

    // true if we sent the process SIGTERM (not SIGKILL) and we were waiting for it to close
    const bool was_disconnected = plugin->state == PLUGIN_STATE_DISCONNECTED;

    const bool failed = exit_status != EXIT_SUCCESS || term_signal;

    plugin_change_state(plugin, failed ? PLUGIN_STATE_FAILED : PLUGIN_STATE_COMPLETE);

    struct dicey_server *const server = plugin->client.parent;
    assert(server);

    const struct dicey_client_info *const info = &plugin->client.info;

    // if the process was asked to quit with a SIGTERM, it was because the pipe was closed before it could quit
    // therefore we were already in the middle of a dicey_server_remove_client call that got suspended
    const enum dicey_error err =
        was_disconnected ? plugin_cleanup_disconnect_data(plugin) : dicey_server_remove_client(server, info->id);
    if (err && server->on_error) {
        server->on_error(server, err, info, "failed to cleanup plugin: %s\n", dicey_error_name(err));
    }
}

static void plugin_no_handshake_timeout(uv_timer_t *const timer) {
    assert(timer);

    struct dicey_plugin_data *const plugin = DICEY_CONTAINEROF(timer, struct dicey_plugin_data, process_timer);
    assert(plugin);

    // if the plugin is still in the handshake state, we kill it
    if (plugin->state == PLUGIN_STATE_SPAWNED) {
        // the plugin has not yet completed the handshake, so we kill it (not very nicely)
        kill_child(plugin, KILL_SIGNAL);

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
    DICEY_UNUSED(client); // suppress unused variable warning with NDEBUG and MSVC

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

    err = info_dup_to(&new_plugin->info, (struct dicey_plugin_info) { .name = NULL, .path = path });

    if (err) {
        goto quit;
    }

    err = spawn_child(server, new_plugin);

quit:
    if (err) {
        const enum dicey_error clean_err = dicey_server_cleanup_id(server, id);
        DICEY_UNUSED(clean_err);
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

enum dicey_error dicey_server_list_plugins(
    struct dicey_server *const server,
    struct dicey_plugin_info **const buf,
    uint16_t *const count
) {
    assert(server && count);

    if (server->state != SERVER_STATE_RUNNING) {
        return TRACE(DICEY_EINVAL);
    }

    const size_t plugin_count = count_plugins(server->clients);
    if (plugin_count > UINT16_MAX) {
        return TRACE(DICEY_EOVERFLOW);
    }

    if (!plugin_count) {
        // quick shortcircuit: set NULL and quit
        *buf = NULL;
        *count = 0U;

        return DICEY_OK;
    }

    // null target buffer: just count the plugins
    if (!buf) {
        *count = (uint16_t) plugin_count;

        return DICEY_OK;
    }

    // if the user provides a count, we assume that buf of size *count and the user doesn't want to allocate a new
    // buffer
    if (*count && plugin_count > *count) {
        assert(*buf);

        *count = (uint16_t) plugin_count;

        return TRACE(DICEY_EOVERFLOW);
    }

    if (!*buf) {
        *buf = calloc(plugin_count, sizeof **buf);
        if (!*buf) {
            return TRACE(DICEY_ENOMEM);
        }
    }

    *count = (uint16_t) plugin_count;

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

enum dicey_error dicey_server_plugin_handshake(
    struct dicey_server *const server,
    struct dicey_client_data *const client,
    const char *const name,
    const char **const out_path
) {
    assert(server && client && name && out_path);

    const char *metaplugin_name = NULL;
    enum dicey_error err = DICEY_OK;
    struct dicey_plugin_data *const plugin = as_plugin(client);
    if (!plugin || plugin->state != PLUGIN_STATE_SPAWNED) {
        err = TRACE(DICEY_EINVAL);

        goto quit;
    }

    // the name must not be set yet
    assert(!plugin->info.name);

    err = info_dup_to(&plugin->info, (struct dicey_plugin_info) { .name = name, .path = plugin->info.path });
    if (err) {
        goto quit;
    }

    // create the plugin object
    metaplugin_name = dicey_registry_format_metaname(&server->registry, METAPLUGIN_FORMAT, name);
    if (!metaplugin_name) {
        err = TRACE(DICEY_ENOMEM);

        goto quit;
    }

    err = dicey_registry_add_object_with(&server->registry, metaplugin_name, DICEY_PLUGIN_TRAIT_NAME, NULL);
    if (err) {
        goto quit;
    }

    // this has to be the last thing ever done in the handshake, otherwise we need to restart it if something fails
    err = dicey_error_from_uv(uv_timer_stop(&plugin->process_timer));
    if (err) {
        goto quit;
    }

    plugin_change_state(plugin, PLUGIN_STATE_RUNNING);

quit:
    {
        uv_sem_t *const spawn_wait_sem = plugin->spawn_md.spawn_wait_sem;
        if (spawn_wait_sem) {
            *plugin->spawn_md.spawn_result = err;
            uv_sem_post(spawn_wait_sem);
        }
    }

    *out_path = metaplugin_name;

    return err;
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
