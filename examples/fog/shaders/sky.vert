#version 460
#extension GL_GOOGLE_include_directive : require
#include "fog_common.glsl"

layout(location = 0) in vec3 in_pos;
layout(location = 0) out vec3 v_direction;

void main() {
    // 天空只保留相机的旋转（去掉平移），并把 z 钉在远平面。
    mat4 sky_view = mat4(mat3(ubo.view));
    vec4 clip = ubo.proj * sky_view * vec4(in_pos, 1.0);
    gl_Position = clip.xyww;
    v_direction = in_pos;
}
