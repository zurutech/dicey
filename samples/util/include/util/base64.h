/*
 * Copyright (c) 2024-2025 Zuru Tech HK Limited, All rights reserved.
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

#if !defined(QVHUCEDPLP_BASE64_H)
#define QVHUCEDPLP_BASE64_H

#include <stddef.h>
#include <stdint.h>

uint8_t *util_base64_decode(const char *src, size_t len, size_t *out_len);
char *util_base64_encode(const uint8_t *src, size_t len, size_t *out_len);

#endif // QVHUCEDPLP_BASE64_H
