cmake_minimum_required(VERSION 3.10)
project(UnlimitedLife-Linux)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_definitions(-DGLM_ENABLE_EXPERIMENTAL)
add_compile_options(-fno-omit-frame-pointer -g)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options("-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=UnlimitedLife-Linux")
endif()

if (WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-multiple-definition -Wl,--rpath,./") 
else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-multiple-definition -Wl,--rpath,./")
endif()

include_directories(./CDep/headers)
link_directories(/usr/local/lib)
link_directories(./build)

# add_subdirectory(aaaa0ggmcLib)
# add_subdirectory(UnlimitedLife-src)
add_subdirectory(AGE)
add_subdirectory(ULCore)

enable_testing()
include(GoogleTest)
aux_source_directory("./GTest" GTest_SRCS)
add_executable(ULGTests ${GTest_SRCS})
target_link_libraries(ULGTests 
    PRIVATE 
    aaaa0ggmcLib 
    gtest 
    gtest_main
)
gtest_discover_tests(ULGTests)