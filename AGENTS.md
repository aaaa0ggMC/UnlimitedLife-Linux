# AGENTS.md

## 子项目：`examples/fog`、`examples/fog-android` 与 `examples/fog_gl`

这三个目录（`fog_gl` 为 AGE/OpenGL 版雾场景基准复刻）已随科研轮转结束**并入根仓库公开跟踪**：

- 不再写入 `.gitignore`，按普通目录用根仓库的 `git add` / `git commit` 提交。
- 原独立子仓库的 `.git` 已移到 `.git/fog-subrepo-backup/`（仅本地留存，不入库）；
  需要追溯子仓历史时从该处恢复，不要在示例目录内重新 `git init`。
- 隐私边界不变：`fog-android/local.properties`、`.deps/`、各类 `build/` 产物仍由对应
  `.gitignore` 排除，不要 `git add -f` 强行纳入。

### 提交规则

1. 修改这些目录后按根仓库常规流程提交：只暂存本任务相关文件，不混入其他改动，
   不把三个目录拆成多个提交。
2. `fog-android` 的 APK 构建命名规则见 `examples/fog-android/AGENTS.md`，仍然有效。
3. 副总监模式派工涉及这些目录时按根仓库单仓处理，无需再拆子仓提交。

### 构建

`xmake/examples.lua` 对示例目录做存在性检查：目录缺失（例如裁剪过的检出）时
会打印 `[examples] skip ...` 并跳过该 target，不会导致配置失败。
