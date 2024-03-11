// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#if !defined(GDHHDHCBCM_TRAITS_H)
#define GDHHDHCBCM_TRAITS_H

#include <dicey/core/errors.h>
#include <dicey/core/hashtable.h>
#include <dicey/ipc/traits.h>

void dicey_trait_delete(struct dicey_trait *trait);
struct dicey_trait *dicey_trait_new(const char *name);

enum dicey_error dicey_trait_add_element(struct dicey_trait *trait, const char *name, struct dicey_element elem);
bool dicey_trait_contains_element(const struct dicey_trait *trait, const char *name);
struct dicey_element *dicey_trait_get_element(const struct dicey_trait *trait, const char *name);

#endif // GDHHDHCBCM_TRAITS_H
