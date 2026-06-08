function add_age_debug_defines()
    if is_mode("debug") then
        add_defines(
            "AGE_TRACE_SKIP_CPP_NATIVE",
            "AGE_TRACE_COMPACT",
            "AGE_ML_DEBUG",
            "AGE_LIGHT_BUZZ"
        )
    end
end

function configure_age_test_target(target_name,src_dir)
    target(target_name)
        set_kind("binary")
        add_files(src_dir .. "/**.cpp")
        add_age_debug_defines()
        
        add_deps("AGE")
        add_packages("imgui")
        add_links(aaaa0ggmcLib , { public = true})
        set_rundir("assets")
        if is_plat("windows") then
            add_defines("GLFW_DLL")
            add_links("glfw3", "glew32", "opengl32")
        else
            add_syslinks("GLEW", "GL", "glfw")
        end
        
        -- set_policy("build.sanitizer.address", true)
        -- set_policy("build.sanitizer.undefined", true)
end 
