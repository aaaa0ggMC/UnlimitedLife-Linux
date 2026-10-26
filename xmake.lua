set_project("UnlimitedLife")
set_languages("c++26")
add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

option("examples")
    set_default(true)
    set_showmenu(true)
    set_description("Build the interactive AGE examples")
option_end()

-- Requires a GCC toolchain with C++26 reflection and a matching alib5 build.
set_symbols("debug")
add_cxflags("-fno-omit-frame-pointer")
add_cxxflags("-freflection", {force = true})

-- alib5（头文件式基础库）经 xmake/alib.lua 引入；去 modules 后不再需要 alib6 模块仓库。
-- RUNPATH（含 --alib5_libdir 优先序）在 xmake/alib.lua 内、选项定义之后设置。
includes("xmake/alib.lua")
-- Root-scope options must be applied here: settings in the package-only include
-- do not propagate to targets declared by sibling include scripts.
if is_plat("linux") then
    local libdir = get_config("alib5_libdir")
    if libdir then add_rpathdirs(path.absolute(libdir)) end
    add_rpathdirs("$ORIGIN", path.join(get_config("alib_prefix") or "/usr/local", "lib"))
end
includes("xmake/ave.lua")
includes("xmake/packages.lua")
includes("xmake/age.lua")
if has_config("examples") then
    includes("xmake/examples.lua")
end
