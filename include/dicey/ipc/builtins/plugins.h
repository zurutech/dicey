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

#if !defined(BXJFESQCSR_PLUGINS_H)
#define BXJFESQCSR_PLUGINS_H

/**
 * all plugins have a "plugin object" under the "/dicey/server/plugins" path
 */
#define DICEY_SERVER_PLUGINS_PATH "/dicey/server/plugins"

/**
 * trait dicey.PluginManager {
 *     ListPlugins: () -> {ss} // returns a list of plugins, each with a name and path
 * }
 */

#define DICEY_PLUGINMANAGER_TRAIT_NAME "dicey.PluginManager"

#define DICEY_PLUGINMANAGER_LISTPLUGINS_OP_NAME "ListPlugins"
#define DICEY_PLUGINMANAGER_LISTPLUGINS_OP_SIG "$ -> [{ss}]"

/**
 * trait dicey.Plugin {
 *     ro Name: s // name
 *     ro Path: s // path to executable
 * }
 */

#define DICEY_PLUGIN_TRAIT_NAME "dicey.Plugin"

#define DICEY_PLUGIN_NAME_PROP_NAME "Name"
#define DICEY_PLUGIN_NAME_PROP_SIG "s"

#define DICEY_PLUGIN_PATH_PROP_NAME "Path"
#define DICEY_PLUGIN_PATH_PROP_SIG "s"

#endif // BXJFESQCSR_PLUGINS_H
