#version 460
#extension GL_GOOGLE_include_directive : require
#include "fog_common.glsl"

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out vec3 v_normal;
layout(location = 3) out vec3 v_world_pos;
layout(location = 4) flat out vec4 v_params;
layout(location = 5) flat out vec4 v_base_color;
/// 实例原点（不含摇摆）：树叶挖洞图案必须稳定，不能随顶点位移沸腾。
layout(location = 6) flat out vec3 v_stable_pos;
layout(location = 7) flat out vec4 v_material_params;
struct Instance {
    /// 模型矩阵（每个实例一块）
    mat4 model;
    /// x: solid, y: specular, z: shininess, w: voxel 类型
    vec4 params;
    /// rgb: 颜色, w: emission
    vec4 base_color;
    /// x: 方块法线图层
    vec4 material_params;
};
layout(std430, set = 0, binding = 2) readonly buffer StaticInstances {
    /// 每个实例一块（布局同 PushConstant）
    Instance instances[];
};
void main() {
    Instance instance = instances[gl_InstanceIndex];
    vec4 world_pos = instance.model * vec4(in_pos, 1.0);
    // 树叶随风轻摆：整块刚性平移，避免四个顶点各自位移把方块扭出对角线折痕。
    int voxel = int(instance.params.w + 0.5);
    if(voxel == 4) {
        vec3 origin = instance.model[3].xyz;
        float wind = ubo.surface_params.y;
        float t = ubo.surface_params.x;
        world_pos.x += sin(t * 1.7 + origin.z * 0.8 + origin.y * 0.6) * 0.08 * wind;
        world_pos.z += cos(t * 1.3 + origin.x * 0.7) * 0.06 * wind;
    }
    vec4 view_pos = ubo.view * world_pos;
    v_world_pos = world_pos.xyz;
    v_stable_pos = instance.model[3].xyz;
    v_uv = in_uv;
    v_normal = transpose(inverse(mat3(instance.model))) * in_normal;
    v_params = instance.params;
    v_base_color = instance.base_color;
    v_material_params = instance.material_params;
    gl_Position = ubo.proj * view_pos;
}
