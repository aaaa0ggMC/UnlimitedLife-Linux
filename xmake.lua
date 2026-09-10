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
-- Retain the existing executable link policy until duplicate symbols are audited.
add_ldflags("-Wl,--allow-multiple-definition")

if is_plat("linux") then
    add_rpathdirs("$ORIGIN", path.join(get_config("alib_prefix") or "/usr/local", "lib"))
end

includes("xmake/alib.lua")
includes("xmake/packages.lua")
includes("xmake/age.lua")
if has_config("examples") then
    includes("xmake/examples.lua")
end
