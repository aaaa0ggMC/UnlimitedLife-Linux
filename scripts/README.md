# 开发脚本

构建脚本需要 Bash，自动定位项目根目录，可从任意工作目录调用；参数原样转交 xmake。
`install` 使用 Python 3，拥有独立参数，详见 [安装与升级](../docs/install.md)。

| 入口 | 用途 | 示例（在项目根目录执行） |
| --- | --- | --- |
| `install` | 带清单、备份和旧文件清理的库安装 | `./scripts/install alib5` / `./scripts/install alib6` |
| `configure` | 配置 xmake | `./scripts/configure -m release --examples=y` |
| `build` | 构建全部或指定目标 | `./scripts/build AGE` |
| `run` | 运行目标，默认 `agetest` | `./scripts/run age_simptest` |
| `dev` | 配置 Debug 后构建；参数交给配置步骤 | `./scripts/dev --examples=y` |
| `configure_win_msys2.sh` | UCRT64 下配置 MinGW | `./scripts/configure_win_msys2.sh -m debug` |
| `build_win_msys2.sh` | UCRT64 下构建 | `./scripts/build_win_msys2.sh` |
| `compress` | 将已提交的 HEAD 导出到 `build/packages/` | `./scripts/compress` |
| `pre-commit` | 只检查暂存差异中的空白错误 | 可手动调用；不自动安装 |

根目录 `configure.sh` 是 `scripts/configure` 的兼容入口。配置脚本不再安装系统软件，
也不更新系统包数据库；依赖获取交给 xmake，首次配置可能需要网络及确认。
工具链和 alib5 需要预先安装，详见根目录 README。

`run` 支持 xmake 的运行参数，例如 `./scripts/run -d agetest`；程序参数放在目标名后。
资源工作目录由 xmake 设置为 `assets/`。Windows 脚本要求 `MSYSTEM=UCRT64`，
需先配置再构建；目前未验证 Windows 运行和发布包，不提供复制 DLL 的发布脚本。

`compress` 只导出 Git 中已提交的内容，不包含工作区改动、未跟踪资源或构建产物。
请先提交需要发布的改动。该源码归档不是可直接运行的游戏发布包。

旧的 `buildPkg`、`post_msys.sh`、`imgui/CMakeLists.txt` 已移除：
仓库没有配套的 PKGBUILD 或主 CMake 项目，ImGui 已由 xmake 管理。
`pre-commit` 不再自动更新日期或重新暂存整个文件，以免把未暂存修改带进提交。
