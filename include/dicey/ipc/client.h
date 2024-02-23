// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(RGSZHBDASZ_CLIENT_H)
#define RGSZHBDASZ_CLIENT_H

#include "../core/errors.h"
#include "../core/packet.h"

#if defined(__cplusplus)
extern "C" {
#endif

enum dicey_client_event_type {
    DICEY_CLIENT_EVENT_CONNECT,
    DICEY_CLIENT_EVENT_DISCONNECT,
    DICEY_CLIENT_EVENT_ERROR,
    DICEY_CLIENT_EVENT_HANDSHAKE_START,
    DICEY_CLIENT_EVENT_HANDSHAKE_WAITING,
    DICEY_CLIENT_EVENT_INIT,
    DICEY_CLIENT_EVENT_MESSAGE_RECEIVING,
    DICEY_CLIENT_EVENT_MESSAGE_SENDING,
    DICEY_CLIENT_EVENT_SERVER_BYE,
};

struct dicey_client_event {
    enum dicey_client_event_type type;

    union {
        struct {
            enum dicey_error err;
            char *msg;
        } error;
        struct dicey_packet *packet;
        struct dicey_version version;
    };
};

struct dicey_client;

typedef void dicey_client_on_connect_fn(struct dicey_client *client, void *ctx, enum dicey_error status);
typedef void dicey_client_on_reply_fn(
    struct dicey_client *client,
    enum dicey_error status,
    struct dicey_packet packet,
    void *ctx
);

typedef void dicey_client_event_fn(const struct dicey_client *client, void *ctx, struct dicey_packet packet);
typedef void dicey_client_inspect_fn(const struct dicey_client *client, void *ctx, struct dicey_client_event event);

struct dicey_client_args {
    dicey_client_inspect_fn *inspect_func;
    dicey_client_event_fn *on_event;
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
DICEY_EXPORT enum dicey_error dicey_client_connect_async(
    struct dicey_client *client,
    struct dicey_addr addr,
    dicey_client_on_connect_fn *cb,
    void *data
);

DICEY_EXPORT void *dicey_client_get_context(const struct dicey_client *client);

DICEY_EXPORT enum dicey_error dicey_client_request(
    struct dicey_client *const client,
    struct dicey_packet packet,
    struct dicey_packet *const response,
    uint32_t timeout
);
DICEY_EXPORT enum dicey_error dicey_client_request_async(
    struct dicey_client *const client,
    struct dicey_packet packet,
    dicey_client_on_reply_fn *cb,
    void *data,
    uint32_t timeout
);

#if defined(__cplusplus)
}
#endif

#endif // RGSZHBDASZ_CLIENT_H
