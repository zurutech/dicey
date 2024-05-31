// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(GFBAZEDZQQ_SERVER_H)
#define GFBAZEDZQQ_SERVER_H

/**
 * object "/dicey/server" : dicey.EventManager
 */
#define DICEY_SERVER_PATH "/dicey/server"

/**
 * trait dicey.EventManager {
 *     Subscribe: (@%) -> u // takes a path and selector of an event to subscribe to
 *     Unsubscribe: (@%) -> u // takes a path and selector of an event to unsubscribe from
 * }
 */

#define DICEY_EVENTMANAGER_TRAIT_NAME "dicey.EventManager"

#define DICEY_EVENTMANAGER_SUBSCRIBE_OP_NAME "Subscribe"
#define DICEY_EVENTMANAGER_SUBSCRIBE_OP_SIG "(@%) -> u"

#define DICEY_EVENTMANAGER_UNSUBSCRIBE_OP_NAME "Unsubscribe"
#define DICEY_EVENTMANAGER_UNSUBSCRIBE_OP_SIG "(@%) -> u"

extern const struct dicey_registry_builtin_set dicey_registry_server_builtins;

#endif // GFBAZEDZQQ_SERVER_H
