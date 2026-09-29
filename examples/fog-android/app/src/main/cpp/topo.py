#!/usr/bin/env python3
"""解析 alib6 include 目录下的模块图，输出拓扑排序后的 .cppm 文件列表。

用法: topo.py <alib6-include-dir>
partition 单元 (alib6.core:types -> core/types.cppm) 与 parent 单元
(alib6.core -> core.cppm) 均支持；":name" 形式的相对 import 自动解析为
<当前 parent>:name。"import std" / "import ave*" / "import alib5*" 忽略。
"""
import re
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()

mod_file = {}   # 完整模块名 -> 文件

# 第一遍：只登记模块声明（保证依赖解析时所有模块已知）
for path in sorted(root.rglob("*.cppm")):
    text = path.read_text(encoding="utf-8", errors="replace")
    m = re.search(r'^\s*export\s+module\s+([\w:.]+)\s*;', text, re.M)
    if not m:
        continue
    mod_file[m.group(1)] = path

# 第二遍：解析导入
deps_of = {}    # 文件 -> 依赖的文件集合
for name, path in mod_file.items():
    text = path.read_text(encoding="utf-8", errors="replace")
    deps = set()
    for im in re.finditer(r'^\s*(?:export\s+)?import\s+([\w:.]+)\s*;', text, re.M):
        dep = im.group(1)
        if dep == "std":
            continue
        if dep.startswith(":"):
            parent = name.split(":")[0]
            dep = parent + dep
        if not dep.startswith("alib6"):
            continue
        if dep not in mod_file:
            print(f"warning: {path.relative_to(root)} imports unknown module {dep}",
                  file=sys.stderr)
            continue
        deps.add(mod_file[dep])
    deps_of[path] = deps

# Kahn 拓扑排序（按文件路径排序保证同层顺序稳定 → BMIs 可缓存复用）
order = []
pending = {p: set(d) for p, d in deps_of.items()}
while pending:
    ready = sorted(p for p, d in pending.items() if not d)
    if not ready:
        names = ", ".join(str(p.relative_to(root)) for p in pending)
        print(f"error: cyclic module dependency: {names}", file=sys.stderr)
        sys.exit(1)
    for p in ready:
        order.append(p)
        del pending[p]
    for d in pending.values():
        d.difference_update(ready)

for p in order:
    print(p)
