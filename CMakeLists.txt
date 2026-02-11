cmake_minimum_required(VERSION 3.10)
project(UnlimitedLife-Linux)

# ==========================================
# 1. 基础编译选项与标准
# ==========================================
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 基础编译宏与性能/调试选项
add_definitions(-DGLM_ENABLE_EXPERIMENTAL)
add_compile_options(-fno-omit-frame-pointer -g)

# Release 模式下的路径映射
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options("-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=UnlimitedLife-Linux")
endif()

# ==========================================
# 2. 路径与输出配置
# ==========================================
# 用户自定义路径缓存
set(custom_include_dir "" CACHE PATH "User customed search include path")
set(custom_lib_dir "" CACHE PATH "User customed search lib path")

# 设置构建输出目录
if(UNIX)
    set(build_dir "${CMAKE_SOURCE_DIR}/CBuild" CACHE PATH "Where to store the built bins.")
    add_link_options(-Wl,--rpath,/usr/local/lib)
else()
    set(build_dir "${CMAKE_SOURCE_DIR}/CBuild/${CMAKE_SYSTEM_NAME}" CACHE PATH "Where to store the built bin.")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${build_dir})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${build_dir})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${build_dir})

# ==========================================
# 3. 链接器全局配置
# ==========================================
# 保持你原始逻辑：无论 Win/Unix 都应用相同的 Linker Flags
if (WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-multiple-definition -Wl,--rpath,./") 
else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-multiple-definition -Wl,--rpath,./")
endif()

# ==========================================
# 4. 依赖库目录
# ==========================================
include_directories(./CDep/headers)
include_directories(${custom_include_dir})

link_directories(${custom_lib_dir})
link_directories(${build_dir})

# ==========================================
# 5. 子项目管理
# ==========================================
# add_subdirectory(aaaa0ggmcLib)
# add_subdirectory(UnlimitedLife-src)
add_subdirectory(AGE)
# add_subdirectory(ULCore)

# ==========================================
# 6. 测试配置 (GTest)
# ==========================================
enable_testing()
include(GoogleTest)

# 自动收集测试源码
aux_source_directory("./GTest" GTest_SRCS)

add_executable(ULGTests ${GTest_SRCS})

# 链接依赖
target_link_libraries(ULGTests 
    PRIVATE 
    aaaa0ggmcLib 
    gtest 
    gtest_main
)

# 注册测试
gtest_discover_tests(ULGTests)