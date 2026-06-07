includes("xmake/defines.lua")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

add_requires("opengl","glfw", "glew", "glm", "stb", "rapidjson","miniaudio")
add_requires("imgui docking", {configs = {glfw = true, opengl3 = true}})

add_rules("mode.debug","mode.release")
set_languages("c++26")
set_symbols("debug")
add_cxflags("-fno-omit-frame-pointer")
add_cxxflags("-freflection", {force = true})
add_ldflags("-fPIC")
add_ldflags("-Wl,--allow-multiple-definition")

add_defines(
    "GLM_ENABLE_EXPERIMENTAL",
    "ALIB5_ENABLE_REFLECTION"
)
add_rpathdirs(".","/usr/local/lib")
add_includedirs("CDep/headers")

function add_debug_flags()
    if is_mode("debug") then
        add_defines(
            "AGE_TRACE_SKIP_CPP_NATIVE",
            "AGE_TRACE_COMPACT",
            "AGE_ML_DEBUG",
            "AGE_LIGHT_BUZZ"
        )
    end
end

target("AGE", function ()
    set_kind("shared")
    add_files("AGE/src/**.cpp")
    add_files("AGE/miniaudio/main.c", {cxflags = "-O1" })
    
    add_defines("AGE_BUILD_DLL", { public = false })
    add_debug_flags()

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

function configure_age_test_target(target_name,src_dir)
    target(target_name)
        set_kind("binary")
        add_files(src_dir .. "/**.cpp")
        add_debug_flags()
        
        add_deps("AGE")
        add_packages("imgui")
        add_links(aaaa0ggmcLib , { public = true})
        set_rundir("data") 
        if is_plat("windows") then
            add_defines("GLFW_DLL")
            add_links("glfw3", "glew32", "opengl32")
        else
            add_syslinks("GLEW", "GL", "glfw")
        end
        
        -- set_policy("build.sanitizer.address", true)
        -- set_policy("build.sanitizer.undefined", true)
end 

configure_age_test_target("agetest","CTests/agetest")
configure_age_test_target("age_simptest","CTests/age_simptest")