for _, name in ipairs({"agetest", "age_simptest"}) do
    target(name)
        set_kind("binary")
        add_files("../examples/" .. name .. "/**.cpp")
        add_deps("AGE")
        if name == "agetest" then
            add_packages("imgui")
        end
        set_rundir("../assets")
    target_end()
end

--- AGE Vulkan
for _, name in ipairs({"avetest", "ave_event_test"}) do
    target(name)
        set_kind("binary")
        add_files("../examples/" .. name .. "/**.cpp")
        add_deps("AVE")
        if name == "agetest" then
            add_packages("imgui")
        end
        set_rundir("../assets")
    target_end()
end
