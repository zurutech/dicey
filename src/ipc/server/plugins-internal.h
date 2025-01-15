#if !defined(NPTFJAYCZU_PLUGINS_H)
#define NPTFJAYCZU_PLUGINS_H

#include "dicey_config.h"

#if DICEY_HAS_PLUGINS

#include <stdbool.h>

#include <dicey/core/errors.h>
#include <dicey/ipc/builtins/plugins.h>
#include <dicey/ipc/plugins.h>
#include <dicey/ipc/server.h>

#include "client-data.h"

#define DICEY_METAPLUGIN_FORMAT DICEY_SERVER_PLUGINS_PATH "/%s"

enum dicey_plugin_state {
    PLUGIN_STATE_INVALID,

    PLUGIN_STATE_SPAWNED, // the child was spawned, but hasn't yet handshaked

    // good states
    PLUGIN_STATE_RUNNING, // the child is running and has handshaked with the server

    // quitting states
    PLUGIN_STATE_TERMINATED, // the child has no working pipe anymore, so it was sent a SIGTERM (Unix only)
    PLUGIN_STATE_QUITTING,   // the child has communicated its intention of quitting via IPC

    // final states
    PLUGIN_STATE_FAILED,   // the child is dead; either it failed to handshake or it returned a non-zero exit code
    PLUGIN_STATE_COMPLETE, // the child is dead; it has exited cleanly
};

// the fixed, non variable-sized bits of a spawn (or quit) request
struct plugin_spawn_metadata {
    struct dicey_plugin_info *out_info; // output, will be filled after handshake (spawn only)
    uv_sem_t *wait_sem;                 // optional, only for blocking requests
    enum dicey_error *error;            // optional, will be set with the result of the spawn/quit operation
    int64_t *retval;                    // optional, will be set with the exit status of the child (quit only)
};

struct plugin_work_request {
    uint64_t jid;                                 // the job id
    dicey_server_plugin_on_work_done_fn *on_done; // the callback to call when the work is done
    void *ctx;                                    // the context to pass to the callback
};

void dicey_server_plugin_work_request_cancel(struct plugin_work_request *elem);

// struct used by the sync work request to store the result
struct dicey_work_request_sync_data {
    uv_sem_t *sem;
    struct dicey_owning_value *result;
    enum dicey_error err;
};

#define ARRAY_TYPE_NAME plugin_work_list
#define ARRAY_VALUE_TYPE struct plugin_work_request
#define ARRAY_VALUE_TYPE_NEEDS_CLEANUP 1
#include "sup/array.inc"

struct dicey_plugin_data {
    struct dicey_client_data client; // must remain the first field for the cast to work

    uv_process_t process;

    // timer used for both the handshake timeout and process killing
    uv_timer_t process_timer;

    enum dicey_plugin_state state;
    struct dicey_plugin_info info;

    uint64_t next_jid;                  // the next job id
    struct plugin_work_list *work_list; // list of pending jobs

    // spawn metadata
    struct plugin_spawn_metadata spawn_md;

    // store the after cleanup function somewhere during cleanup
    // this is necessary because the cleanup has to be done in multiple steps to let the various close
    // variables time to fire before freeing the memory
    dicey_client_data_after_cleanup_fn *after_cleanup;
};

struct dicey_plugin_data *dicey_client_data_as_plugin(struct dicey_client_data *client);

struct dicey_plugin_info dicey_plugin_data_get_info(const struct dicey_plugin_data *data);
enum dicey_plugin_state dicey_plugin_data_get_state(const struct dicey_plugin_data *data);

struct dicey_plugin_data *dicey_server_plugin_find_by_name(const struct dicey_server *server, const char *name);

enum dicey_error dicey_server_plugin_handshake(
    struct dicey_server *server,
    struct dicey_plugin_data *plugin,
    const char *name,
    const char **obj_path // careful - borrowed from the registry, do not store!
);

enum dicey_error dicey_server_plugin_report_work_done(
    struct dicey_server *server,
    struct dicey_plugin_data *plugin,
    uint64_t jid,
    const struct dicey_owning_value *value
);

enum dicey_error dicey_server_plugin_quitting(struct dicey_server *server, struct dicey_plugin_data *plugin);

bool dicey_string_is_valid_plugin_name(const char *name);

#endif // DICEY_HAS_PLUGINS

#endif // NPTFJAYCZU_PLUGINS_H
