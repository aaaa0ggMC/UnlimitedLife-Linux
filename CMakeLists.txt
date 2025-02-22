 cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)

set(custom_include_dir "" CACHE PATH "User customed search include path")
set(custom_lib_dir "" CACHE PATH "User customed search lib path")

aux_source_directory("./aaaa0ggmcLib/src" ALIB_SRC)
aux_source_directory("./aaaa0ggmcLib-g4/src" ALIB4_SRC)

include_directories(./CDep/headers)
include_directories(${custom_include_dir})
link_directories(${custom_lib_dir})

project(aaaa0ggmcLib) 
add_library(aaaa0ggmcLib SHARED ${ALIB_SRC})
target_link_directories(aaaa0ggmcLib PRIVATE rapidjson)
target_compile_options(aaaa0ggmcLib PUBLIC "-Wl,--allow-multiple-definition")
target_compile_definitions(aaaa0ggmcLib PUBLIC BUILD_DLL)

project(alib-g4)
add_library(aaaa0ggmcLib-g4 SHARED ${ALIB4_SRC})
target_compile_definitions(aaaa0ggmcLib-g4 PUBLIC ALIB4_BUILD_DLL)

