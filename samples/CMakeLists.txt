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

add_subdirectory(util)

set(SAMPLE_FILES base64.c client.c dump.c load.c server.c subtest.c sval.c)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

foreach(SAMPLE IN LISTS SAMPLE_FILES)
    get_filename_component(target_name ${SAMPLE} NAME_WE)
    
    add_executable("${target_name}" "${SAMPLE}")

    if (MSVC)
        target_compile_options("${target_name}" PRIVATE /W4 /WX)
    else()
        target_compile_options("${target_name}" PRIVATE -Wall -Wextra -pedantic -Werror)
    endif()
    
    # add the include directory of the parent project
    target_link_libraries("${target_name}" PRIVATE "${PROJECT_NAME}" samples_util)
endforeach()

# if the compiler is Clang, build fuzz/fuzz.c with -fsanitize=fuzzer
if(BUILD_FUZZER)
    add_executable(fuzz fuzz/fuzz.c)
    target_link_libraries(fuzz PRIVATE "${PROJECT_NAME}" samples_util)

    if (MSVC OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
        target_compile_options(fuzz PRIVATE -fsanitize=fuzzer -fsanitize=undefined -fsanitize=address)
        target_link_libraries(fuzz PRIVATE -fsanitize=fuzzer -fsanitize=undefined -fsanitize=address)
    else()
        message(FATAL_ERROR "Fuzzer is only supported with MSVC or Clang")
    endif()
endif()