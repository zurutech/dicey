#if !defined(FJWTVTVLMM_BUILDERS_H)
#define FJWTVTVLMM_BUILDERS_H

#include <stdint.h>

#include "dicey_export.h"
#include "packet.h"
#include "value.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct dicey_message_builder {
    int _state;

    enum dicey_op _type;
    uint32_t _seq;

    const char *_path;
    struct dicey_selector _selector;
    struct dicey_arg *_root;

    const struct dicey_value_builder *_borrowed_to;
};

DICEY_EXPORT enum dicey_error dicey_message_builder_init(struct dicey_message_builder *builder);

DICEY_EXPORT enum dicey_error dicey_message_builder_begin(struct dicey_message_builder *builder, enum dicey_op type);
DICEY_EXPORT enum dicey_error dicey_message_builder_build(struct dicey_message_builder *builder, struct dicey_packet *packet);
DICEY_EXPORT enum dicey_error dicey_message_builder_destroy(struct dicey_message_builder *builder);
DICEY_EXPORT void dicey_message_builder_discard(struct dicey_message_builder *builder);
DICEY_EXPORT enum dicey_error dicey_message_builder_set_path(struct dicey_message_builder *builder, const char *path);

DICEY_EXPORT enum dicey_error dicey_message_builder_set_selector(
    struct dicey_message_builder *builder,
    struct dicey_selector selector
);

DICEY_EXPORT enum dicey_error dicey_message_builder_set_seq(struct dicey_message_builder *builder, uint32_t seq);

struct dicey_arg {
    enum dicey_type type;

    union {
        dicey_bool boolean;
        dicey_byte byte;

        dicey_float floating;

        dicey_i16 i16;
        dicey_i32 i32;
        dicey_i64 i64;

        dicey_u16 u16;
        dicey_u32 u32;
        dicey_u64 u64;

        struct dicey_array_arg {
            enum dicey_type type;
            uint16_t nitems;
            const struct dicey_arg *elems;
        } array;
        struct dicey_tuple_arg {
            uint16_t nitems;
            const struct dicey_arg *elems;
        } tuple;
        struct dicey_pair_arg {
            const struct dicey_arg *first;
            const struct dicey_arg *second;
        } pair;
        struct dicey_bytes_arg {
            uint32_t len;
            const uint8_t *data;
        } bytes;
        const char *str;// for str, path
        struct dicey_selector selector;
        struct dicey_error_arg {
            uint16_t code;
            const char *message;
        } error;
    };
};

DICEY_EXPORT enum dicey_error dicey_message_builder_set_value(
    struct dicey_message_builder *builder,
    struct dicey_arg value
);

struct dicey_value_builder {
    int _state;

    // TODO: improve efficency by caching these values
    struct dicey_arg *_root;

    struct _dicey_value_builder_list {
        enum dicey_type type;
        uint16_t nitems;
        size_t cap;
        struct dicey_arg *elems;
    } _list;
};

DICEY_EXPORT enum dicey_error dicey_message_builder_value_start(
    struct dicey_message_builder *builder,
    struct dicey_value_builder *value
);

DICEY_EXPORT enum dicey_error dicey_message_builder_value_end(
    struct dicey_message_builder *builder,
    struct dicey_value_builder *value
);

DICEY_EXPORT enum dicey_error dicey_value_builder_array_start(struct dicey_value_builder *builder, enum dicey_type type);

DICEY_EXPORT enum dicey_error dicey_value_builder_array_end(struct dicey_value_builder *builder);

DICEY_EXPORT enum dicey_error dicey_value_builder_next(
    struct dicey_value_builder *list,
    struct dicey_value_builder *elem
);


DICEY_EXPORT enum dicey_error dicey_value_builder_set(
    struct dicey_value_builder *builder,
    struct dicey_arg value
);

DICEY_EXPORT enum dicey_error dicey_value_builder_tuple_start(struct dicey_value_builder *builder);
DICEY_EXPORT enum dicey_error dicey_value_builder_tuple_end(struct dicey_value_builder *builder);

#if defined(__cplusplus)
}
#endif


#endif // FJWTVTVLMM_BUILDERS_H
