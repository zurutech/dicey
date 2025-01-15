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

#include <stdio.h>
#if !defined(KXYIXJEFNE_PLUGINS_H)
#define KXYIXJEFNE_PLUGINS_H

#include "../builtins.h"

/*
 *     // internal operation used by the plugin API. Never call directly!
 *     HandshakeInternal: s -> @ // takes the name and returns the plugin object path
 */
#define PLUGINMANAGER_HANDSHAKEINTERNAL_OP_NAME "HandshakeInternal"
#define PLUGINMANAGER_HANDSHAKEINTERNAL_OP_SIG "s -> @"

/*
 *     // internal plugin communication. Don't call directly
 *     signal Command: {tc} // job number + an enumeration of plugin commands (private)
 *     Quitting: $ -> $     // the plugin communicates its intention to quit
 *     Reply: {tv} -> $     // reply to a command (private)
 */
#define PLUGIN_COMMAND_SIGNAL_NAME "Command"
#define PLUGIN_COMMAND_SIGNAL_SIG "(tcv)"
#define PLUGIN_QUITTING_OP_NAME "Quitting"
#define PLUGIN_QUITTING_OP_SIG "$ -> $"
#define PLUGIN_REPLY_OP_NAME "Reply"
#define PLUGIN_REPLY_OP_SIG "{tv} -> $"

#define PLUGIN_COMMAND_SIGNAL_SEL                                                                                      \
    (struct dicey_selector) { .trait = DICEY_PLUGIN_TRAIT_NAME, .elem = PLUGIN_COMMAND_SIGNAL_NAME, }

extern const struct dicey_registry_builtin_set dicey_registry_plugins_builtins;

#endif // KXYIXJEFNE_PLUGINS_H
