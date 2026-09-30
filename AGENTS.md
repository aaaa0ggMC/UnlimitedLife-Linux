# AGENTS.md

## 子项目：`examples/fog` 与 `examples/fog-android`

这两个目录**暂不公开**（导师要求），因此：

- 已写入根仓库 `.gitignore`，**不再被根仓库跟踪**。
- 各自是**独立的 git 仓库**（在各自目录内 `git init`）。等项目完成后由用户手动合并回主仓库。

### 提交规则（必须遵守）

1. 修改 `examples/fog` 或 `examples/fog-android` 后，必须**进入对应目录单独 commit**：
   `git -C examples/fog commit ...` / `git -C examples/fog-android commit ...`。
   两个仓库互相独立，各自提交，不要混在一个 commit 里。
2. **不要**用根仓库的 `git add`/`git commit` 去提交它们（会被 ignore，也不应 `git add -f`）。
3. 若改动同时涉及根仓库（如 `modules/`、`xmake/`）与 fog 目录，根仓库与子仓库**分别提交**。
4. **副总监模式（`/deputy`）注意**：派给下游模型的工作包若涉及 fog / fog-android，
   验收、review、隐私扫描后的提交都要在子仓库内单独进行；派发提示里必须写明
   "fog 系列是独立仓库，单独 commit"。

### 构建

`xmake/examples.lua` 对示例目录做了存在性检查：目录缺失（例如别人克隆公开仓库时没有 fog）
时会打印 `[examples] skip ...` 并跳过该 target，不会导致配置失败。
