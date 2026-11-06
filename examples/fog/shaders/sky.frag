#version 460
#extension GL_GOOGLE_include_directive : require
#include "fog_common.glsl"
#include "froxel_sample.glsl"
#include "display.glsl"

layout(location = 0) in vec3 v_direction;
layout(location = 0) out vec4 out_color;
layout(set = 0, binding = 1) uniform sampler2D sky_texture;
layout(push_constant) uniform SkyPushConstant {
    /// x: 曝光, y: 是否使用全景图
    vec4 params;
} push;

const float PI = 3.141592653589793;

void main() {
    vec3 direction = normalize(v_direction);
    vec3 color;
    if(push.params.y > 0.5) {
        // 图像顶端对应 +Y，水平周期覆盖整个天空。
        vec2 uv = vec2(atan(direction.z, direction.x) / (2.0 * PI) + 0.5,
                       acos(clamp(direction.y, -1.0, 1.0)) / PI);
        color = texture(sky_texture, uv).rgb;
    } else {
        // 连续的程序天空背景；本地体积介质在下方统一合成。
        color = atmosphere_color(direction);
        vec3 sun_direction = normalize(ubo.light_direction.xyz);
        float sun = smoothstep(cos(0.035), cos(0.025), dot(direction, sun_direction));
        color += vec3(1.0, 0.85, 0.55) * sun;
    }

    // Background has no finite surface: sample the optically converged sky layer.
    vec4 fog = froxel_sky_sample(direction);
    // 预乘颜色 = fog.rgb；输出 straight alpha 后页面合成结果为 fog.rgb + bg * transmittance。
    if(push.params.z > 0.5) {
        float opacity = clamp(1.0 - fog.a, 0.0, 1.0);
        vec3 straight = opacity > 1e-4 ? fog.rgb / opacity : vec3(0.0);
        out_color = vec4(display_radiance(straight), opacity);
        return;
    }
    color = color * push.params.x * fog.a + fog.rgb;
    if(ubo.temporal.w > 2.5) color = vec3(1.0);
    else if(ubo.temporal.w > 1.5) color = fog.rgb;
    else if(ubo.temporal.w > 0.5) color = vec3(fog.a);
    out_color = vec4(display_radiance(color), 1.0);
}
