#version 460
#extension GL_GOOGLE_include_directive : require
#include "fog_common.glsl"
layout(location = 0) in vec3 in_pos;
layout(push_constant) uniform ShadowPush { mat4 model; mat4 light_view_proj; } push;
#ifdef INSTANCED
// 必须与 C++ PushConstant / fog_instanced.vert 的 SSBO 布局一致（含材质参数）。
struct Instance { mat4 model; vec4 params; vec4 base_color; vec4 material_params; };
layout(std430, set = 0, binding = 2) readonly buffer StaticInstances { Instance instances[]; };
#endif
void main() {
#ifdef INSTANCED
    mat4 model = instances[gl_InstanceIndex].model;
#else
    mat4 model = push.model;
#endif
    gl_Position = push.light_view_proj * model * vec4(in_pos, 1.0);
}
