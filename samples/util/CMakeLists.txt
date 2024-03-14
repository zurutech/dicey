set(utils_INCLUDES
    "include/util/base64.h"
    "include/util/dumper.h"
    "include/util/getopt.h"
    "include/util/packet-dump.h"
    "include/util/packet-json.h"
    "include/util/packet-xml.h"
    "include/util/strext.h"
)

set(utils_SRC
    "src/base64.c"
    "src/dumper.c"
    "src/packet-dump.c"
    "src/packet-from-json.c"
    "src/packet-from-xml.c"
)

if (WIN32)
    list(APPEND utils_SRC "src/getopt.c")
endif()

function(embed_resources target)
    set(script_path "${CMAKE_CURRENT_BINARY_DIR}/anything_to_c.cmake")

    file(WRITE  "${script_path}" "file(READ \${CMAKE_ARGV3} buf HEX)\n")
    file(APPEND "${script_path}" "string(REGEX REPLACE \"([0-9a-f][0-9a-f])\" \"0x\\\\1, \" buf \${buf})\n")
    file(APPEND "${script_path}" "file(WRITE \${CMAKE_ARGV4} \"const unsigned char \${CMAKE_ARGV5}[] = { \${buf}0x00 };\\n\")\n")
    file(APPEND "${script_path}" "file(APPEND \${CMAKE_ARGV4} \"const unsigned long long \${CMAKE_ARGV6} = sizeof(\${CMAKE_ARGV5}) - 1;\\n\")\n")

    foreach(res_path ${ARGN})
        file(RELATIVE_PATH res_rel ${DICEY_ROOT} ${res_path})
        string(MAKE_C_IDENTIFIER ${res_rel} identifier)

        set(src_path "${res_path}")
        set(dst_path "${CMAKE_CURRENT_BINARY_DIR}/${identifier}.c")

        set(anything_to_c ${CMAKE_COMMAND} -P ${script_path} ${src_path} ${dst_path} ${identifier} ${identifier}_size)

        add_custom_command(OUTPUT ${dst_path} COMMAND ${anything_to_c} DEPENDS ${src_path} VERBATIM)

        target_sources(${target} PRIVATE ${src_path} ${dst_path} )
    endforeach()
endfunction()

set(CMAKE_C_STANDARD 17)

set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(samples_util STATIC)

if (MSVC)
    target_compile_options(samples_util PRIVATE /W4 /WX)
else()
    target_compile_options(samples_util PRIVATE -Wall -Wextra -pedantic -Werror)
endif()

if (DEFINED CJSON_DIRS AND DEFINED CJSON_LIBS)
    target_include_directories(samples_util PUBLIC ${CJSON_INCLUDE_DIRS})
    target_link_libraries(samples_util PUBLIC ${CJSON_LIBS})
    set(cJSON_FOUND TRUE)
elseif (NOT DEFINED RAW_CROSS) # set above in the root CMakeLists.txt
    find_package(PkgConfig QUIET)

    if (PkgConfig_FOUND)
        pkg_check_modules(cJSON QUIET IMPORTED_TARGET libcjson)

        if (cJSON_FOUND)
            target_link_libraries(samples_util PUBLIC PkgConfig::cJSON)
        endif()
    endif()
endif()

if (NOT cJSON_FOUND)
    include(FetchContent)

    # if someone knows how to do this without changing the global state, please let me know
    set(OLD_SHARED_STATE ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF)
    set(ENABLE_CJSON_UNINSTALL CACHE BOOL OFF)
    set(ENABLE_CUSTOM_COMPILER_FLAGS CACHE BOOL OFF)
    set(ENABLE_CJSON_TEST CACHE BOOL OFF)
    
    FetchContent_Declare(
        cJSON
        GIT_REPOSITORY https://github.com/DaveGamble/cJSON
        GIT_TAG        v1.7.17
    )

    FetchContent_MakeAvailable(cJSON)

    set(BUILD_SHARED_LIBS ${OLD_SHARED_STATE})

    # guess what? A HACK! Who would have thought? Hacks and CMake go together like peanut butter and jelly
    # The reason for this is that cJSON has a borked CMakeLists.txt that makes a mess with the includes
    target_include_directories(samples_util PUBLIC ${cJSON_SOURCE_DIR})
    target_link_libraries(samples_util PUBLIC cjson)
endif()

target_sources(samples_util
    PRIVATE ${utils_SRC}
    PUBLIC 
    FILE_SET headers
    TYPE HEADERS
    BASE_DIRS include
    FILES ${utils_HEADERS}
)

embed_resources(samples_util
    "${DICEY_ROOT}/schemas/packet.xsd"
)

target_include_directories(samples_util
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(samples_util PUBLIC "${PROJECT_NAME}")
