from libc.stdint cimport uint8_t, uint16_t, uint32_t

from .errors cimport dicey_error
from .packet cimport dicey_packet, dicey_op
from .type   cimport dicey_type, \
                     dicey_byte, dicey_bool, dicey_float, \
                     dicey_i16, dicey_i32, dicey_i64, \
                     dicey_u16, dicey_u32, dicey_u64, \
                     dicey_selector

cdef extern from "dicey/dicey.h":

    cdef struct dicey_array_arg:
        dicey_type type
        uint16_t nitems
        const dicey_arg *elems

    cdef struct dicey_bytes_arg:
        uint32_t len
        const uint8_t *data
    
    cdef struct dicey_error_arg:
        uint16_t code
        const char *message
    
    cdef struct dicey_pair_arg:
        const dicey_arg *first
        const dicey_arg *second
        
    cdef struct dicey_tuple_arg:
        uint16_t nitems
        const dicey_arg *elems

    cdef struct dicey_arg:
        dicey_type type

        # the anonymous union is flattened here, Cython does not care about it
        dicey_bool boolean
        dicey_byte byte
        dicey_float floating
        dicey_i16 i16
        dicey_i32 i32
        dicey_i64 i64
        dicey_u16 u16
        dicey_u32 u32
        dicey_u64 u64

        dicey_array_arg array
        dicey_tuple_arg tuple
        dicey_pair_arg pair

        dicey_bytes_arg bytes

        const char *str
        dicey_selector selector

        dicey_error_arg error

    cdef struct dicey_message_builder:
        pass

    cdef struct dicey_value_builder:
        pass

    dicey_error dicey_message_builder_init(dicey_message_builder *builder)
    dicey_error dicey_message_builder_begin(dicey_message_builder *builder, dicey_op op)
    dicey_error dicey_message_builder_build(dicey_message_builder *builder, dicey_packet *packet)
    void dicey_message_builder_discard(dicey_message_builder *builder)
    dicey_error dicey_message_builder_set_path(dicey_message_builder *builder, const char *path)
    dicey_error dicey_message_builder_set_selector(dicey_message_builder *builder, dicey_selector selector)
    dicey_error dicey_message_builder_set_seq(dicey_message_builder *builder, uint32_t seq)
    dicey_error dicey_message_builder_set_value(dicey_message_builder *builder, dicey_arg value)

    dicey_error dicey_message_builder_value_start(
        dicey_message_builder *builder,
        dicey_value_builder *value
    )

    dicey_error dicey_message_builder_value_end(
        dicey_message_builder *builder,
        dicey_value_builder *value
    )

    dicey_error dicey_value_builder_array_start(
        dicey_value_builder *builder,
        dicey_type type
    )

    dicey_error dicey_value_builder_array_end(dicey_value_builder *builder)
    
    dicey_error dicey_value_builder_next(
        dicey_value_builder *list,
        dicey_value_builder *elem
    )

    dicey_error dicey_value_builder_pair_start(dicey_value_builder *builder)
    dicey_error dicey_value_builder_pair_end(dicey_value_builder *builder)
    dicey_error dicey_value_builder_set(dicey_value_builder *builder, dicey_arg value)
    dicey_error dicey_value_builder_tuple_start(dicey_value_builder *builder)
    dicey_error dicey_value_builder_tuple_end(dicey_value_builder *builder)

    dicey_error dicey_packet_message(
        dicey_packet *dest,
        uint32_t seq,
        dicey_op op,
        const char *path,
        dicey_selector selector,
        dicey_arg value
    )