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

#if !defined(GFBKZEFZQX_SERVER_H)
#define GFBKZEFZQX_SERVER_H

#include "dicey_config.h"

/**
 * object "/dicey/server" : dicey.EventManager, dicey.PluginManager
 */
#define DICEY_SERVER_PATH "/dicey/server"

/**
 * trait dicey.EventManager {
 *     Subscribe: {@%} -> u // takes a path and selector of an event to subscribe to
 *     Unsubscribe: {@%} -> u // takes a path and selector of an event to unsubscribe from
 * }
 */

#define DICEY_EVENTMANAGER_TRAIT_NAME "dicey.EventManager"

#define DICEY_EVENTMANAGER_SUBSCRIBE_OP_NAME "Subscribe"
#define DICEY_EVENTMANAGER_SUBSCRIBE_OP_SIG "{@%} -> $"

#define DICEY_EVENTMANAGER_UNSUBSCRIBE_OP_NAME "Unsubscribe"
#define DICEY_EVENTMANAGER_UNSUBSCRIBE_OP_SIG "{@%} -> $"

#if DICEY_HAS_PLUGINS

/**
 * trait dicey.PluginManager {
 *     ListPlugins: () -> {ss} // returns a list of plugins, each with a name and path
 * }
 */

#define DICEY_PLUGINMANAGER_TRAIT_NAME "dicey.PluginManager"

#define DICEY_PLUGINMANAGER_LISTPLUGINS_OP_NAME "ListPlugins"
#define DICEY_PLUGINMANAGER_LISTPLUGINS_OP_SIG "$ -> [{ss}]"

#endif // DICEY_HAS_PLUGINS

#endif // GFBKZEFZQX_SERVER_H
