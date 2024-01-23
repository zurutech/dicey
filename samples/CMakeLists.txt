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
