cmake_minimum_required(VERSION 3.23.0)

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

set(PUBLIC_HEADERS
    "include/dicey/builders.h"
    "include/dicey/dicey.h"
    "include/dicey/errors.h"
    "include/dicey/packet.h"
    "include/dicey/type.h"
    "include/dicey/value.h"
    "include/dicey/version.h"
    "include/dicey/views.h"
    "include/dicey/internal/data-info.h"
)

set(CMAKE_C_STANDARD 17)

set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

add_library(${PROJECT_NAME})

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

if (NOT LibUV_FOUND)
    include(FetchContent)

    option(LIBUV_BUILD_TESTS OFF)    

    FetchContent_Declare(
        libuv
        GIT_REPOSITORY https://github.com/libuv/libuv.git
        GIT_TAG        v1.48.0
    )

    FetchContent_MakeAvailable(libuv)

    target_link_libraries(${PROJECT_NAME} PUBLIC uv)
endif()

find_package(LibXml2 QUIET)
if (LibXml2_FOUND)
    target_link_libraries(${PROJECT_NAME} PUBLIC LibXml2::LibXml2)
else()
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
        GIT_TAG        v2.11.7
    )

    FetchContent_MakeAvailable(libxml2)

    target_link_libraries(${PROJECT_NAME} PUBLIC LibXml2::LibXml2)
endif()

if (WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(${PROJECT_NAME} PUBLIC dbghelp)
endif()

if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

generate_export_header(${PROJECT_NAME})

set(CORE_SOURCES
    # common
    src/errors.c

    
    # wirefmt
    
    src/wirefmt/builders.c 
    src/wirefmt/packet-args.c
    src/wirefmt/packet-args.h
    src/wirefmt/packet.c
    src/wirefmt/typedescr.c
    src/wirefmt/typedescr.h
    src/wirefmt/value.c
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

    src/sup/trace.c
    src/sup/trace.h
    src/sup/unsafe.c
    src/sup/unsafe.h
    src/sup/util.c
    src/sup/util.h
    src/sup/view-ops.c
    src/sup/view-ops.h
)

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
    
install(TARGETS dicey FILE_SET headers)

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