cmake_minimum_required(VERSION 3.10)
project(UnlimitedLife-Linux)

set(CMAKE_CXX_STANDARD 23)

if (WIN32)
    add_compile_options(-ftls-model=global-dynamic)
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,--allow-multiple-definition -Wl,--rpath,./") 
else()
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,--allow-multiple-definition -Wl,--rpath,./")
endif()

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
add_subdirectory(aaaa0ggmcLib)
add_subdirectory(aaaa0ggmcLib-g4)
add_subdirectory(UnlimitedLife-src)
add_subdirectory(AGE)
