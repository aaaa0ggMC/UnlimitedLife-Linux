cmake_minimum_required(VERSION 3.10)
project(UnlimitedLife-Linux)

add_definitions(-DGLM_ENABLE_EXPERIMENTAL)

set(CMAKE_CXX_STANDARD 26)

if (WIN32)
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

if(CMAKE_BUILD_TYPE STREQUAL "Release")
add_compile_options("-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=UnlimitedLife-Linux")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${build_dir})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${build_dir})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${build_dir})
add_compile_options(-fno-omit-frame-pointer -g)

include_directories(./CDep/headers)
include_directories(${custom_include_dir})
link_directories(${custom_lib_dir})
link_directories(${build_dir})
add_subdirectory(aaaa0ggmcLib)
add_subdirectory(UnlimitedLife-src)
add_subdirectory(AGE)
add_subdirectory(ULCore)

enable_testing()

aux_source_directory("./GTest" GTest)
add_executable(ULGTests 
    ${GTest}
)
target_link_libraries(ULGTests aaaa0ggmcLib gtest gtest_main)

include(GoogleTest)
gtest_discover_tests(ULGTests)