cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_COMPILER  "/usr/bin/clang++")
set(CMAKE_C_COMPILER  "/usr/bin/clang")
set(CMAKE_CXX_STANDARD 20)

aux_source_directory("../aaaa0ggmcLib/src" ALIB_SRC)

include_directories(../CDep/headers)

project(aaaa0ggmcLib) 

add_library(aaaa0ggmcLib SHARED ${ALIB_SRC})
target_link_directories(aaaa0ggmcLib PRIVATE rapidjson)

