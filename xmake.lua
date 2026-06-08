includes("xmake/defines.lua")
includes("xmake/age.lua")

add_rules("mode.debug","mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

add_requires("opengl","glfw", "glew", "glm", "stb","miniaudio")
add_requires("imgui docking", {configs = {glfw = true, opengl3 = true}})

set_languages("c++26")
set_symbols("debug")
add_cxflags("-fno-omit-frame-pointer")
add_cxxflags("-freflection", {force = true})
add_ldflags("-fPIC")
add_ldflags("-Wl,--allow-multiple-definition")
add_rpathdirs(".","/usr/local/lib")
add_includedirs("include")
add_defines(
    "GLM_ENABLE_EXPERIMENTAL",
    "ALIB5_ENABLE_REFLECTION"
)

target("AGE", function ()
    set_kind("shared")
    add_files("modules/AGE/**.cpp")
    add_files("thirdparty/miniaudio/main.c", {cxflags = "-O1" })
    
    add_defines("AGE_BUILD_DLL", { public = false })
    add_age_debug_defines()

    add_packages("opengl", "glfw", "glew", "glm", "stb", "miniaudio")

    add_links(aaaa0ggmclib , {public = true})
    if is_plat("linux", "macosx") then 
        add_syslinks("stdc++exp", {public = true})
    end
    if is_plat("windows") then
        add_defines("GLFW_DLL")
        add_syslinks("dbghelp", "Shcore", "glu32", "gdi32", "user32", "kernel32")
        add_links("glfw3", "glew32", "opengl32") 
    else
        add_syslinks("dl", "GLU", "GL", "GLEW", "glfw")
    end

end )

configure_age_test_target("agetest","tests/agetest")
configure_age_test_target("age_simptest","tests/age_simptest")