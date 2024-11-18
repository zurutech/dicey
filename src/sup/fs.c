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

#include <assert.h>
#include <stdbool.h>

#include <uv.h>

#include <dicey/core/errors.h>

#include "fs.h"
#include "uvtools.h"

enum dicey_error dicey_fs_is_dir(uv_loop_t *const loop, const char *const path) {
    uv_fs_t req = { 0 };

    enum dicey_error err = dicey_error_from_uv(uv_fs_opendir(loop, &req, path, NULL));
    if (err) {
        return err;
    }

    uv_dir_t *const dir = req.ptr;

    req = (uv_fs_t) { 0 };

    err = dicey_error_from_uv(uv_fs_closedir(loop, &req, dir, NULL));
    assert(!err); // there's no reason whatsoever for this to fail

    return err;
}
