#!/usr/bin/env bash
set -euo pipefail
source_dir="$(cd -- "$(dirname -- "$0")" && pwd)/shaders"
output_dir="${1:-$(dirname -- "$source_dir")/../../assets/fog/shaders}"
mkdir -p "$output_dir"
# Desktop shaders remain unchanged. Android packaging explicitly opts in.
shader_defines=()
if [[ "${FOG_MOBILE:-0}" == 1 ]]; then
    shader_defines+=(-DFOG_MOBILE=1)
fi
# 运行时纹理与 shader 一样从源码目录同步到 assets/fog。
texture_dir="$(dirname -- "$source_dir")/textures"
if [ -d "$texture_dir" ]; then
    mkdir -p "$(dirname -- "$output_dir")/textures"
    cp -f "$texture_dir"/* "$(dirname -- "$output_dir")/textures/"
fi
# fog_common.glsl 通过 -I 被各 shader include（GL_GOOGLE_include_directive）
glslangValidator -I"$source_dir" -V "$source_dir/fog.vert" -o "$output_dir/fog-vert.spv"
glslangValidator -I"$source_dir" -V "$source_dir/fog_instanced.vert" -o "$output_dir/fog-instanced-vert.spv"
glslangValidator -I"$source_dir" "${shader_defines[@]}" -V "$source_dir/fog.frag" -o "$output_dir/fog-frag.spv"
glslangValidator -I"$source_dir" -V "$source_dir/sky.vert" -o "$output_dir/sky-vert.spv"
glslangValidator -I"$source_dir" -V "$source_dir/sky.frag" -o "$output_dir/sky-frag.spv"
glslangValidator -I"$source_dir" -V "$source_dir/froxel.comp" -o "$output_dir/froxel.spv"
glslangValidator -I"$source_dir" -V "$source_dir/light_cull.comp" -o "$output_dir/light-cull.spv"
glslangValidator -I"$source_dir" -V "$source_dir/control_density.comp" -o "$output_dir/control-density.spv"
glslangValidator -I"$source_dir" -V "$source_dir/froxel_temporal.comp" -o "$output_dir/froxel-temporal.spv"
glslangValidator -I"$source_dir" -V "$source_dir/froxel_integrate.comp" -o "$output_dir/froxel-integrate.spv"
glslangValidator -I"$source_dir" -V "$source_dir/shadow.vert" -o "$output_dir/shadow-vert.spv"
glslangValidator -I"$source_dir" -DINSTANCED=1 -V "$source_dir/shadow.vert" -o "$output_dir/shadow-instanced-vert.spv"
glslangValidator -V "$source_dir/shadow.frag" -o "$output_dir/shadow-frag.spv"
# radiance_check.comp 可能被临时移除（WIP），缺失时跳过，不阻断其他 shader
if [ -f "$source_dir/radiance_check.comp" ]; then
    glslangValidator -I"$source_dir" -V "$source_dir/radiance_check.comp" -o "$output_dir/radiance-check.spv"
fi
