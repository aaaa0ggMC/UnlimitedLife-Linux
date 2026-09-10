# 用这个项目学习 xmake

先沿着一条实际依赖读代码：`agetest → AGE → alib5`。
`agetest` 与 `AGE` 都在当前工程中定义；alib5 是另一个工程安装出来的库。
这决定了它们在 xmake 中使用不同接口。

## 1. 四个最常用的依赖接口

| 接口 | 回答的问题 | 本项目示例 |
| --- | --- | --- |
| `target("AGE")` | 本工程要生成什么？ | 引擎共享库 |
| `add_deps("AGE")` | 当前目标依赖本工程哪个目标？ | 演示先构建并链接 AGE |
| `add_requires("alib5")` | 工程需要解析哪个外部包？ | 查询已安装的 alib5 |
| `add_packages("alib5")` | 哪个目标使用这个包？ | 把包的头文件、库和参数应用到 AGE |

`add_requires` 只声明工程需要包，不会把它自动塞给所有目标。
`add_links("aaaa0ggmcLib")` 只提供链接库名，不提供头文件目录、包来源或模块描述。
`add_repositories` 用于告诉 xmake 去哪里找包的描述文件。

## 2. 从最小工程开始

假设根目录有 `include/`、`src/`、`examples/`：

```lua
set_project("Demo")
set_languages("c++23")
add_rules("mode.debug", "mode.release")

add_requires("glm")

target("engine")
    set_kind("shared")
    add_files("src/**.cpp")
    add_includedirs("include", {public = true})
    add_headerfiles("include/(Demo/**.h)")
    add_packages("glm", {public = true})
target_end()

target("demo")
    set_kind("binary")
    add_files("examples/main.cpp")
    add_deps("engine")
target_end()
```

`**.cpp` 匹配子目录中的源文件；`add_files` 决定编译哪些文件。
`add_headerfiles` 决定安装哪些头文件。两者不是同一件事。
`include/(Demo/**.h)` 的括号保留安装后的 `Demo/` 层级，因此消费方仍可写 `<Demo/foo.h>`。

## 3. public 是依赖边界

如果 `AGE/Application.h` 包含 GLFW 或 GLM 的头文件，消费 AGE 的程序编译时也需要这些依赖。
因此 AGE 写 `add_packages("glfw", "glm", {public = true})`。
演示只写 `add_deps("AGE")` 就能继承公开的使用要求。

库自身才使用的实现宏（例如 `AGE_BUILD_DLL`）保持私有，不能传给调用者。
ImGui 只被完整演示使用，所以放在 `agetest` 的 `add_packages` 中。
判断是否 public 的方法是：使用我的公开头文件的人，是否也需要这个设置？

注意：不要假定所有类型的参数都沿依赖自动传递。本机验证发现运行库搜索路径需要单独明确配置。

## 4. 为什么拆成这些文件

根 `xmake.lua` 声明通用规则、配置开关，再按顺序加载子文件：

- `xmake/alib.lua`：描述“已经安装的 alib5”如何查找。
- `xmake/packages.lua`：声明第三方包来源；平台分支集中在这里。
- `xmake/age.lua`：定义 AGE 的源文件、公开依赖、私有依赖与安装头文件。
- `xmake/examples.lua`：定义演示目标。

目录名不影响构建含义，`includes(...)` 才使文件参与配置。
子脚本的文件路径以子脚本所在目录解释，因此 `xmake/age.lua` 中使用 `../modules/AGE/**.cpp`。
这是分文件后容易写错的一点。

根级设置会作用于后面定义的目标，适合语言版本和通用构建模式。
只针对一个库的设置应放在对应 `target` 内，并用 `target_end()` 明确结束。

## 5. 配置开关和执行阶段

```lua
option("examples")
    set_default(true)
    set_showmenu(true)
    set_description("Build interactive examples")
option_end()

if has_config("examples") then
    includes("xmake/examples.lua")
end
```

用户通过 `xmake f --examples=n` 关闭演示。
`option` 声明应能够在配置菜单加载时被读取，不要把声明藏在尚未确定的平台分支中。
布尔开关用 `has_config`，路径等配置值用 `get_config`。

`on_load`、`on_fetch` 等是回调函数：配置文件先登记它们，xmake 在相应阶段调用。
它们不是运行到这一行就立即执行的普通函数调用。

## 6. alib5 的包描述怎么读

打开 `xmake/alib.lua`，按这条顺序读：

1. `option("alib_prefix")` 提供工程级安装路径。
2. `package("alib5")` 定义一个外部包描述。
3. `add_configs("prefix", ...)` 定义这个包接受的参数。
4. `on_fetch(function(package) ... end)` 检查安装内容，再从 `.pc` 文件读取使用要求。
5. `add_requires("alib5", {configs = {prefix = ...}})` 将工程选项传给包。
6. AGE 用 `add_packages("alib5", {public = true})` 消费它。

这里的 `on_fetch` 负责描述现有安装，不负责下载源码或执行安装脚本。
当前包没有 `on_install`，缺失安装时会报错，提示先安装。
真正要做可下载的源码包时，才添加来源、版本约束和 `on_install`。

## 7. alib6 多出来的模块层

普通头文件库通常需要头文件路径和链接库。
alib6 的消费端还需要知道模块接口源文件在哪里、叫什么名字、导入了哪些模块。

安装脚本保存以下内容：

```text
<prefix>/
  include/alib6/                      公开接口原始目录
  lib/libaaaa0ggmcLib6.so             已编译的实现
  share/alib6/
    modules/<hash>/*.cppm             xmake 可发现的模块接口
    modules/<hash>/*.meta-info        模块名字与导入关系
    repository/packages/a/alib6/xmake.lua
```

生成的包描述用 `on_load` 把包的 `installdir` 指向 `share/alib6`。
xmake 从这个目录下的 `modules/` 扫描接口；`on_fetch` 另外返回真实的 include/lib 路径和库文件。
消费端根据自己的工具链编译模块接口，不复制另一工程的 BMI 缓存。
编译器、标准库和编译选项仍须与库兼容。

消费端只有 `main.cpp` 时，显式设置 `set_policy("build.c++.modules", true)`，
让 xmake 启用模块依赖扫描。只有 `-freflection` 并不会启用模块构建。
完整用法见 [安装文档的 alib6 部分](install.md#alib6-模块库)。

## 8. 构建、运行和安装要分别考虑

```bash
xmake f -m release --examples=y   # 保存工程配置并解析依赖
xmake build AGE                  # 构建指定目标
xmake build                      # 构建默认目标
xmake run agetest                 # 从配置的运行目录启动
./scripts/install AGE            # 构建、暂存、按清单安装
```

`set_rundir("../assets")` 控制程序工作目录，用于解释纹理、着色器等相对路径。
`add_rpathdirs` 控制 Linux 动态加载器到哪里找共享库，两者解决不同的问题。
`$ORIGIN` 表示可执行文件或共享库自身所在目录，不是 shell 当前目录。

安装脚本是项目自己的额外管理层：

```text
普通用户构建 → xmake 安装到临时目录 → 生成元数据 → 比较新旧清单
                                              ↓
                             备份 → 复制更新 → 清理旧文件 → 保存清单
```

xmake 定义产物，Python 清单逻辑管理安装文件的生命周期。
删除范围由库的命名空间及清单约束，不能对整个 `/usr/local/include` 做目录同步删除。

## 9. 不要原样复制的项目特定设置

本项目的 `c++26`、`-freflection` 和 `stdc++exp` 服务于当前 GCC/反射代码。
`force = true` 只让 xmake 不忽略该参数，不会赋予编译器不存在的能力。
`add_cxxflags` 用于 C++，`add_cxflags` 同时用于 C/C++；反射参数不应传给 miniaudio 的 C 源文件。

`--allow-multiple-definition` 是继承的链接策略，不是新项目默认应加的选项。
它可能掩盖重复定义问题，后续应该根据具体符号排查。

建议学习顺序：先写一个库加一个程序，再加 public 依赖，最后学习外部包和 Modules。
