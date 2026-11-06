#!/usr/bin/env python3
"""从 app_scene.cpp 原有的过程式布局生成 scene.json（一次性生成器，供人工后续编辑）。"""

import os
import sys
import math, json, collections

cubes = []


def block(tag, pos, mat, scale=(1.0, 1.0, 1.0), shadow=True):
    entry = {
        "tag": tag,
        "pos": [float(f"{v:g}") for v in pos],
        "scale": [float(f"{v:g}") for v in scale],
        "material": mat,
    }
    if not shadow:
        entry["shadow"] = False
    cubes.append(entry)


def terrain_height(x, z):
    if z > -9:
        return 0
    return max(
        0, int(math.floor(2.0 + 1.5 * math.sin(x * 0.42) + 1.3 * math.cos(z * 0.51)))
    )


# 27x25 岛屿：后部阶梯山，中央石路，右侧湖泊与沙岸
for z in range(-16, 9):
    for x in range(-13, 14):
        lake = 4 <= x <= 10 and -6 <= z <= 3
        shore = 3 <= x <= 11 and -7 <= z <= 4
        h = terrain_height(x, z)
        path = abs(x) <= 1 or (z == 2 and -7 <= x <= 4)
        if lake:
            block("lake-bed", (x, -1.5, z), "sand")
            block("water", (x, -0.25, z), "water", (1.0, 0.5, 1.0))
        else:
            block(
                "terrain",
                (x, (h - 1.0) * 0.5, z),
                "sand" if shore else ("stone" if path else "grass"),
                (1.0, h + 1.0, 1.0),
            )

# 跨湖木桥
for x in range(3, 12):
    for z in (-2, -1):
        block("bridge-deck", (x, 0.2, z), "wood", (1.0, 0.4, 1.0))
    if x % 2 != 0:
        block("bridge-post", (x, 0.9, -3), "wood", (0.22, 1.8, 0.22))
        block("bridge-post", (x, 0.9, 0), "wood", (0.22, 1.8, 0.22))
block("bridge-rail", (7.0, 1.25, -3.0), "wood", (9.0, 0.18, 0.18))
block("bridge-rail", (7.0, 1.25, 0.0), "wood", (9.0, 0.18, 0.18))

# 左侧小屋：石地基、木墙、门窗洞、阶梯红瓦屋顶
for x in range(-9, -4):
    for z in range(-6, -1):
        block("house-foundation", (x, 0.15, z), "stone", (1.0, 0.3, 1.0))
        if not (x == -9 or x == -5 or z == -6 or z == -2):
            continue
        for y in range(1, 4):
            door = x == -7 and z == -2 and y <= 2
            window = y == 2 and (x == -9 or x == -5) and z == -4
            if not door and not window:
                block("house-wall", (x, y - 0.2, z), "wood")
for tier in range(3):
    for x in range(-10 + tier, -4 - tier + 1):
        for z in range(-7, 0):
            block("house-roof", (x, 3.3 + tier * 0.6, z), "roof", (1.0, 0.6, 1.0))
block("chimney", (-8.0, 5.1, -5.0), "stone", (0.65, 2.0, 0.65))


# 树
def tree(x, z, trunk_height):
    base = float(terrain_height(x, z))
    for y in range(trunk_height):
        block("tree-trunk", (x, base + y + 0.5, z), "wood")
    for y in range(3):
        radius = 1 if y == 2 else 2
        for dx in range(-radius, radius + 1):
            for dz in range(-radius, radius + 1):
                if abs(dx) == radius and abs(dz) == radius:
                    continue
                if dx == 0 and dz == 0 and y == 0:
                    continue
                block(
                    "tree-leaves",
                    (x + dx, base + trunk_height - 0.5 + y, z + dz),
                    "leaves",
                )


for t in (
    (-11, 4, 4),
    (-5, 6, 3),
    (-11, -10, 4),
    (11, -10, 4),
    (7, -12, 3),
    (0, -13, 4),
):
    tree(*t)

# 中央祭坛
for x in range(-2, 3):
    for z in range(-3, 2):
        block("dragon-plinth", (x, 0.25, z), "stone", (1.0, 0.5, 1.0))
block("dragon-pedestal", (0.0, 0.75, -1.0), "stone", (3.0, 0.5, 3.0))

# 后方石拱门：拱坐在柱顶(y=5.0)上，避免与柱顶块重叠（原先 y=5.0 → 占 [4.5,5.5] 会插进柱顶）
for y in range(5):
    block("ruin-pillar", (-3, y + 0.5, -8), "stone")
    block("ruin-pillar", (3, y + 0.5, -8), "stone")
for x in range(-3, 4):
    block("ruin-arch", (x, 5.5, -8), "stone")

# 火把
for x in (-3, 3):
    for z in (2, -4):
        # 火把杆贴着点光源，标记为不投影；火焰方块本身是自发光，不进入 shadow pass。
        block("torch-post", (x, 0.8, z), "wood", (0.18, 1.6, 0.18), shadow=False)
        block("torch-flame", (x, 1.7, z), "torch-flame", (0.3, 0.4, 0.3))

# 桥上悬挂的青色灯笼：体素框架 + 自发光玻璃
block("lantern-chain", (9.0, 1.16, 0.18), "lantern-frame", (0.035, 0.16, 0.035))
block("lantern-top", (9.0, 1.06, 0.18), "lantern-frame", (0.34, 0.06, 0.34))
block("lantern-glass", (9.0, 0.88, 0.18), "lantern-glass", (0.24, 0.30, 0.24))
block("lantern-post-xn", (8.85, 0.88, 0.18), "lantern-frame", (0.04, 0.32, 0.04))
block("lantern-post-xp", (9.15, 0.88, 0.18), "lantern-frame", (0.04, 0.32, 0.04))
block("lantern-post-zn", (9.0, 0.88, 0.03), "lantern-frame", (0.04, 0.32, 0.04))
block("lantern-post-zp", (9.0, 0.88, 0.33), "lantern-frame", (0.04, 0.32, 0.04))
block("lantern-bottom", (9.0, 0.70, 0.18), "lantern-frame", (0.34, 0.06, 0.34))
block("lantern-finial", (9.0, 0.64, 0.18), "lantern-frame", (0.10, 0.06, 0.10))

crystal_entries = [
    {
        "tag": "ice-crystal",
        "pos": [5.0 + i, 1.0 + i * 0.25, 6.0],
        "axis": [0.0, 1.0, 0.0],
        "angle": 15.0 * i,
        "scale": [0.75, 2.0 + i * 0.5, 0.75],
        "material": "crystal",
        "solid": False,
    }
    for i in range(3)
]

scene = {
    "materials": {
        "grass": {"color": [0.30, 0.58, 0.13], "specular": 0.02, "shininess": 24.0, "emission": 0.0, "voxel": 1},
        "stone": {"color": [0.48, 0.51, 0.55], "specular": 0.12, "shininess": 24.0, "emission": 0.0, "voxel": 2},
        "wood": {"color": [0.45, 0.27, 0.12], "specular": 0.03, "shininess": 24.0, "emission": 0.0, "voxel": 3},
        "leaves": {"color": [0.14, 0.43, 0.12], "specular": 0.02, "shininess": 24.0, "emission": 0.0, "voxel": 4},
        "water": {"color": [0.06, 0.32, 0.61], "specular": 0.8, "shininess": 96.0, "emission": 0.0, "voxel": 5},
        "sand": {"color": [0.73, 0.64, 0.40], "specular": 0.01, "shininess": 24.0, "emission": 0.0, "voxel": 2},
        "roof": {"color": [0.50, 0.16, 0.09], "specular": 0.10, "shininess": 24.0, "emission": 0.0, "voxel": 2},
        "torch-flame": {"color": [1.0, 0.48, 0.08], "specular": 0.0, "shininess": 24.0, "emission": 1.5, "voxel": 0},
        "crystal": {"color": [1.0, 1.0, 1.0], "specular": 0.7, "shininess": 80.0, "emission": 0.0, "voxel": 0},
        "jade": {"color": [0.18, 0.62, 0.38], "specular": 0.65, "shininess": 64.0, "emission": 0.0, "voxel": 0},
        "lantern-frame": {"color": [0.15, 0.17, 0.20], "specular": 0.45, "shininess": 48.0, "emission": 0.0, "voxel": 0},
        "lantern-glass": {"color": [0.30, 0.88, 1.0], "specular": 0.25, "shininess": 32.0, "emission": 2.4, "voxel": 0},
    },
    "cubes": cubes + crystal_entries,
    "dragon": {
        "tag": "dragon",
        "pos": [0.0125, 2.132, -1.1294],
        "axis": [0.0, 1.0, 0.0],
        "angle": 25.0,
        "scale": [4.0, 4.0, 4.0],
        "material": "jade",
    },
    "lights": [
        {"tag": "torch-nw", "pos": [-3.0, 1.7, 2.0], "color": [1.0, 0.46, 0.14],
         "intensity": 36.0, "range": 11.0, "radius": 0.09, "shadow": True},
        {"tag": "torch-sw", "pos": [-3.0, 1.7, -4.0], "color": [1.0, 0.46, 0.14],
         "intensity": 36.0, "range": 11.0, "radius": 0.09, "shadow": True},
        {"tag": "torch-ne", "pos": [3.0, 1.7, 2.0], "color": [1.0, 0.46, 0.14],
         "intensity": 36.0, "range": 11.0, "radius": 0.09, "shadow": True},
        {"tag": "torch-se", "pos": [3.0, 1.7, -4.0], "color": [1.0, 0.46, 0.14],
         "intensity": 36.0, "range": 11.0, "radius": 0.09, "shadow": True},
        {"tag": "bridge-lantern", "pos": [9.0, 0.88, 0.18], "color": [0.28, 0.86, 1.0],
         "intensity": 44.0, "range": 14.0, "radius": 0.10, "shadow": True},
    ],
}


def num(v):
    # alib6 的校验器严格区分 Int/Double：浮点字段必须带小数点
    s = f"{v:g}"
    return s if ("." in s or "e" in s) else s + ".0"


def nums(a):
    return "[" + ", ".join(num(v) for v in a) + "]"


def emit_light(e):
    return (
        '  {"tag":"%s", "pos":%s, "color":%s, "intensity":%s, "range":%s, '
        '"radius":%s, "shadow":%s}'
        % (
            e["tag"], nums(e["pos"]), nums(e["color"]), num(e["intensity"]),
            num(e["range"]), num(e["radius"]), str(e["shadow"]).lower(),
        )
    )


def emit_entry(e):
    parts = [
        f'"tag":"{e["tag"]}"',
        f'"pos":{nums(e["pos"])}',
        f'"axis":{nums(e.get("axis", [0.0, 1.0, 0.0]))}',
        f'"angle":{num(e.get("angle", 0.0))}',
        f'"scale":{nums(e["scale"])}',
        f'"material":"{e["material"]}"',
        f'"solid":{str(e.get("solid", True)).lower()}',
    ]
    if not e.get("shadow", True):
        parts.append('"shadow":false')
    return "  {" + ", ".join(parts) + "}"

lines = ["{"]
lines.append(' "materials": {')
mat_names = list(scene["materials"].keys())
for i, name in enumerate(mat_names):
    body = ", ".join(
        f'"{k}": {nums(v) if isinstance(v, list) else ("%d" % int(v) if k == "voxel" else num(v))}'
        for k, v in scene["materials"][name].items()
    )
    lines.append(f'  "{name}": {{{body}}}' + ("," if i + 1 < len(mat_names) else ""))
lines.append(" },")
lines.append(' "cubes": [')
lines.append(",\n".join(emit_entry(c) for c in cubes + crystal_entries))
lines.append(" ],")
d = scene["dragon"]
lines.append(
    f' "dragon": {{"tag":"{d["tag"]}", "pos":{nums(d["pos"])}, "axis":{nums(d["axis"])}, '
    f'"angle":{num(d["angle"])}, "scale":{nums(d["scale"])}, "material":"{d["material"]}", "solid":true}},'
)
lines.append(' "lights": [')
lines.append(",\n".join(emit_light(l) for l in scene["lights"]))
lines.append(" ]")
lines.append("}")

out = "\n".join(lines)

# 默认写到 <仓库>/assets/scene.json（运行目录）；也可传参指定输出路径
here = os.path.dirname(os.path.abspath(__file__))
out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.normpath(
    os.path.join(here, "..", "..", "..", "assets", "scene.json")
)
with open(out_path, "w") as f:
    f.write(out + "\n")

print("cubes:", len(cubes) + len(crystal_entries))
print("tags:", dict(collections.Counter(c["tag"] for c in cubes + crystal_entries)))
print("bytes:", len(out))
print("wrote:", out_path)
