# ==========================================
# 1. 第三方库: miniaudio (静态库)
# ==========================================
aux_source_directory("./miniaudio-src" MINIAUDIO_SRC)
add_library(miniaudio STATIC ${MINIAUDIO_SRC})

# 保持你的特殊优化设置：-O2 会崩溃，故锁定 -O1
target_compile_options(miniaudio PUBLIC "-O1")


# ==========================================
# 2. 核心库: AGE (动态库)
# ==========================================
# 收集所有源码目录
aux_source_directory("./src" AGE_SRC_ROOT)
aux_source_directory("./src/ModelLoader" AGE_SRC_MODEL)
aux_source_directory("./src/Audio" AGE_SRC_AUDIO)
aux_source_directory("./src/Details" AGE_SRC_DETAIL)

add_library(AGE SHARED 
    ${AGE_SRC_ROOT} 
    ${AGE_SRC_MODEL} 
    ${AGE_SRC_AUDIO} 
    ${AGE_SRC_DETAIL}
)

set_target_properties(AGE PROPERTIES LINKER_LANGUAGE CXX)

# --- AGE 编译宏与选项 ---
target_compile_definitions(AGE PUBLIC AGE_BUILD_DLL) # 统一设为 PUBLIC

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(AGE PRIVATE "-g" "-O0")
    target_compile_definitions(AGE PRIVATE 
        AGE_TRACE_SKIP_CPP_NATIVE 
        AGE_TRACE_COMPACT 
        AGE_ML_DEBUG 
        AGE_LIGHT_BUZZ
    )
else()
    target_compile_options(AGE PRIVATE "-g" "-O3")
endif()

# --- AGE 平台特定链接 ---
target_link_libraries(AGE PUBLIC stdc++exp aaaa0ggmcLib)

if(WIN32)
    target_compile_definitions(AGE PRIVATE GLFW_DLL)
    target_link_libraries(AGE PRIVATE 
        miniaudio dbghelp Shcore glu32 
        glfw3.dll glew32.dll opengl32 gdi32 user32 kernel32
    )
else()
    target_link_libraries(AGE PRIVATE miniaudio dl GLU OpenGL GLEW glfw)
endif()


# ==========================================
# 3. 测试程序 (agetest & age_simptest)
# ==========================================

# --- 辅助宏：减少测试程序的重复配置 ---
macro(configure_age_test_target TARGET_NAME SOURCES)
    add_executable(${TARGET_NAME} ${SOURCES})
    
    # 编译选项
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${TARGET_NAME} PRIVATE "-g" "-O0")
        target_compile_definitions(${TARGET_NAME} PRIVATE AGE_TRACE_COMPACT AGE_EM_DEBUG AGE_LIGHT_BUZZ)
    else()
        target_compile_options(${TARGET_NAME} PRIVATE "-g" "-O3")
    endif()

    # 链接与平台定义
    if(WIN32)
        target_compile_definitions(${TARGET_NAME} PRIVATE GLFW_DLL)
        target_link_libraries(${TARGET_NAME} PRIVATE imgui AGE glfw3.dll glew32.dll opengl32)
    else()
        target_link_libraries(${TARGET_NAME} PRIVATE imgui AGE glfw GLEW OpenGL)
    endif()
endmacro()

# 实例化 agetest
aux_source_directory("./age_test/agetest-src/" AGETEST_SRC)
configure_age_test_target(agetest "${AGETEST_SRC}")

# 实例化 age_simptest
aux_source_directory("./age_test/simp-src/" SIMPTEST_SRC)
configure_age_test_target(age_simptest "${SIMPTEST_SRC}")