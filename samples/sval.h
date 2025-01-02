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

#if !defined(OCVMKCPQZY_SVAL_H)
#define OCVMKCPQZY_SVAL_H

#include <dicey/dicey.h>

#define SVAL_PATH "/sval"
#define SVAL_TRAIT "sval.Sval"
#define SVAL_PROP "Value"
#define SVAL_SEL                                                                                                       \
    (struct dicey_selector) { .trait = SVAL_TRAIT, .elem = SVAL_PROP }
#define SVAL_SIG "s"

#endif // OCVMKCPQZY_SVAL_H
