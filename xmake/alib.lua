option("alib_prefix")
    set_default("/usr/local")
    set_showmenu(true)
    set_description("Prefix containing alib5 installed by scripts/install")
option_end()

-- 可选：指向 aaaa0ggmcLib 源码根（含 include/alib5）。设置后 AVE/AGE 用源码头编译。
-- 注意：源码头与链接库必须匹配——安装头/库落后于源码时，需同时用 --alib5_libdir
-- 指定基础库自己 build 出的 libaaaa0ggmcLib.so 目录（否则会链到旧的 /usr/local 库，
-- 出现缺符号如 alib5::data::JSON::__internal_dump）。不把个人绝对路径写死进源码。
option("alib5_src")
    set_showmenu(true)
    set_description("aaaa0ggmcLib source root; compiles against its include/alib5 headers (pair with --alib5_libdir)")
option_end()

-- 可选：libaaaa0ggmcLib.so 所在目录。设置后链接与 RUNPATH 都优先使用该目录的匹配库，
-- 不再让 /usr/local/lib 的旧库覆盖。默认空 = 用已安装 prefix 的库。
option("alib5_libdir")
    set_showmenu(true)
    set_description("Directory holding the matching libaaaa0ggmcLib.so; takes link + RUNPATH priority over the installed prefix")
option_end()

-- Locate this exact installation, even when /usr/local is absent from pc_path.
package("alib5")
    set_description("Locally installed aaaa0ggmcLib shared library")
    add_configs("prefix", {description = "Installation prefix", default = "/usr/local", type = "string"})
    add_configs("src", {description = "Optional source root for matching headers", default = "", type = "string"})
    add_configs("libdir", {description = "Optional matching library directory", default = "", type = "string"})
    on_fetch(function (package)
        local prefix = path.absolute(package:config("prefix"))
        local pcdir = path.join(prefix, "lib", "pkgconfig")
        assert(os.isfile(path.join(pcdir, "aaaa0ggmcLib.pc")),
               "alib5 metadata missing; run ./scripts/install alib5 or configure --alib_prefix=<prefix>")
        assert(os.isfile(path.join(prefix, "lib", "libaaaa0ggmcLib.so")) and
               os.isfile(path.join(prefix, "include", "alib5", "autil.h")),
               "alib5 installation is incomplete: " .. prefix)
        local result = import("lib.detect.find_package")("pkgconfig::aaaa0ggmcLib", {
            configdirs = {pcdir}, plat = package:plat(), arch = package:arch(),
            force = true, cachekey = "alib5_" .. prefix
        })
        -- Put both overrides in package configs, so changing them invalidates
        -- xmake's package fetch cache, not just an unused field in its result.
        local src = package:config("src")
        local libdir = package:config("libdir")
        if src == "" then src = nil end
        if libdir == "" then libdir = nil end
        if result then
            -- 源码头（头文件式）：优先用源码 include/alib5 头。
            if src then
                local src_inc = path.absolute(path.join(src, "include"))
                assert(os.isdir(path.join(src_inc, "alib5")), "alib5_src has no include/alib5: " .. src)
                result.includedirs = result.includedirs or {}
                table.insert(result.includedirs, 1, src_inc)
            end
            -- 匹配库：链接目录指向 libdir（-L 首选），并纳入 cachekey 避免陈旧缓存。
            if libdir then
                libdir = path.absolute(libdir)
                assert(os.isfile(path.join(libdir, "libaaaa0ggmcLib.so")),
                       "alib5_libdir has no libaaaa0ggmcLib.so: " .. libdir)
                result.linkdirs = { libdir }
            end
            -- 源码头但未指定匹配库：提醒可能链到旧 /usr/local 库而缺符号。
            if src and not libdir then
                print("warning: --alib5_src set without --alib5_libdir; the installed /usr/local libaaaa0ggmcLib.so may mismatch the source headers (e.g. missing alib5::data::JSON::__internal_dump). Consider --alib5_libdir=<aaaa0ggmcLib build dir>.")
            end
        end
        return result
    end)
package_end()

if is_plat("linux") then
    add_requires("alib5", {system = false, configs = {
        prefix = get_config("alib_prefix") or "/usr/local",
        src = get_config("alib5_src") or "",
        libdir = get_config("alib5_libdir") or ""
    }})
end
