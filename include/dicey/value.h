#include <stddef.h>
#if !defined(TOJAFCVDUG_VALUE_H)
#define TOJAFCVDUG_VALUE_H

#include <stdbool.h>
#include <stdint.h>

#include "errors.h"
#include "type.h"
#include "views.h"

#include "internal/data-info.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct dicey_value {
    enum dicey_type _type;

    union _dicey_data_info _data;
};

struct dicey_iterator {
    uint16_t _type;

    struct dicey_view _data;
};

bool dicey_iterator_has_next(struct dicey_iterator iter);
enum dicey_error dicey_iterator_next(struct dicey_iterator *iter, struct dicey_value *dest);

struct dicey_list {
    uint16_t _type;
    uint16_t _nitems;

    struct dicey_view _data;
};

struct dicey_iterator dicey_list_iter(const struct dicey_list *list);
int dicey_list_type(const struct dicey_list *list);

struct dicey_pair {
    struct dicey_value first;
    struct dicey_value second;
};

enum dicey_type dicey_value_get_type(const struct dicey_value *value);

enum dicey_error dicey_value_get_array(const struct dicey_value *value, struct dicey_list *dest);

enum dicey_error dicey_value_get_bool(const struct dicey_value *value, bool *dest);
enum dicey_error dicey_value_get_byte(const struct dicey_value *value, uint8_t *dest);

enum dicey_error dicey_value_get_bytes(const struct dicey_value *value, const uint8_t **dest, size_t *nbytes);

enum dicey_error dicey_value_get_error(const struct dicey_value *value, struct dicey_errmsg *dest);

enum dicey_error dicey_value_get_float(const struct dicey_value *value, double *dest);

enum dicey_error dicey_value_get_i16(const struct dicey_value *value, int16_t *dest);
enum dicey_error dicey_value_get_i32(const struct dicey_value *value, int32_t *dest);
enum dicey_error dicey_value_get_i64(const struct dicey_value *value, int64_t *dest);

enum dicey_error dicey_value_get_pair(const struct dicey_value *value, struct dicey_pair *dest);
enum dicey_error dicey_value_get_path(const struct dicey_value *value, const char **dest);
enum dicey_error dicey_value_get_selector(const struct dicey_value *value, struct dicey_selector *dest);
enum dicey_error dicey_value_get_str(const struct dicey_value *value, const char **dest);
enum dicey_error dicey_value_get_tuple(const struct dicey_value *value, struct dicey_list *dest);

enum dicey_error dicey_value_get_u16(const struct dicey_value *value, uint16_t *dest);
enum dicey_error dicey_value_get_u32(const struct dicey_value *value, uint32_t *dest);
enum dicey_error dicey_value_get_u64(const struct dicey_value *value, uint64_t *dest);

static inline bool dicey_value_is(const struct dicey_value *const value, const enum dicey_type type) {
    return value->_type == type;
}

static inline bool dicey_value_is_valid(const struct dicey_value *const value) {
    return dicey_type_is_valid(value->_type);
}

#if defined(__cplusplus)
}
#endif


#endif // TOJAFCVDUG_VALUE_H
