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

#if !defined(GFBKZEFZQX_SERVER_H)
#define GFBKZEFZQX_SERVER_H

#include "dicey_config.h"

/**
 * object "/dicey/server" : dicey.SignalManager, dicey.PluginManager (if plugins are enabled)
 */
#define DICEY_SERVER_PATH "/dicey/server"

/**
 * trait dicey.SignalManager {
 *     Subscribe: {@%} -> v   // takes a path and selector of a signal to subscribe to. Returns either unit or the
 *                            // main path of the target object (if the request targets an alias)
 *                            // Note: signals are never raised on aliases. The client must take note of whether the
 *                            // user subscribed to an aliased path or not.
 *     Unsubscribe: {@%} -> $ // takes a path and selector of a signal to unsubscribe from. Will follow aliases.
 * }
 */

#define DICEY_EVENTMANAGER_TRAIT_NAME "dicey.SignalManager"

#define DICEY_EVENTMANAGER_SUBSCRIBE_OP_NAME "Subscribe"
#define DICEY_EVENTMANAGER_SUBSCRIBE_OP_SIG "{@%} -> v"

#define DICEY_EVENTMANAGER_UNSUBSCRIBE_OP_NAME "Unsubscribe"
#define DICEY_EVENTMANAGER_UNSUBSCRIBE_OP_SIG "{@%} -> $"

#endif // GFBKZEFZQX_SERVER_H
