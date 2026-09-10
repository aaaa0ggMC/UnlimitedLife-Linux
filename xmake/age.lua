target("AGE")
    set_kind("shared")
    add_files("../modules/AGE/**.cpp")
    add_files("../thirdparty/miniaudio/implementation.c", {cxflags = "-O1"})
    add_includedirs("../include", {public = true})
    add_headerfiles("../include/(AGE/**.h)", "../include/(AGE/**.inl)")

    add_defines("AGE_BUILD_DLL")
    add_defines("GLM_ENABLE_EXPERIMENTAL", "ALIB5_ENABLE_REFLECTION", {public = true})
    if is_mode("debug") then
        add_defines("AGE_TRACE_SKIP_CPP_NATIVE", "AGE_TRACE_COMPACT",
                    "AGE_ML_DEBUG", "AGE_LIGHT_BUZZ", {public = true})
    end

    -- AGE public headers expose these libraries; consumers inherit their settings.
    add_packages("opengl", "glfw", "glew", "glm", {public = true})
    add_packages("stb", "miniaudio")
    if is_plat("linux") then
        add_packages("alib5", {public = true})
    else
        -- The managed installer currently supports Linux only.
        add_links("aaaa0ggmcLib", {public = true})
    end
    add_syslinks("stdc++exp", {public = true})

    if is_plat("windows", "mingw", "msys") then
        add_defines("GLFW_DLL", {public = true})
        add_syslinks("dbghelp", "shcore", "glu32", "gdi32", "user32", "kernel32")
    else
        add_syslinks("dl", "GLU")
    end
target_end()
