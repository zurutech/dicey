// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(JJYWCOYURK_SERVER_H)
#define JJYWCOYURK_SERVER_H

#include <stdbool.h>
#include <stddef.h>

#include "../core/errors.h"
#include "../core/packet.h"

#include "dicey_export.h"

struct dicey_client_info {
    size_t id;
    void *user_data;
};

struct dicey_server;

typedef bool dicey_server_on_connect_fn(size_t id, void **user_data);
typedef void dicey_server_on_disconnect_fn(const struct dicey_client_info *cln);
typedef void dicey_server_on_error_fn(enum dicey_error err, const struct dicey_client_info *cln, const char *msg, ...);
typedef void dicey_server_on_message_fn(const struct dicey_client_info *cln, struct dicey_packet packet);

struct dicey_server_args {
    dicey_server_on_connect_fn *on_connect;
    dicey_server_on_disconnect_fn *on_disconnect;
    dicey_server_on_error_fn *on_error;
    dicey_server_on_message_fn *on_message;
};

DICEY_EXPORT void dicey_server_delete(struct dicey_server *state);
DICEY_EXPORT enum dicey_error dicey_server_new(struct dicey_server **const dest, const struct dicey_server_args *args);
DICEY_EXPORT enum dicey_error dicey_server_send(struct dicey_server *server, size_t id, struct dicey_packet packet);
DICEY_EXPORT enum dicey_error dicey_server_start(struct dicey_server *server, const char *name, size_t len);

#endif // JJYWCOYURK_SERVER_H
