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

#if !defined(IJDIIZJEMO_DICEY_H)
#define IJDIIZJEMO_DICEY_H

#include "core/builders.h"
#include "core/data-info.h"
#include "core/errors.h"
#include "core/hashset.h"
#include "core/hashtable.h"
#include "core/packet.h"
#include "core/type.h"
#include "core/typedescr.h"
#include "core/value.h"
#include "core/version.h"
#include "core/views.h"
#include "ipc/address.h"
#include "ipc/builtins.h"
#include "ipc/builtins/introspection.h"
#include "ipc/builtins/server.h"
#include "ipc/client.h"
#include "ipc/registry.h"
#include "ipc/server-api.h"
#include "ipc/server.h"
#include "ipc/traits.h"

#include "dicey_config.h"

#if DICEY_HAS_PLUGINS

#include "ipc/builtins/plugins.h"
#include "ipc/plugins.h"

#endif // DICEY_HAS_PLUGINS

#endif // IJDIIZJEMO_DICEY_H
