/*
 * Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if !defined(CNHZVJKDMF_DTF_VALUE_H)
#define CNHZVJKDMF_DTF_VALUE_H

#if defined(__cplusplus)
#error "This header is not meant to be included from C++"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <dicey/core/builders.h>
#include <dicey/core/packet.h>
#include <dicey/core/value.h>
#include <dicey/core/views.h>

#include "to.h"
#include "writer.h"

#define DTF_SIZE_DYNAMIC PTRDIFF_MAX

struct dtf_probed_value {
    enum dicey_type type;
    union _dicey_data_info data;
};

ptrdiff_t dtf_selector_from(struct dicey_selector *sel, struct dicey_view *src);
ptrdiff_t dtf_selector_write(struct dicey_selector sel, struct dicey_view_mut *dest);

struct dtf_valueres {
    ptrdiff_t result;
    size_t size;
    struct dtf_value *value;
};

ptrdiff_t dtf_value_estimate_size(const struct dicey_arg *item);

ptrdiff_t dtf_value_probe(struct dicey_view *src, struct dtf_probed_value *info);
ptrdiff_t dtf_value_probe_as(enum dicey_type type, struct dicey_view *src, union _dicey_data_info *info);
ptrdiff_t dtf_value_probe_type(struct dicey_view *src);

struct dtf_valueres dtf_value_write(struct dicey_view_mut dest, const struct dicey_arg *item);
ptrdiff_t dtf_value_write_to(struct dtf_bytes_writer *writer, const struct dicey_arg *item);

#endif // CNHZVJKDMF_DTF_VALUE_H
