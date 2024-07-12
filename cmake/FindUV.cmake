# Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

find_package(PkgConfig QUIET)

if (PkgConfig_FOUND)
    pkg_check_modules(uv QUIET libuv>=0.48.0)

    if (uv_FOUND)
        find_library(uv_LIBRARY NAMES uv libuv HINTS ${uv_LIBRARY_DIRS})

        set(UV_FOUND "${uv_FOUND}")
        add_library(uv STATIC IMPORTED)
        set_target_properties(uv PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${uv_LIBRARY}"
            IMPORTED_IMPLIB "${uv_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${uv_INCLUDE_DIRS}"
        )

        mark_as_advanced(uv_LIBRARY uv_INCLUDE_DIRS)
    endif()
endif()
