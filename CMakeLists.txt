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

cmake_minimum_required(VERSION 3.26.0)

project(dicey VERSION 0.3.9 LANGUAGES C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(JoinPaths)

if (NOT CMAKE_C_BYTE_ORDER STREQUAL "LITTLE_ENDIAN")
    message(FATAL_ERROR "Big endian is not supported")
endif()

set(DICEY_ROOT ${CMAKE_CURRENT_LIST_DIR})
string(TOUPPER ${CMAKE_SYSTEM_NAME} DICEY_OS)
string(TOUPPER ${CMAKE_C_COMPILER_ID} DICEY_COMPILER)

include(GenerateExportHeader)

if (CMAKE_CROSSCOMPILING AND NOT CMAKE_SYSROOT)
    set(RAW_CROSS ON)
    message(STATUS "Cross compiling without a sysroot - switching to internal libraries")
endif()

set(CMAKE_C_STANDARD 17)

set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_DOC "Build documentation" OFF)
option(BUILD_SAMPLES "Build samples" ON)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(BUILD_LIBXML2 "Always build libxml2 from source" OFF)
option(BUILD_UV "Always build libuv from source" OFF)
option(GENERATE_EXPORTS "Generate exports for PkgConfig and CMake (for systemwide installation)" OFF)
option(PLUGIN_SUPPORT "Enable plugin support" ON)
option(USE_VENDORED_LIBS "Use vendored libraries (implies BUILD_LIBXML2 and BUILD_UV)" OFF)

if (PLUGIN_SUPPORT)
    set (DICEY_HAS_PLUGINS 1)
endif()

if (USE_VENDORED_LIBS)
    set(BUILD_LIBXML2 ON)
    set(BUILD_UV ON)
endif()

add_library(${PROJECT_NAME})

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)

    include(CheckCSourceCompiles)
    set(C11_ATOMICS_TEST "int main(void) { _Atomic int x = 2; x += 1; return x; }")

    check_c_source_compiles("${C11_ATOMICS_TEST}" DICEY_MSVC_HAS_C11_ATOMICS)

    if (NOT DICEY_MSVC_HAS_C11_ATOMICS)
        set(CMAKE_REQUIRED_FLAGS /experimental:c11atomics)

        check_c_source_compiles("${C11_ATOMICS_TEST}" DICEY_MSVC_HAS_C11_EXPERIMENTAL_ATOMICS)

        if (DICEY_MSVC_HAS_C11_EXPERIMENTAL_ATOMICS)
            target_compile_options(${PROJECT_NAME} PRIVATE /experimental:c11atomics)
        else()
            message(FATAL_ERROR "MSVC is too old - no C11 atomics support")
        endif()
    endif()
elseif(NOT MINGW)
    # On MinGW there are wonky warnings around libuv that cause pedantic and Werror to fail randomly.

    list(APPEND IGNORED_WARNINGS
        -Wno-gnu-statement-expression # these are useful for container_of and the like
    )
    
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic -Werror ${IGNORED_WARNINGS})
endif()

if (NOT BUILD_UV)
    if (DEFINED UV_INCLUDE_DIRS AND DEFINED UV_LIBS)
        target_include_directories(${PROJECT_NAME} PUBLIC ${UV_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME} PUBLIC ${UV_LIBS})
        set(UV_FOUND TRUE)
    elseif (NOT DEFINED RAW_CROSS)
        find_package(UV QUIET)
        if (UV_FOUND)
            target_link_libraries(${PROJECT_NAME} PUBLIC uv)
        endif()
    endif()
endif()

if (NOT UV_FOUND)
    include(FetchContent)

    # I have no tricks left in my bag, so I'll do this the stupid way
    set(LIBUV_BUILD_SHARED ${BUILD_SHARED_LIBS} CACHE BOOL "Build libuv as a shared library" FORCE)
    set(LIBUV_BUILD_TESTS OFF CACHE BOOL "Do not build libuv tests" FORCE)

    FetchContent_Declare(
        libuv
        GIT_REPOSITORY https://github.com/libuv/libuv.git
        GIT_TAG        v1.49.2
    )

    FetchContent_MakeAvailable(libuv)

    if (LIBUV_BUILD_SHARED)
        target_link_libraries(${PROJECT_NAME} PUBLIC uv)
    else()
        target_link_libraries(${PROJECT_NAME} PUBLIC uv_a)
    endif()
endif()

if (NOT BUILD_LIBXML2)
    find_package(LibXml2 QUIET)
    if (LibXml2_FOUND)
        target_link_libraries(${PROJECT_NAME} PUBLIC LibXml2::LibXml2)
    endif()
endif()

if (NOT LibXml2_FOUND)
    include(FetchContent)

    set(LIBXML2_WITH_ICONV OFF)
    set(LIBXML2_WITH_ICU OFF)
    set(LIBXML2_WITH_LZMA OFF)
    set(LIBXML2_WITH_PYTHON OFF)
    set(LIBXML2_WITH_SCHEMAS ON)
    set(LIBXML2_WITH_TESTS OFF)
    set(LIBXML2_WITH_THREADS ON)
    set(LIBXML2_WITH_ZLIB OFF)

    FetchContent_Declare(
        libxml2
        GIT_REPOSITORY https://gitlab.gnome.org/GNOME/libxml2.git
        GIT_TAG        v2.13.4
    )

    FetchContent_MakeAvailable(libxml2)

    target_link_libraries(${PROJECT_NAME} PUBLIC LibXml2::LibXml2)
endif()

if (NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    if (WIN32)
        target_link_libraries(${PROJECT_NAME} PUBLIC dbghelp)
    endif()

    if (DEFINED BSD)
	target_link_libraries(${PROJECT_NAME} PUBLIC execinfo)
    endif()
endif()

generate_export_header(${PROJECT_NAME})

set(CONFIG_HEADER_NAME "dicey_config.h")
set(CONFIG_HEADER "${PROJECT_BINARY_DIR}/${CONFIG_HEADER_NAME}")

# generate config.h
configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/${CONFIG_HEADER_NAME}.in"
    "${CONFIG_HEADER}"
)

set(PUBLIC_HEADERS
    "include/dicey/dicey.h"

    
    # internal but public

    # core
    "include/dicey/core/builders.h"
    "include/dicey/core/data-info.h"
    "include/dicey/core/errors.h"
    "include/dicey/core/hashset.h"
    "include/dicey/core/hashtable.h"
    "include/dicey/core/packet.h"
    "include/dicey/core/type.h"
    "include/dicey/core/typedescr.h"
    "include/dicey/core/value.h"
    "include/dicey/core/version.h"
    "include/dicey/core/views.h"

    # ipc
    "include/dicey/ipc/address.h"
    "include/dicey/ipc/builtins.h"
    "include/dicey/ipc/client.h"
    "include/dicey/ipc/registry.h"
    "include/dicey/ipc/server-api.h"
    "include/dicey/ipc/server.h"
    "include/dicey/ipc/traits.h"

    # ipc/builtins
    "include/dicey/ipc/builtins/introspection.h"
    "include/dicey/ipc/builtins/server.h"
)

set(CORE_SOURCES
    # common
    src/errors.c
    src/version.c

    # wirefmt    
    src/wirefmt/builders.c
    src/wirefmt/packet-args.c
    src/wirefmt/packet-args.h
    src/wirefmt/packet.c
    src/wirefmt/typedescr.c
    src/wirefmt/uuid.c
    src/wirefmt/value-validate.c
    src/wirefmt/value.c

    # wirefmt/dtf
    src/wirefmt/dtf/dtf.h
    src/wirefmt/dtf/payload.c
    src/wirefmt/dtf/payload.h
    src/wirefmt/dtf/to.h
    src/wirefmt/dtf/value-probe.c
    src/wirefmt/dtf/value-write.c
    src/wirefmt/dtf/value.h
    src/wirefmt/dtf/writer.c
    src/wirefmt/dtf/writer.h
    
    # sup    
    src/sup/asprintf.c
    src/sup/asprintf.h
    src/sup/assume.h
    src/sup/hashset.c
    src/sup/hashtable.c
    src/sup/trace.c
    src/sup/trace.h
    src/sup/unsafe.c
    src/sup/unsafe.h
    src/sup/util.c
    src/sup/util.h
    src/sup/uvtools.c
    src/sup/uvtools.h
    src/sup/view-ops.c
    src/sup/view-ops.h

    # ipc
    src/ipc/address.c
    src/ipc/chunk.c
    src/ipc/chunk.h
    src/ipc/elemdescr.c
    src/ipc/elemdescr.h
    src/ipc/queue.c
    src/ipc/queue.h
    
    # ipc/client
    src/ipc/client/client.c
    src/ipc/client/waiting-list.c
    src/ipc/client/waiting-list.h
    
    # ipc/server
    src/ipc/server/client-data.c
    src/ipc/server/client-data.h
    src/ipc/server/pending-reqs.c
    src/ipc/server/pending-reqs.h
    src/ipc/server/registry.c
    src/ipc/server/registry-internal.h
    src/ipc/server/server.c
    src/ipc/server/server.h
    src/ipc/server/server-clients.c
    src/ipc/server/server-clients.h
    src/ipc/server/server-loopreq.c
    src/ipc/server/server-loopreq.h
    src/ipc/server/shared-packet.c
    src/ipc/server/shared-packet.h
    src/ipc/server/traits.c

    # ipc/server/builtins
    src/ipc/server/builtins/builtins.c
    src/ipc/server/builtins/builtins.h

    # ipc/server/builtins/introspection
    src/ipc/server/builtins/introspection/commons.c
    src/ipc/server/builtins/introspection/definitions.c
    src/ipc/server/builtins/introspection/introspection.h
    src/ipc/server/builtins/introspection/introspection-internal.h
    src/ipc/server/builtins/introspection/registry.c
    src/ipc/server/builtins/introspection/traits.c
    src/ipc/server/builtins/introspection/xmlgen.c

    # ipc/server/builtins/server
    src/ipc/server/builtins/server/server.c
    src/ipc/server/builtins/server/server.h

    # ipc/tasks
    src/ipc/tasks/io.c
    src/ipc/tasks/io.h
    src/ipc/tasks/list.c
    src/ipc/tasks/list.h
    src/ipc/tasks/loop.c
    src/ipc/tasks/loop.h
)

set(PLUGIN_SOURCES
    src/ipc/plugin-macros.h

    src/ipc/server/plugins.c
    src/ipc/server/plugins.h

    src/ipc/server/builtins/plugins/plugins.c
    src/ipc/server/builtins/plugins/plugins.h
)

if (PLUGIN_SUPPORT)
    list(APPEND PUBLIC_HEADERS
        "include/dicey/ipc/builtins/plugins.h"
        "include/dicey/ipc/plugins.h"
    )

    list(APPEND CORE_SOURCES ${PLUGIN_SOURCES})
endif()

target_sources(
    ${PROJECT_NAME}
    PRIVATE ${CORE_SOURCES}
    PUBLIC
    FILE_SET headers
    TYPE HEADERS
    BASE_DIRS include
    FILES ${PUBLIC_HEADERS}
)
    
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
    
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
set_target_properties(${PROJECT_NAME} PROPERTIES C_VISIBILITY_PRESET hidden)

if(UNIX)
    if (GNU OR Clang)
        set_target_properties(${PROJECT_NAME} PROPERTIES CMAKE_C_EXTENSIONS ON)
        set_target_properties(${PROJECT_NAME} PROPERTIES CMAKE_VISIBILITY_INLINES_HIDDEN ON)
    endif()

    include(GNUInstallDirs)
else()
    if (WIN32)
        set(${CMAKE_INSTALL_LIBDIR} "lib")
        set(${CMAKE_INSTALL_DATADIR} "share")
        set(${CMAKE_INSTALL_INCLUDEDIR} "include")
        set(${CMAKE_INSTALL_BINDIR} "bin")
        message(STATUS "Setting installation destination on Windows to: ${CMAKE_INSTALL_PREFIX}")
    else()
        message(FATAL_ERROR "System not UNIX nor WIN32 - not implemented yet")
    endif()
endif()

set(targets_export_name dicey-targets)
install(TARGETS dicey EXPORT ${targets_export_name} FILE_SET headers) 
install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}_export.h" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}_config.h" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# this probably only works on Linux/BSD with systemwide libraries - which is what's useful for 99% of the time
if (GENERATE_EXPORTS)
    include(CMakePackageConfigHelpers)

    set(CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/dicey CACHE STRING "Installation directory for cmake files")
    set(version_config ${PROJECT_BINARY_DIR}/dicey-config-version.cmake)
    set(project_config ${PROJECT_BINARY_DIR}/dicey-config.cmake)
    set(pkgconfig ${PROJECT_BINARY_DIR}/dicey.pc)

    set(DICEY_VERSION ${CMAKE_PROJECT_VERSION})
    set(PKGCONFIG_DIR ${CMAKE_INSTALL_LIBDIR}/pkgconfig CACHE STRING "Installation directory for pkgconfig (.pc) files")

    # Generate the version, config and target files into the build directory.
    write_basic_package_version_file(
        ${version_config}
        VERSION ${DICEY_VERSION}
        COMPATIBILITY AnyNewerVersion
    )

    set(DICEY_LIB_NAME ${PROJECT_NAME})

    join_paths(libdir_for_pc_file "\${exec_prefix}" "${CMAKE_INSTALL_LIBDIR}")
    join_paths(includedir_for_pc_file "\${prefix}" "${CMAKE_INSTALL_INCLUDEDIR}")

    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/dicey.pc.in"
        "${pkgconfig}"
        @ONLY
    )

    configure_package_config_file(
        ${PROJECT_SOURCE_DIR}/cmake/dicey-config.cmake.in
        ${project_config}
        INSTALL_DESTINATION ${CMAKE_DIR}
    )

    export(TARGETS dicey NAMESPACE dicey:: FILE ${PROJECT_BINARY_DIR}/${targets_export_name}.cmake)
    set(find_uv "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindUV.cmake") 

    # Install version, config and target files.
    install(
        FILES ${project_config} ${version_config} ${find_uv}
        DESTINATION ${CMAKE_DIR}
    )

    install(EXPORT ${targets_export_name} DESTINATION ${CMAKE_DIR} NAMESPACE dicey::)

    install(FILES "${pkgconfig}" DESTINATION "${PKGCONFIG_DIR}")
endif()

if (BUILD_SAMPLES)
    add_subdirectory(samples)
endif()
    
if (BUILD_DOC)
    find_package(Doxygen)

    if (NOT DOXYGEN_FOUND)
        message(FATAL_ERROR "Doxygen is needed to build the documentation.")
    endif()

    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(doxygen
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        ERBATIM
    )
endif()
