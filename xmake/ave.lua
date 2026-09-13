target("AVE")
    set_kind("shared")
    add_defines("AVE_BUILD_DLL")
    add_defines("ALIB6_ERROR_USE_PANIC", {public = true})
    add_defines("GLM_FORCE_RADIANS", "GLM_FORCE_DEPTH_ZERO_TO_ONE", {public = true})

    -- 启用模块依赖扫描和构建
    set_policy("build.c++.modules", true)

    -- 对外提供的模块接口
    add_includedirs("../include", {public = true})
    add_headerfiles("../include/(AVE/**.h)", "../include/(AVE/**.inl)", "../include/(AVE/**.cppm)")
    add_files("../include/AVE/**.cppm", {public = true})
    add_files("../modules/AVE/**.cpp")
    add_syslinks("stdc++exp", {public = true})

    -- AVE 使用的外部依赖
    add_packages("alib6", {public = true})
    add_packages("vulkansdk", "glfw", "glm", {public = true})

    -- 与 alib6 当前共享库构建方式保持一致
    add_rules("utils.symbols.export_all")
target_end()