#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <dicey/errors.h>
#include <dicey/packet.h>

#include "typedescr.h"

#define ARRAY_END ']'
#define PAIR_END '}'
#define TUPLE_END ')'

static bool is_valid_type(const int ch) {
    return dicey_type_is_valid(ch) || ch == DICEY_VARIANT_ID;
}

static int take_one(const char **const cur, const char *const end) {
    assert(cur && *cur && end);

    if (*cur == end) {
        return '\0';
    }

    return *(*cur)++;
}

static bool parse_type(const char **begin, const char *end);

static bool parse_array(const char **const cur, const char *const end) {
    assert(cur && end);

    if (!parse_type(cur, end)) {
        return false;
    }

    return take_one(cur, end) == ARRAY_END;
}

static bool parse_pair(const char **const cur, const char *const end) {
    assert(cur && end);

    if (!parse_type(cur, end)) {
        return false;
    }

    if (!parse_type(cur, end)) {
        return false;
    }

    return take_one(cur, end) == PAIR_END;
}

static bool parse_tuple(const char **const cur, const char *const end) {
    assert(cur && end);

    while (parse_type(cur, end)) {
        if (*cur == end) {
            return false;
        }

        if (**cur == TUPLE_END) {
            take_one(cur, end);

            return true;
        }
    }

    return false;
}

static bool parse_type(const char **const cur, const char *const end) {
    assert(cur && *cur && end);

    if (*cur == end) {
        return cur;
    }

    const int first = take_one(cur, end);

    switch (first) {
    default:
        return is_valid_type(first);

    case DICEY_TYPE_ARRAY:
        return parse_array(cur, end);

    case DICEY_TYPE_PAIR:
        return parse_pair(cur, end);

    case DICEY_TYPE_TUPLE:
        return parse_tuple(cur, end);
    }
}

bool dicey_typedescr_is_valid(const char *typedescr) {
    assert(typedescr);

    const char *const end = typedescr + strlen(typedescr);

    return parse_type(&typedescr, end) && typedescr == end;
}
