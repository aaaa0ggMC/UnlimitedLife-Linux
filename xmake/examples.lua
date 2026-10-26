-- 示例目录可能缺失（例如暂不公开、独立仓库的 fog），缺失时静默跳过对应 target。
local function example_exists(name)
    return os.isdir(path.join(os.scriptdir(), "..", "examples", name))
end

if example_exists("fog") then
    target("fog_camera_test")
        set_kind("binary")
        set_default(false)
        add_files("../examples/fog/tests/camera_test.cc")
        add_deps("AVE")
    target_end()
    target("fog_ecs_test")
        set_kind("binary")
        set_default(false)
        add_files("../examples/fog/tests/ecs_test.cc")
        add_deps("AVE")
    target_end()
end

for _, name in ipairs({"agetest", "age_simptest", "fog_gl"}) do
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
for _, name in ipairs({"fog" , "avetest", "avetest_tri", "ave_event_test", "ave_reflect_test"}) do
    if example_exists(name) then
        target(name)
            set_kind("binary")
            add_files("../examples/" .. name .. "/**.cpp")
            add_deps("AVE")
            if name == "fog" then
                add_packages("imgui")
                before_build(function (target)
                    os.vrunv("bash", {path.join(target:scriptdir(), "..", "examples", "fog", "build_shaders.sh")})
                end)
            end
            if name == "avetest" then
                before_build(function (target)
                    local root = path.join(target:scriptdir(), "..")
                    local output = path.join(root, "assets", "avetest", "shaders", "compute-test.spv")
                    os.mkdir(path.directory(output))
                    os.vrunv("glslangValidator", {
                        "-V", "--target-env", "vulkan1.0", "-o", output,
                        path.join(root, "examples", "avetest", "compute.comp")
                    })
                end)
            end
            set_rundir("../assets")
        target_end()
    else
        print("[examples] skip '%s': directory not found", name)
    end
end
