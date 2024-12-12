#if !defined(GINEOLWEYX_PLUGIN_MACROS_H)
#define GINEOLWEYX_PLUGIN_MACROS_H

// safe bet: we spawn the process with a pipe to communicate with it on 3.
#define DICEY_PLUGIN_FD 3

enum dicey_plugin_command {
    PLUGIN_COMMAND_DO_WORK, //
    PLUGIN_COMMAND_HALT,
};

#endif // GINEOLWEYX_PLUGIN_MACROS_H
