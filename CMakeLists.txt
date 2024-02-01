cmake_minimum_required(VERSION 3.23.0)

project(dicey VERSION 0.0.1 LANGUAGES C)


include(GenerateExportHeader)

if (NOT CMAKE_C_BYTE_ORDER STREQUAL "LITTLE_ENDIAN")
message(FATAL_ERROR "Big endian is not supported")
endif()

option(BUILD_DOC "Build documentation" OFF)

if (MSVC)
add_compile_options(/W4 /WX)
else()
add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

set(DICEY_ROOT ${CMAKE_CURRENT_LIST_DIR})

file(GLOB public_HEADERS
"include/dicey/*.h"
"include/dicey/internal/*.h"
$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>/*.h
)

file(GLOB common_SRC
"src/*.c"
"src/*.h"
"src/dtf/*.c"
"src/dtf/*.h"
)

set(CMAKE_C_STANDARD 17)

set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

add_library(${PROJECT_NAME})
generate_export_header(${PROJECT_NAME})

target_sources(
    ${PROJECT_NAME}
    PRIVATE ${common_SRC}
    PUBLIC
    FILE_SET headers
    TYPE HEADERS
    BASE_DIRS include
    FILES ${public_HEADERS}
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
    
add_subdirectory(samples)
    
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