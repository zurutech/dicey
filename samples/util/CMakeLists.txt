file(GLOB utils_SRC
    "include/*.h"
    "src/*.c"
    "src/*.h"
)

set(CMAKE_C_STANDARD 17)

set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(samples_util STATIC)

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

    FetchContent_Declare(
        cJSON
        GIT_REPOSITORY https://github.com/DaveGamble/cJSON
        GIT_TAG        v1.7.17
        CMAKE_ARGS 
            -DBUILD_SHARED_LIBS=OFF # completely ignored, for some arcane reason
            -DENABLE_CJSON_TEST=OFF
            -DENABLE_CJSON_UNINSTALL=OFF
    )

    FetchContent_MakeAvailable(cJSON)

    set(BUILD_SHARED_LIBS ${OLD_SHARED_STATE})

    # guess what? A HACK! Who would have thought? Hacks and CMake go together like peanut butter and jelly
    # The reason for this is that cJSON has a borked CMakeLists.txt that makes a mess with the includes
    target_include_directories(samples_util PUBLIC ${cJSON_SOURCE_DIR})
    target_link_libraries(samples_util PUBLIC cjson)
endif()

target_sources(samples_util PRIVATE ${utils_SRC})

target_include_directories(samples_util
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(samples_util PUBLIC "${PROJECT_NAME}")
