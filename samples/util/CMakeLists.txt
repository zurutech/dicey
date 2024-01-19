file(GLOB utils_SRC
    "include/*.h"
    "src/*.c"
    "src/*.h"
)

set(CMAKE_C_STANDARD 17)

set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(samples_util STATIC)

target_sources(samples_util PRIVATE ${utils_SRC})

target_include_directories(samples_util
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(samples_util "${PROJECT_NAME}")
