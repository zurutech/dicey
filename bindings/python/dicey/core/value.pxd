from libc.stdint cimport int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t

from libcpp cimport bool as c_bool

from .errors cimport dicey_error
from .type cimport dicey_errmsg, dicey_selector, dicey_type

cdef extern from "dicey/dicey.h":

    cdef struct dicey_value:
        pass

    cdef struct dicey_iterator:
        pass

    cdef struct dicey_list:
        pass

    cdef struct dicey_pair:
        dicey_value first
        dicey_value second

    c_bool dicey_iterator_has_next(dicey_iterator iter)
    dicey_error dicey_iterator_next(dicey_iterator *iter, dicey_value *dest)

    dicey_iterator dicey_list_iter(const dicey_list *list)
    int dicey_list_type(const dicey_list *list)

    dicey_type dicey_value_get_type(const dicey_value *value)

    dicey_error dicey_value_get_array(const dicey_value *value, dicey_list *dest)
    dicey_error dicey_value_get_bool(const dicey_value *value, c_bool *dest)
    dicey_error dicey_value_get_byte(const dicey_value *value, uint8_t *dest)
    dicey_error dicey_value_get_bytes(const dicey_value *value, const uint8_t **dest, size_t *nbytes)
    dicey_error dicey_value_get_error(const dicey_value *value, dicey_errmsg *dest)
    dicey_error dicey_value_get_float(const dicey_value *value, double *dest)
    dicey_error dicey_value_get_i16(const dicey_value *value, int16_t *dest)
    dicey_error dicey_value_get_i32(const dicey_value *value, int32_t *dest)
    dicey_error dicey_value_get_i64(const dicey_value *value, int64_t *dest)
    dicey_error dicey_value_get_pair(const dicey_value *value, dicey_pair *dest)
    dicey_error dicey_value_get_path(const dicey_value *value, char **dest)
    dicey_error dicey_value_get_selector(const dicey_value *value, dicey_selector *dest)
    dicey_error dicey_value_get_str(const dicey_value *value, char **dest)
    dicey_error dicey_value_get_tuple(const dicey_value *value, dicey_list *dest)
    dicey_error dicey_value_get_u16(const dicey_value *value, uint16_t *dest)
    dicey_error dicey_value_get_u32(const dicey_value *value, uint32_t *dest)
    dicey_error dicey_value_get_u64(const dicey_value *value, uint64_t *dest)

    c_bool dicey_value_is(const dicey_value *value, dicey_type type)
    c_bool dicey_value_is_valid(const dicey_value *value)

cdef pythonize_value(const dicey_value *value, object value_hook=*, type array_cls=*, bint pair_lists_as_dict=*)