includes("xmake/defines.lua")
includes("xmake/age.lua")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

if is_plat("mingw", "msys") then
    add_requires("opengl")
    add_requires("pacman::glfw", {alias = "glfw"})
    add_requires("pacman::glew", {alias = "glew"})
    add_requires("pacman::glm", {alias = "glm"})
    add_requires("pacman::stb", {alias = "stb"})
    add_requires("miniaudio") -- miniaudio 是单文件，用 xmake 源即可
else
    add_requires("opengl", "glfw", "glew", "glm", "stb", "miniaudio")
end

add_requires("imgui docking", {configs = {glfw = true, opengl3 = true}})

set_languages("c++26")
set_symbols("debug")
add_cxflags("-fno-omit-frame-pointer")
add_cxxflags("-freflection", {force = true})
add_ldflags("-fPIC")
add_ldflags("-Wl,--allow-multiple-definition")

add_rpathdirs(".", "/usr/local/lib", "/ucrt64/bin")
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
    add_links(aaaa0ggmcLib , {public = true})

    add_syslinks("stdc++exp", {public = true})
    
    if is_plat("windows", "mingw") then
        add_defines("GLFW_DLL")
        add_syslinks("dbghelp", "shcore", "glu32", "gdi32", "user32", "kernel32")
        add_links("glfw3.dll", "glew32", "opengl32") 
    else
        add_syslinks("dl", "GLU", "GL", "GLEW", "glfw")
    end

end)

configure_age_test_target("agetest", "tests/agetest")
configure_age_test_target("age_simptest", "tests/age_simptest")