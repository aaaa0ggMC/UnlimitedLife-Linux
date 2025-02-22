cmake_minimum_required(VERSION 3.10)
project(UnlimitedLife-Linux)

set(CMAKE_CXX_STANDARD 20)

set(custom_include_dir "" CACHE PATH "User customed search include path")
set(custom_lib_dir "" CACHE PATH "User customed search lib path")

if(UNIX)
    set(build_dir "${CMAKE_SOURCE_DIR}/CBuild" CACHE PATH "Where to store the built bins.")
else()
    set(build_dir "${CMAKE_SOURCE_DIR}/CBuild/${CMAKE_SYSTEM_NAME}" CACHE PATH "Where to store the built bin.")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${build_dir})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${build_dir})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${build_dir})

include_directories(./CDep/headers)
include_directories(${custom_include_dir})
link_directories(${custom_lib_dir})

macro(set_output_directory target)
    if(TARGET ${target})
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${build_dir}"
            LIBRARY_OUTPUT_DIRECTORY "${build_dir}"
            ARCHIVE_OUTPUT_DIRECTORY "${build_dir}"
        )
        # 处理多配置生成器（如Visual Studio的Debug/Release）
        foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
            string(TOUPPER ${config} config_upper)
            set_target_properties(${target} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${build_dir}"
                LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${build_dir}"
                ARCHIVE_OUTPUT_DIRECTORY_${config_upper} "${build_dir}"
            )
        endforeach()
    endif()
endmacro()

add_subdirectory(aaaa0ggmcLib)
add_subdirectory(aaaa0ggmcLib-g4)

set_output_directory(aaaa0ggmcLib)
set_output_directory(aaaa0ggmcLib-g4)
