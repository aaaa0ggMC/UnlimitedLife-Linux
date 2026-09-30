-- 示例目录可能缺失（例如暂不公开、独立仓库的 fog），缺失时静默跳过对应 target。
local function example_exists(name)
    return os.isdir(path.join(os.scriptdir(), "..", "examples", name))
end

for _, name in ipairs({"agetest", "age_simptest"}) do
    if example_exists(name) then
        target(name)
            set_kind("binary")
            add_files("../examples/" .. name .. "/**.cpp")
            add_deps("AGE")
            if name == "agetest" then
                add_packages("imgui")
            end
            set_rundir("../assets")
        target_end()
    else
        print("[examples] skip '%s': directory not found", name)
    end
end

--- AGE Vulkan
for _, name in ipairs({"fog" , "avetest", "ave_event_test", "ave_reflect_test"}) do
    if example_exists(name) then
        target(name)
            set_kind("binary")
            add_files("../examples/" .. name .. "/**.cpp")
            add_deps("AVE")
            set_rundir("../assets")
        target_end()
    else
        print("[examples] skip '%s': directory not found", name)
    end
end
