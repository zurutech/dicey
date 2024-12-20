#if !defined(NPTFJAYCZU_PLUGINS_H)
#define NPTFJAYCZU_PLUGINS_H

#include "dicey_config.h"

#if DICEY_HAS_PLUGINS

#include <stdbool.h>

#include <dicey/core/errors.h>
#include <dicey/ipc/plugins.h>
#include <dicey/ipc/server.h>

#include "client-data.h"

enum dicey_plugin_state {
    PLUGIN_STATE_INVALID,

    PLUGIN_STATE_SPAWNED, // the child was spawned, but hasn't yet handshaked

    // good states
    PLUGIN_STATE_RUNNING, // the child is running and has handshaked with the server

    // quitting states
    PLUGIN_STATE_DISCONNECTED, // the child has no working pipe anymore, so it was sent a signal to quit

    // final states
    PLUGIN_STATE_FAILED,   // the child is dead; either it failed to handshake or it returned a non-zero exit code
    PLUGIN_STATE_COMPLETE, // the child is dead; it has exited cleanly
};

struct dicey_plugin_data;

struct dicey_plugin_data *dicey_client_data_as_plugin(struct dicey_client_data *client);

struct dicey_plugin_info dicey_plugin_data_get_info(const struct dicey_plugin_data *data);
enum dicey_plugin_state dicey_plugin_data_get_state(const struct dicey_plugin_data *data);

enum dicey_error dicey_server_plugin_handshake(
    struct dicey_server *server,
    struct dicey_plugin_data *plugin,
    const char *name,
    const char **obj_path // careful - borrowed from the registry, do not store!
);

struct dicey_plugin_data *dicey_server_plugin_find_by_name(const struct dicey_server *server, const char *name);

enum dicey_error dicey_server_plugin_report_work_done(
    struct dicey_server *server,
    struct dicey_plugin_data *plugin,
    uint64_t jid,
    const struct dicey_value *value
);

bool dicey_string_is_valid_plugin_name(const char *name);

#endif // DICEY_HAS_PLUGINS

#endif // NPTFJAYCZU_PLUGINS_H
