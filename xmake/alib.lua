option("alib_prefix")
    set_default("/usr/local")
    set_showmenu(true)
    set_description("Prefix containing alib5 installed by scripts/install")
option_end()

-- Locate this exact installation, even when /usr/local is absent from pc_path.
package("alib5")
    set_description("Locally installed aaaa0ggmcLib shared library")
    add_configs("prefix", {description = "Installation prefix", default = "/usr/local", type = "string"})
    on_fetch(function (package)
        local prefix = path.absolute(package:config("prefix"))
        local pcdir = path.join(prefix, "lib", "pkgconfig")
        assert(os.isfile(path.join(pcdir, "aaaa0ggmcLib.pc")),
               "alib5 metadata missing; run ./scripts/install alib5 or configure --alib_prefix=<prefix>")
        assert(os.isfile(path.join(prefix, "lib", "libaaaa0ggmcLib.so")) and
               os.isfile(path.join(prefix, "include", "alib5", "autil.h")),
               "alib5 installation is incomplete: " .. prefix)
        return import("lib.detect.find_package")("pkgconfig::aaaa0ggmcLib", {
            configdirs = {pcdir}, plat = package:plat(), arch = package:arch(),
            force = true, cachekey = "alib5_" .. prefix
        })
    end)
package_end()

if is_plat("linux") then
    add_requires("alib5", {system = false, configs = {prefix = get_config("alib_prefix") or "/usr/local"}})
end
