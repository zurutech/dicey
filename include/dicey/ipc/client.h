// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(RGSZHBDASZ_CLIENT_H)
#define RGSZHBDASZ_CLIENT_H

#include "../core/errors.h"
#include "../core/packet.h"

struct dicey_client;

typedef void dicey_client_at_exit_fn(struct dicey_client *client);
typedef void dicey_client_on_connect_fn(struct dicey_client *client);
typedef void dicey_client_on_disconnect_fn(struct dicey_client *client);
typedef void dicey_client_on_error_fn(const struct dicey_client *client, enum dicey_error err, const char *msg, ...);
typedef void dicey_client_on_message_recv_fn(struct dicey_client *client, struct dicey_packet packet);
typedef void dicey_client_on_message_sent_fn(struct dicey_client *client, struct dicey_packet packet);

struct dicey_client_args {
    dicey_client_at_exit_fn *at_exit;
    dicey_client_on_connect_fn *on_connect;
    dicey_client_on_disconnect_fn *on_disconnect;
    dicey_client_on_error_fn *on_error;
    dicey_client_on_message_recv_fn *on_message_recv;
    dicey_client_on_message_sent_fn *on_message_sent;
};

struct dicey_addr {
    const char *addr;
    size_t len;
};

DICEY_EXPORT struct dicey_addr dicey_addr_from_str(const char *str);

DICEY_EXPORT void dicey_client_delete(struct dicey_client *client);
DICEY_EXPORT enum dicey_error dicey_client_new(
    struct dicey_client **const dest,
    const struct dicey_client_args *const args
);
DICEY_EXPORT enum dicey_error dicey_client_connect(struct dicey_client *client, struct dicey_addr addr);
DICEY_EXPORT void *dicey_client_get_data(const struct dicey_client *client);
DICEY_EXPORT enum dicey_error dicey_client_send(struct dicey_client *const client, struct dicey_packet packet);
DICEY_EXPORT void *dicey_client_set_data(struct dicey_client *client, void *data);

#endif // RGSZHBDASZ_CLIENT_H
