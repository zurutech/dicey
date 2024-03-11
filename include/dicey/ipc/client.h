// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(RGSZHBDASZ_CLIENT_H)
#define RGSZHBDASZ_CLIENT_H

#include "dicey_export.h"

#include "../core/errors.h"
#include "../core/packet.h"

#include "address.h"

#if defined(__cplusplus)
extern "C" {
#endif

enum dicey_client_event_type {
    DICEY_CLIENT_EVENT_CONNECT,
    DICEY_CLIENT_EVENT_ERROR,
    DICEY_CLIENT_EVENT_HANDSHAKE_START,
    DICEY_CLIENT_EVENT_INIT,
    DICEY_CLIENT_EVENT_MESSAGE_RECEIVING,
    DICEY_CLIENT_EVENT_MESSAGE_SENDING,
    DICEY_CLIENT_EVENT_SERVER_BYE,
    DICEY_CLIENT_EVENT_QUITTING,
    DICEY_CLIENT_EVENT_QUIT,
};

struct dicey_client_event {
    enum dicey_client_event_type type;

    union {
        struct {
            enum dicey_error err;
            char *msg;
        } error;
        struct dicey_packet packet;
        struct dicey_version version;
    };
};

struct dicey_client;

typedef void dicey_client_on_connect_fn(
    struct dicey_client *client,
    void *ctx,
    enum dicey_error status,
    const char *msg
);
typedef void dicey_client_on_disconnect_fn(struct dicey_client *client, void *ctx, enum dicey_error status);

typedef void dicey_client_on_reply_fn(
    struct dicey_client *client,
    void *ctx,
    enum dicey_error status,
    struct dicey_packet *packet // this is a pointer, so the caller can steal the value, if it so desires
);

typedef void dicey_client_event_fn(struct dicey_client *client, void *ctx, struct dicey_packet packet);
typedef void dicey_client_inspect_fn(struct dicey_client *client, void *ctx, struct dicey_client_event event);

struct dicey_client_args {
    dicey_client_inspect_fn *inspect_func;
    dicey_client_event_fn *on_event;
};

DICEY_EXPORT enum dicey_error dicey_client_new(struct dicey_client **dest, const struct dicey_client_args *args);
DICEY_EXPORT void dicey_client_delete(struct dicey_client *client);

DICEY_EXPORT enum dicey_error dicey_client_connect(struct dicey_client *client, struct dicey_addr addr);
DICEY_EXPORT enum dicey_error dicey_client_connect_async(
    struct dicey_client *client,
    struct dicey_addr addr,
    dicey_client_on_connect_fn *cb,
    void *data
);

DICEY_EXPORT enum dicey_error dicey_client_disconnect(struct dicey_client *client);
DICEY_EXPORT enum dicey_error dicey_client_disconnect_async(
    struct dicey_client *client,
    dicey_client_on_disconnect_fn *cb,
    void *data
);

DICEY_EXPORT void *dicey_client_get_context(const struct dicey_client *client);

DICEY_EXPORT bool dicey_client_is_running(const struct dicey_client *client);

DICEY_EXPORT enum dicey_error dicey_client_request(
    struct dicey_client *client,
    struct dicey_packet packet,
    struct dicey_packet *response,
    uint32_t timeout
);

DICEY_EXPORT enum dicey_error dicey_client_request_async(
    struct dicey_client *client,
    struct dicey_packet packet,
    dicey_client_on_reply_fn *cb,
    void *data,
    uint32_t timeout
);

DICEY_EXPORT void *dicey_client_set_context(struct dicey_client *client, void *data);

#if defined(__cplusplus)
}
#endif

#endif // RGSZHBDASZ_CLIENT_H
