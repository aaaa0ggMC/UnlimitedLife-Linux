#!/usr/bin/env python3
"""Compiler-free source consistency checks; not a GLSL/C++ compiler or runtime test."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def fields(block, matrix, vector):
    result = []
    for kind, name, count in re.findall(
        rf"\b({matrix}|{vector})\s+(\w+)\s*(?:\[\s*(\d+)\s*\])?\s*;", block
    ):
        result.append(("mat4" if kind == matrix else "vec4", name, int(count or 1)))
    return result


def main():
    header = (ROOT / "app.h").read_text()
    common = (ROOT / "shaders/fog_common.glsl").read_text()
    cpp_ubo = header.split("struct alignas(16) CameraFogUbo {", 1)[1].split("};", 1)[0]
    glsl_ubo = common.split("uniform CameraFogUbo {", 1)[1].split("} ubo;", 1)[0]
    cpp_fields = fields(cpp_ubo, "glm::mat4", "glm::vec4")
    glsl_fields = fields(glsl_ubo, "mat4", "vec4")
    require(cpp_fields == glsl_fields, "C++/GLSL CameraFogUbo field mismatch")
    offsets = {}
    size = 0
    for kind, name, count in cpp_fields:
        offsets[name] = size
        size += (64 if kind == "mat4" else 16) * count
    for name, value in re.findall(r"offsetof\(CameraFogUbo, (\w+)\) == (\d+)", header):
        require(offsets[name] == int(value), f"Wrong asserted offset for {name}")
    require(
        f"sizeof(CameraFogUbo) == {size}" in header,
        "Missing/mismatched UBO size assertion",
    )
    media = (ROOT / "shaders/fog_media.glsl").read_text()
    cpp_volume = header.split("struct alignas(16) GpuFogVolume {", 1)[1].split(
        "\n};", 1
    )[0]
    cpp_volume = re.sub(r"\s*\{[^{}]*\}(?=;)", "", cpp_volume)
    glsl_volume = media.split("struct GpuFogVolume {", 1)[1].split("};", 1)[0]
    volume_fields = fields(cpp_volume, "glm::mat4", "glm::vec4")
    require(
        volume_fields == fields(glsl_volume, "mat4", "vec4"),
        "CPU/GLSL GpuFogVolume mismatch",
    )
    volume_size = sum(
        (64 if kind == "mat4" else 16) * count for kind, _, count in volume_fields
    )
    require(
        f"sizeof(GpuFogVolume) == {volume_size}" in header, "GpuFogVolume size mismatch"
    )
    require(
        "max_local_fog_volumes = 16" in header
        and "min(local_fog.counts.x, 16u)" in media,
        "Review local fog count limit",
    )
    require(
        "sizeof(LocalFogUpload) == 2832" in header
        and "offsetof(LocalFogUpload, entries) == 16" in header,
        "Review local fog upload header/stride",
    )

    control_shader = (ROOT / "shaders/control_density.comp").read_text()
    control_common = (ROOT / "shaders/control_density.glsl").read_text()
    components = (ROOT / "app_comp.h").read_text()
    for cpp_name, shader_name in (
        ("GpuFogControlPoint", "ControlPoint"),
        ("GpuFogControlField", "ControlField"),
    ):
        cpp = header.split(f"struct alignas(16) {cpp_name} {{", 1)[1].split("\n};", 1)[
            0
        ]
        cpp = re.sub(r"\s*\{[^{}]*\}(?=;)", "", cpp)
        shader = control_shader.split(f"struct {shader_name} {{", 1)[1].split("};", 1)[
            0
        ]
        cpu_vectors = re.findall(r"glm::(u?vec4)\s+(\w+)\s*;", cpp)
        shader_vectors = re.findall(r"\b(u?vec4)\s+(\w+)\s*;", shader)
        require(cpu_vectors == shader_vectors, f"CPU/GLSL {cpp_name} field mismatch")
    require(
        "max_fog_control_points = 32" in components
        and "ControlPoint points[32]" in control_shader,
        "Control point capacity mismatch",
    )
    require(
        "std::array<GpuFogControlPoint, fog::max_fog_control_points> points" in header
        and "sizeof(GpuFogControlPoint) == 32" in header
        and "offsetof(GpuFogControlField, points) == 48" in header
        and "sizeof(GpuFogControlField) == 1072" in header,
        "Control field std430 stride mismatch",
    )
    require(
        "fog_control_grid_size = 64" in header
        and "CONTROL_GRID_SIZE = 64u" in control_common
        and "CONTROL_GRID_CELLS = 64u * 64u * 64u" in control_common,
        "CPU/GLSL control grid dimensions mismatch",
    )

    shader_dir = ROOT / "shaders"
    for path in shader_dir.iterdir():
        if path.suffix not in {".glsl", ".vert", ".frag", ".comp"}:
            continue
        for include in re.findall(r'#include "([^"]+)"', path.read_text()):
            require(
                (shader_dir / include).is_file(),
                f"Missing include {include} in {path.name}",
            )
    script = (ROOT / "build_shaders.sh").read_text()
    outputs = set(re.findall(r'\$output_dir/([^"\s]+\.spv)', script))
    references = set()
    for path in ROOT.glob("app*.cpp"):
        references.update(re.findall(r'fog/shaders/([^"\s]+\.spv)', path.read_text()))
    require(
        references <= outputs,
        f"SPIR-V outputs missing from build script: {references - outputs}",
    )

    expected = {
        "control_density.comp": {11, 12},
        "froxel.comp": {0, 4, 6, 9, 10, 11, 12, 14},
        "froxel_temporal.comp": {0, 6, 7, 8},
        "froxel_integrate.comp": {0, 3, 4, 8, 9, 10, 11, 14},
        "fog.frag": {0, 1, 3, 4, 9, 10, 11, 14, 15, 16, 17, 18, 19, 20, 21, 22},
        "sky.frag": {0, 1, 3},
        "shadow.vert": {0, 2},
        "light_cull.comp": {0, 12},
        "radiance_check.comp": {0, 3, 4, 9, 10, 11, 13, 14},
    }

    def expand(path):
        text = path.read_text()
        return re.sub(r'#include "([^"]+)"', lambda m: expand(shader_dir / m[1]), text)

    # SSBO 实例布局必须与 C++ PushConstant 的字段同步；模型矩阵按固定 stride 读取。
    for shader_name in ("fog_instanced.vert", "shadow.vert"):
        require(
            "material_params" in (ROOT / "shaders" / shader_name).read_text(),
            f"{shader_name}: instance layout is missing material_params",
        )

    for name, bindings in expected.items():
        actual = set(
            map(int, re.findall(r"binding\s*=\s*(\d+)", expand(shader_dir / name)))
        )
        require(actual == bindings, f"Unexpected bindings in {name}: {actual}")
    # Scene, sky, inject, temporal x2, integrate x2, shadow, control density generation.
    setup = (ROOT / "app_setup.cpp").read_text()
    for text in (
        "uniform_buffer(count * 12)",
        "combined_image_sampler(count * 40)",
        "storage_buffer(count * 32)",
        ".max_sets = count * 12",
    ):
        require(text in setup, f"Review descriptor pool accounting: {text}")
    print(
        f"Source consistency OK: {size}-byte UBO, {volume_size}-byte local medium, 1072-byte control field, shader includes/outputs, bindings, descriptor pool declarations"
    )
    print(
        "Not checked: C++/GLSL compilation, shader reflection, GPU synchronization or visual behavior"
    )


if __name__ == "__main__":
    main()
