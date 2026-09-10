# 安装、升级与卸载（Linux）

统一入口是 `scripts/install`，需要 Python 3、xmake 和 pkg-config。
它支持 AGE、alib5 和 alib6 的本机 Linux 共享库，不覆盖静态库或跨平台发布。
不要在入口前加 sudo：构建与 xmake 暂存安装以当前用户运行，只有最终写入受保护前缀时才调用 sudo。

## 首次迁移已有的 /usr/local 安装

在 UL 根目录执行（默认 alib5 源码位于相邻的 `../aaaa0ggmcLib`）：

```bash
./scripts/install alib5 --adopt-existing --dry-run
./scripts/install alib5 --adopt-existing
./scripts/configure --alib_prefix=/usr/local
./scripts/build
```

源码在其他位置时传 `--project /path/to/aaaa0ggmcLib`。项目需已完成 xmake 配置。
`--dry-run` 仍会构建和暂存，但不修改安装前缀；`--no-build` 可复用已构建产物。
已有构建配置（编译器、Debug/Release）保持不变。

首次 `--adopt-existing` 只接管该库的头文件目录、共享库及对应 `.pc` 文件。
它会删除该目录中已不再由新版本安装的文件，所以先用预览查看；旧内容会备份。
alib5 不接管 `alib6/`、`libaaaa0ggmcLib6.so` 或 `libaaaa0ggmcLib-static.a`。
已建立清单后不再使用 `--adopt-existing`。

## 日常更新

```bash
./scripts/install alib5
./scripts/configure --alib_prefix=/usr/local
./scripts/build
# 安装 AGE 的共享库与完整 AGE 头文件
./scripts/install AGE
```

每次安装比较当前产物与上次清单，新增/更新文件并删除旧清单中已消失的文件。
不会根据整个 `/usr/local` 的内容猜测所有权。未管理的同名文件会阻止安装；
已管理文件若被手动修改，也会停止，避免覆盖本地改动。
只有确实变化的文件才会重写，重复安装不会不断产生同样的备份。
不要混用此脚本和直接 `xmake install` 写入同一库，否则后续校验会发现内容变化。

## 无 sudo 的独立前缀

```bash
./scripts/install alib5 --prefix "$HOME/.local"
./scripts/configure --alib_prefix="$HOME/.local"
./scripts/build
```

UL 的 `alib5` 包声明会从指定前缀的 `.pc` 读取 include、link 和运行库搜索路径，
不需要全局设置 `CPATH`、`LIBRARY_PATH` 或 `LD_LIBRARY_PATH`。
更新前缀后重新配置 xmake，以刷新它缓存的包信息。

## 其他 xmake 项目使用已安装的 alib5

安装会生成 `<prefix>/lib/pkgconfig/aaaa0ggmcLib.pc`。
将其目录加入 `PKG_CONFIG_PATH`（本机 pkg-config 默认搜索路径不包含 `/usr/local/lib/pkgconfig`）：

```bash
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
pkg-config --cflags --libs aaaa0ggmcLib
```

```lua
set_languages("c++26")
add_cxxflags("-freflection", {force = true})
add_requires("pkgconfig::aaaa0ggmcLib", {alias = "alib5", system = true})
target("app")
    set_kind("binary")
    add_files("main.cpp")
    add_packages("alib5")
```

UL 将上述查找封装在 `xmake/alib.lua`，额外检查指定前缀，使用 `add_requires("alib5")`
和 AGE 上的 `add_packages("alib5", {public = true})` 向示例传递依赖。
`.pc` 标记为本地开发版本 `0.0.0`，不代表上游版本或 ABI 兼容承诺。
编译器和第三方开发头文件仍需与库匹配；AGE 的图形依赖仍由消费项目声明。

安装文件和提供包描述是两件事。xmake 的原生本地仓库也需要包描述再通过
`add_repositories` / `add_requires` 引用，参见 [xmake 官方说明](https://xmake.io/guide/package-management/using-local-packages.html)。
本项目采用 pkg-config 元数据，方便其他构建系统复用。

## 卸载与恢复资料

```bash
./scripts/install alib5 --uninstall --dry-run
./scripts/install alib5 --uninstall
# 自定义前缀必须与安装时相同
./scripts/install AGE --uninstall --prefix "$HOME/.local"
```

清单位于 `<prefix>/.ul-install/<库名>.json`，备份位于同目录的 `backups/`。
备份含被修改/删除的原文件、旧清单及 `transaction.json`，以安装前缀为相对路径基准。
正常复制异常或 Ctrl-C 会尝试恢复本次变更；这不是跨多个文件的断电原子事务。
备份不会自动删除，空目录也不递归删除。不要删除清单后再期望自动清理旧文件。
脚本拒绝经过符号链接的目标路径；当前仅支持普通共享库文件。

测试：`python3 -m unittest discover -s tests/scripts -v`，仅使用临时目录。

## alib6 模块库

```bash
# 首次接管已有 /usr/local 安装
./scripts/install alib6 --adopt-existing --dry-run
./scripts/install alib6 --adopt-existing
# 后续更新
./scripts/install alib6
# 卸载预览
./scripts/install alib6 --uninstall --dry-run
```

源码默认使用相邻的 `../aaaa0ggmcLib`，同样支持 `--project`、`--prefix` 和 `--no-build`。
安装使用的是源码项目自己的构建目录，不是调用脚本时所在目录的 `build/`。
例如在 UL 中执行 `scripts/install alib6`，默认使用相邻 `aaaa0ggmcLib` 的缓存。
脚本会打印实际源码目录。

若 `.meta-info` 报 `cannot open file` / `Not access because it is busy`，先检查报错文件的
所有者和写权限。以前的 `sudo xmake --root install` 可能在普通用户的构建目录生成
root 所有的元数据；此时应修复这些生成文件的所有权，不需要继续用 sudo 构建。
本机已确认过此原因，不能仅凭该错误文本断定存在并发构建。

新安装将模块接口和元数据放在 `<prefix>/share/alib6/modules/`。
首次接管旧安装时，只清理旧 `<prefix>/modules/<hash>/` 中元数据明确声明属于 `alib6`
或 `alib6.*` 的文件对。其他模块库不受影响。

消费端的 xmake.lua：

```lua
set_languages("c++26")
add_cxxflags("-freflection", {force = true})
set_policy("build.c++.modules", true)
add_repositories("local-alib6 /usr/local/share/alib6/repository")
add_requires("alib6", {system = false})

target("demo")
    set_kind("binary")
    add_files("main.cpp")
    add_packages("alib6")
target_end()
```

随后源码可以写 `import alib6;`。自定义安装前缀时替换仓库路径。
这里使用脚本生成的原生 xmake 包描述，因为它还负责让 xmake 发现模块接口。
生成的 `aaaa0ggmcLib6.pc` 仅描述头文件与链接参数，不能单独完成模块导入。

想学习各个 xmake 文件的写法，见 [项目 xmake 教程](xmake-guide.md)。
