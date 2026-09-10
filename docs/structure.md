# 目录分层与管理

当前可构建目标为 `AGE` 共享库、`agetest` 和 `age_simptest` 两个交互式示例。

| 目录 | 职责 | 管理约定 |
| --- | --- | --- |
| `include/AGE/` | 引擎接口及接口依赖的头文件 | 保持 `<AGE/...>` 路径稳定；`Details/` 为内部实现细节，不保证接口稳定 |
| `modules/AGE/` | 引擎实现 | 子目录尽量对应头文件，例如 `World/`、`Audio/`、`ModelLoader/` |
| `include/ul/`、`modules/ULCore/` | 游戏逻辑原型 | 尚未接入当前 xmake 构建；依赖方向应为游戏 → AGE |
| `modules/MineForge/` | 游戏入口占位 | 当前入口为空，尚未配置构建目标 |
| `examples/` | 需要窗口和人工操作的演示 | 保留 `agetest`、`age_simptest` 目标名；不作为自动化测试 |
| `tests/` | 未来的自动化测试 | 新增时应能明确判断成功/失败；不把交互式程序作为测试 |
| `assets/` | 本地运行资源 | 当前资源未纳入 Git；参见目录内 README；日志也不提交 |
| `thirdparty/` | 随仓库保留的第三方文件及实现入口 | 下载依赖统一由 xmake 管理；许可证放在 `licenses/` |
| `xmake/` | 构建定义 | `packages.lua` 获取依赖，`alib.lua` 查找已安装的 alib5，`age.lua` 定义引擎，`examples.lua` 定义演示 |
| `scripts/` | 开发操作入口 | 统一调用 xmake；不再维护另一套 CMake 配置 |
| `docs/` | 项目文档、开发记录 | Doxygen 仅扫描 `include/`、`modules/` |
| `build/`、`.xmake/`、`.cache/`、`doxygen/` | 构建产物与缓存 | 仅忽略根目录下的这些路径，避免误伤同名脚本 |

`include/AGE/Details/` 目前仍被 `Application.h` 等公开头文件直接包含。
若要移为真正的私有头文件，需要先调整类型暴露或引入实现隐藏；本次保留路径。
`Audio.h` 与 `Audio/Audio.h` 也保留原接口，避免目录整理改变调用方行为。

第三方头文件出现在 AGE 公共接口时，其依赖须在 AGE 目标上以 `public` 传播，
下游通过 `add_deps("AGE")` 获取。ImGui 仅属于 `agetest`，不属于引擎依赖。
Linux 上 alib5 通过安装元数据获取，支持 `--alib_prefix`；安装及清理见 [安装文档](install.md)。

新增模块应同时说明所属层、依赖方向和构建目标。不要仅放入源文件后就假定已参与构建。
未来需要可重现的干净环境构建时，还需补充 alib5 的版本约束和资源获取方式。
