#if !defined(NPTFJAYCZU_PLUGINS_H)
#define NPTFJAYCZU_PLUGINS_H

#include "dicey_config.h"

#if DICEY_HAS_PLUGINS

#include <dicey/core/errors.h>
#include <dicey/ipc/plugins.h>

enum dicey_plugin_state {
    PLUGIN_STATE_INVALID,

    PLUGIN_STATE_SPAWNED, // the child was spawned, but hasn't yet handshaked

    // good states
    PLUGIN_STATE_RUNNING, // the child is running and has handshaked with the server

    // quitting states
    PLUGIN_STATE_QUITTING, // the child is quitting (cleanly)

    // final states
    PLUGIN_STATE_FAILED,   // the child is dead; either it failed to handshake or it returned a non-zero exit code
    PLUGIN_STATE_COMPLETE, // the child is dead; it has exited cleanly
};

struct dicey_plugin_data;

struct dicey_plugin_info dicey_plugin_data_get_info(const struct dicey_plugin_data *data);
enum dicey_plugin_state dicey_plugin_data_get_state(const struct dicey_plugin_data *data);

#endif // DICEY_HAS_PLUGINS

#endif // NPTFJAYCZU_PLUGINS_H
