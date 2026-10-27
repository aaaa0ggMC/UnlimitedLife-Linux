target("AVE")
    set_kind("shared")
    add_defines("AVE_BUILD_DLL")
    add_defines("GLM_FORCE_RADIANS", "GLM_FORCE_DEPTH_ZERO_TO_ONE", {public = true})
    add_defines("ALIB5_ENABLE_REFLECTION", "GLM_ENABLE_EXPERIMENTAL", {public = true})

    -- 对外提供的公共头（去 modules 后为传统头文件，不再编译 .cppm 接口单元）
    add_includedirs("../include", {public = true})
    add_headerfiles("../include/(AVE/**.h)", "../include/(AVE/**.inl)")
    add_files("../modules/AVE/**.cpp")
    add_syslinks("stdc++exp", {public = true})

    -- AVE 使用的外部依赖（alib5 头文件式基础库）
    add_packages("alib5", {public = true})
    -- Implementation-only dependency: consumers never include vk_mem_alloc.h.
    add_packages("vulkan-memory-allocator")
    add_packages("vulkansdk", "glfw", "glm", {public = true})

    -- 与 alib5 当前共享库构建方式保持一致
    add_rules("utils.symbols.export_all")
    add_packages("stb", {public = true})
target_end()

target("ave_buffer_test")
    set_kind("binary")
    set_default(false)
    add_files("../tests/ave_buffer_test.cpp")
    add_deps("AVE")
target_end()

target("ave_shape_loader_test")
    set_kind("binary")
    set_default(false)
    add_files("../tests/ave_shape_loader_test.cpp")
    add_deps("AVE")
target_end()

target("ave_debug_messenger_test")
    set_kind("binary")
    set_default(false)
    add_files("../tests/ave_debug_messenger_test.cpp")
    add_deps("AVE")
target_end()

target("ave_prefab_test")
    set_kind("binary")
    set_default(false)
    add_files("../tests/ave_prefab_test.cpp")
    add_deps("AVE")
target_end()

target("ave_error_test")
    set_kind("binary")
    set_default(false)
    add_files("../tests/ave_error_test.cpp")
    add_deps("AVE")
target_end()

target("ave_swapchain_policy_test")
    set_kind("binary")
    set_default(false)
    add_files("../tests/ave_swapchain_policy_test.cpp")
    add_packages("vulkansdk")
target_end()
