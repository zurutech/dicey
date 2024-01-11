#if !defined(XYDQQUJZAI_PACKET_H)
#define XYDQQUJZAI_PACKET_H

#include "types.h"

#if defined (__cplusplus)
extern "C" {
#endif

enum dicey_packet_kind {
    DICEY_PACKET_KIND_HELLO,
    DICEY_PACKET_KIND_BYE,
    DICEY_PACKET_KIND_MESSAGE,
};

struct dicey_packet_builder;

void dicey_packet_builder_init(struct dicey_packet_builder *builder, enum dicey_packet_kind kind);
void dicey_packet_builder_add_path(struct dicey_packet_builder *builder, struct dicey_view path);
void dicey_packet_builder_destroy(struct dicey_packet_builder *builder);

#if defined(__cplusplus)
}
#endif


#endif // XYDQQUJZAI_PACKET_H
