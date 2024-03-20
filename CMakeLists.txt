cmake_minimum_required(VERSION 3.26.0)

project(dicey VERSION 0.0.1 LANGUAGES C)

if (NOT CMAKE_C_BYTE_ORDER STREQUAL "LITTLE_ENDIAN")
    message(FATAL_ERROR "Big endian is not supported")
endif()

set(DICEY_ROOT ${CMAKE_CURRENT_LIST_DIR})

include(GenerateExportHeader)

option(BUILD_DOC "Build documentation" OFF)
option(BUILD_SAMPLES "Build samples" ON)

if (CMAKE_CROSSCOMPILING AND NOT CMAKE_SYSROOT)
    set(RAW_CROSS ON)
    message(STATUS "Cross compiling without a sysroot - switching to internal libraries")
endif()

set(CMAKE_C_STANDARD 17)

set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(BUILD_LIBXML2 "Always build libxml2 from source" OFF)
option(BUILD_UV "Always build libuv from source" OFF)

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
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic -Werror)
endif()

if (NOT BUILD_UV)
    if (DEFINED UV_INCLUDE_DIRS AND DEFINED UV_LIBS)
        target_include_directories(${PROJECT_NAME} PUBLIC ${UV_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME} PUBLIC ${UV_LIBS})
        set(LibUV_FOUND TRUE)
    elseif (NOT DEFINED RAW_CROSS)
        find_package(PkgConfig QUIET)

        if (PkgConfig_FOUND)
            pkg_check_modules(LibUV QUIET IMPORTED_TARGET libuv)

            if (LibUV_FOUND)
                target_link_libraries(${PROJECT_NAME} PUBLIC PkgConfig::LibUV)
            endif()
        endif()
    endif()
endif()

if (NOT LibUV_FOUND)
    include(FetchContent)

    option(LIBUV_BUILD_SHARED ${BUILD_SHARED_LIBS})
    option(LIBUV_BUILD_TESTS OFF)    

    FetchContent_Declare(
        libuv
        GIT_REPOSITORY https://github.com/libuv/libuv.git
        GIT_TAG        v1.48.0
    )

    FetchContent_MakeAvailable(libuv)

    if (BUILD_SHARED_LIBS)
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
        GIT_TAG        v2.12.5
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
    "include/dicey/core/value.h"
    "include/dicey/core/version.h"
    "include/dicey/core/views.h"

    # ipc
    "include/dicey/ipc/address.h"
    "include/dicey/ipc/client.h"
    "include/dicey/ipc/registry.h"
    "include/dicey/ipc/server.h"
    "include/dicey/ipc/traits.h"
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
    src/sup/assume.h
    src/sup/hashset.c
    src/sup/hashtable.c
    src/sup/trace.c
    src/sup/trace.h
    src/sup/unsafe.c
    src/sup/unsafe.h
    src/sup/util.c
    src/sup/util.h
    src/sup/view-ops.c
    src/sup/view-ops.h

    # ipc
    src/ipc/address.c
    src/ipc/chunk.c
    src/ipc/chunk.h
    src/ipc/queue.c
    src/ipc/queue.h
    src/ipc/typedescr.c
    src/ipc/typedescr.h
    src/ipc/uvtools.c
    src/ipc/uvtools.h
    
    # ipc/client
    src/ipc/client/client.c
    src/ipc/client/waiting-list.c
    src/ipc/client/waiting-list.h
    
    # ipc/server
    src/ipc/server/client-data.c
    src/ipc/server/client-data.h
    src/ipc/server/registry.c
    src/ipc/server/server.c
    src/ipc/server/traits.c

    # ipc/tasks
    src/ipc/tasks/io.c
    src/ipc/tasks/io.h
    src/ipc/tasks/list.c
    src/ipc/tasks/list.h
    src/ipc/tasks/loop.c
    src/ipc/tasks/loop.h
)

if (WIN32)
    list(APPEND CORE_SOURCES src/sup/asprintf.c)
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

if(UNIX)
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

    
install(TARGETS dicey FILE_SET headers)
install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}_export.h" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

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
