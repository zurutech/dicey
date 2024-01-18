#if !defined(FJWTVTVLMM_BUILDERS_H)
#define FJWTVTVLMM_BUILDERS_H

#include <stdint.h>

#include <dicey/packet.h>

#if defined (__cplusplus)
extern "C" {
#endif

struct dicey_message_builder {
    int _state;

    enum dicey_message_type _type;
    uint32_t _seq;

    const char *_path;
    struct dicey_selector _selector;
    struct dicey_arg *_root;

    const struct dicey_value_builder *_borrowed_to;
};

enum dicey_error dicey_message_builder_init(struct dicey_message_builder *builder);

enum dicey_error dicey_message_builder_begin(struct dicey_message_builder *builder, enum dicey_message_type type);
enum dicey_error dicey_message_builder_build(struct dicey_message_builder *builder, struct dicey_packet *packet);
enum dicey_error dicey_message_builder_destroy(struct dicey_message_builder *builder);
void dicey_message_builder_discard(struct dicey_message_builder *builder);
enum dicey_error dicey_message_builder_set_path(struct dicey_message_builder *builder, const char *path);

enum dicey_error dicey_message_builder_set_selector(
    struct dicey_message_builder *builder,
    struct dicey_selector selector
);

enum dicey_error dicey_message_builder_set_seq(struct dicey_message_builder *builder, uint32_t seq);

struct dicey_arg {
    enum dicey_type type;

    union {
        dicey_bool boolean;
        dicey_float floating;
        dicey_int integer;

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

enum dicey_error dicey_message_builder_set_value(
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

enum dicey_error dicey_message_builder_value_start(
    struct dicey_message_builder *builder,
    struct dicey_value_builder *value
);

enum dicey_error dicey_message_builder_value_end(
    struct dicey_message_builder *builder,
    struct dicey_value_builder *value
);

enum dicey_error dicey_value_builder_array_start(struct dicey_value_builder *builder, enum dicey_type type);

enum dicey_error dicey_value_builder_array_end(struct dicey_value_builder *builder);

enum dicey_error dicey_value_builder_next(
    struct dicey_value_builder *list,
    struct dicey_value_builder *elem
);


enum dicey_error dicey_value_builder_set(
    struct dicey_value_builder *builder,
    struct dicey_arg value
);

enum dicey_error dicey_value_builder_tuple_start(struct dicey_value_builder *builder);
enum dicey_error dicey_value_builder_tuple_end(struct dicey_value_builder *builder);

#if defined(__cplusplus)
}
#endif


#endif // FJWTVTVLMM_BUILDERS_H
