add_subdirectory(util)

file(GLOB samples_SRC
    "*.c"
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

foreach(sample IN LISTS samples_SRC)
    get_filename_component(target_name ${sample} NAME_WE)
    
    add_executable("${target_name}" "${sample}")
    
    # add the include directory of the parent project
    target_link_libraries("${target_name}" PRIVATE "${PROJECT_NAME}" samples_util)
endforeach()

# if the compiler is Clang, build fuzz/fuzz.c with -fsanitize=fuzzer
if(BUILD_FUZZER AND CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_executable(fuzz fuzz/fuzz.c)
    target_link_libraries(fuzz PRIVATE "${PROJECT_NAME}" samples_util)
    target_compile_options(fuzz PRIVATE -fsanitize=fuzzer,undefined,address)
    target_link_libraries(fuzz PRIVATE -fsanitize=fuzzer,undefined,address)
endif()