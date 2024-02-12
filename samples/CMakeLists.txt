add_subdirectory(util)

set(SAMPLE_FILES base64.c dump.c load.c)

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
if(BUILD_FUZZER AND CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_executable(fuzz fuzz/fuzz.c)
    target_link_libraries(fuzz PRIVATE "${PROJECT_NAME}" samples_util)

    if (WIN32)
        target_compile_options(fuzz PRIVATE -fsanitize=fuzzer)
        target_link_libraries(fuzz PRIVATE -fsanitize=fuzzer)
    else()
        target_compile_options(fuzz PRIVATE -fsanitize=fuzzer,undefined,address)
        target_link_libraries(fuzz PRIVATE -fsanitize=fuzzer,undefined,address)
    endif()
endif()