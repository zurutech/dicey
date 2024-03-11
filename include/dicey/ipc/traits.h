// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(GFJABYMEEM_TRAITS_H)
#define GFJABYMEEM_TRAITS_H

#include "../core/errors.h"
#include "../core/hashtable.h"

#include "dicey_export.h"

#if defined(__cplusplus)
extern "C" {
#endif

enum dicey_element_type {
    DICEY_ELEMENT_TYPE_INVALID,

    DICEY_ELEMENT_TYPE_OPERATION,
    DICEY_ELEMENT_TYPE_PROPERTY,
    DICEY_ELEMENT_TYPE_SIGNAL,
};

struct dicey_element {
    enum dicey_element_type type;

    const char *signature;
};

struct dicey_element_entry {
    enum dicey_element_type type;

    const char *name;
    const char *signature;
};

struct dicey_trait_iter {
    struct dicey_hashtable_iter _inner;
};

struct dicey_trait {
    const char *name;

    struct dicey_hashtable *elems;
};

DICEY_EXPORT struct dicey_trait_iter dicey_trait_iter_start(struct dicey_trait *trait);
DICEY_EXPORT bool dicey_trait_iter_next(
    struct dicey_trait_iter *iter,
    const char **elem_name,
    struct dicey_element *elem
);

DICEY_EXPORT void dicey_trait_deinit(struct dicey_trait *trait);
DICEY_EXPORT bool dicey_trait_init(struct dicey_trait *trait, const char *name);
DICEY_EXPORT enum dicey_error dicey_trait_init_with(struct dicey_trait *trait, const char *name, ...);
DICEY_EXPORT enum dicey_error dicey_trait_init_with_list(
    struct dicey_trait *trait,
    const char *name,
    const struct dicey_element_entry *elems,
    size_t count
);

DICEY_EXPORT enum dicey_error dicey_trait_add_element(
    struct dicey_trait *trait,
    const char *name,
    struct dicey_element elem
);
DICEY_EXPORT bool dicey_trait_contains_element(const struct dicey_trait *trait, const char *name);
DICEY_EXPORT struct dicey_element *dicey_trait_get_element(const struct dicey_trait *trait, const char *name);

#if defined(__cplusplus)
}
#endif

#endif // GFJABYMEEM_TRAITS_H
