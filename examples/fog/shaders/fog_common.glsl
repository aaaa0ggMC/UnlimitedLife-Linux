// Froxel 雾公共部分：UBO 布局、世界空间密度场和天空背景色。
// 被 fog.vert / fog_instanced.vert / fog.frag / sky.vert / sky.frag 共用（build_shaders.sh 提供 -I）。
// 体积积分见 froxel.comp；采样见 froxel_sample.glsl。
// C++ 侧的 UBO 布局见 app.h 的 CameraFogUbo。
#ifndef FOG_COMMON_GLSL
#define FOG_COMMON_GLSL

layout(set = 0, binding = 0) uniform CameraFogUbo {
    mat4 view;
    mat4 proj;
    /// rgb: 雾/散射色
    vec4 fog_color;
    /// x: 可选近端裁剪, y: 网格视深覆盖（相机far）, z: sigma_mist, w: 噪声扰动强度
    vec4 fog_params;
    /// xyz: 相机世界坐标
    vec4 camera_position;
    /// xyz: 指向太阳, w: 环境光
    vec4 light_direction;
    /// rgb: 太阳辐射色, w: 高光开关
    vec4 light_color;
    /// x: 高度衰减 k, y: 参考面高度 ground, z: 噪声频率, w: 全空间基础密度 sigma_base
    vec4 fog_extra;
    /// x: 动画相位 phi, y: 动画幅度, z: 行波频率, w: 全局近端裁剪开关
    vec4 fog_anim;
    mat4 inverse_view_proj;
    vec4 froxel_grid;
    mat4 light_view_proj[2];
    mat4 previous_view;
    mat4 previous_view_proj;
    vec4 shadow_params;
    /// x: local light count, y: shadowed count, z: lighting enabled, w: shadows enabled
    vec4 local_light_info;
    vec4 local_light_position_range[8];
    vec4 local_light_color_intensity[8];
    vec4 local_light_params[8];
    vec4 volume_params;
    vec4 fog_shape;
    vec4 temporal;
    vec4 local_fog_info;
    vec4 display_params;
    vec4 surface_params;
} ubo;

// Physical mode has no camera-centered void. start is an opt-in artistic clip,
// never a boundary of the world-space medium or a cut on local FogVolume shapes.
float fog_global_view_start() {
    return ubo.fog_anim.w > 0.5 ? max(ubo.fog_params.x, 0.0) : 0.0;
}

float fog_view_integration_start() {
    return ubo.local_fog_info.x > 0.5 ? 0.0 : fog_global_view_start();
}

float fog_global_injection_weight(float distance) {
    // If locals exist, the integral starts at zero and only the global part is gated.
    // Without locals the integrator clips the interval itself, avoiding double coverage weighting.
    return ubo.local_fog_info.x > 0.5 && distance < fog_global_view_start() ? 0.0 : 1.0;
}


/// 2D 哈希（密度场的低频噪声与方块颗粒共用）
float fog_hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

/// 平滑值噪声：低频疏密结构
float fog_value_noise(vec2 p) {
    const vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    const float a = fog_hash21(i);
    const float b = fog_hash21(i + vec2(1.0, 0.0));
    const float c = fog_hash21(i + vec2(0.0, 1.0));
    const float d = fog_hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

/// 世界坐标 p 处的相对噪声扰动（1 附近）
float fog_noise_factor(vec3 p) {
    // A 3D field, advected in world space; no camera-dependent noise coordinates.
    vec3 q = p * max(ubo.fog_extra.z, 0.0001)
        + vec3(0.8, 0.1, 0.35) * ubo.fog_anim.x * ubo.fog_shape.w;
    float z = floor(q.z);
    float f = fract(q.z);
    f = f * f * (3.0 - 2.0 * f);
    float n = mix(fog_value_noise(q.xy + z * vec2(19.1, 7.7)),
                  fog_value_noise(q.xy + (z + 1.0) * vec2(19.1, 7.7)), f);
    n = n * 0.7 + fog_value_noise(q.xz * 2.7 + q.y * 0.3 + 13.1) * 0.3;
    const float amount = clamp(ubo.fog_params.w, 0.0, 1.0);
    return mix(1.0, n * 2.0, amount);
}

/// 时间脉动因子（1 附近）：呼吸 + 行波，公式与理由见 fog_common.md 第 5 节。
float fog_anim_factor(vec3 p) {
    const float phi = ubo.fog_anim.x;
    const float amp = clamp(ubo.fog_anim.y, 0.0, 0.9);
    if(amp <= 1e-5) return 1.0;

    // 频率比 1 : sqrt(phi) : phi（无理数且彼此接近 -> 拍频长，不显短循环）
    const float breath = 0.50 * sin(phi)
                       + 0.30 * sin(1.2720196 * phi + 1.7)
                       + 0.20 * sin(1.6180340 * phi + 0.4);
    // 行波作用于所有密度路径：已不存在单独的解析天空雾
    const vec2 k = vec2(0.83, 0.55);                       // 固定波矢方向（与相机无关）
    const float two_pi_f = 6.2831853 * max(ubo.fog_anim.z, 1e-4);
    const float phase = dot(p.xz, k) * two_pi_f - 1.2720196 * phi;
    const float warp = 0.45 * sin(dot(p.xz, k.yx) * two_pi_f * 0.59 + 1.09 * phi);
    const float signal = 0.55 * breath + 0.45 * sin(phase + warp);
    return max(0.0, 1.0 + amp * signal);
}

/// Froxel 中点的消光系数（世界单位的倒数）。
float fog_global_density(vec3 p) {
    const float base = max(ubo.fog_extra.w, 0.0);          // 全空间基础密度
    const float mist = max(ubo.fog_params.z, 0.0)          // 近地面附加密度
        * exp(-max(p.y - ubo.fog_extra.y, 0.0) * max(ubo.fog_extra.x, 0.0))
        * fog_noise_factor(p);
    float density = base + mist;
    if(ubo.fog_shape.x > 2.5) {
        float h = (p.y - ubo.fog_shape.y) / max(ubo.fog_shape.z, 0.1);
        density = (base + max(ubo.fog_params.z, 0.0)) * exp(-0.5 * h * h)
            * fog_noise_factor(p);
    } else if(ubo.fog_shape.x > 1.5) {
        // Patchy weather modulates the whole medium, not just the ground term.
        density *= smoothstep(0.30, 1.20, fog_noise_factor(p)) * 2.0;
    }
    return max(density * fog_anim_factor(p), 0.0);
}

float froxel_depth_at(float slice) {
    float end = max(ubo.fog_params.y, 0.001);
    return ubo.froxel_grid.w * (exp(log(1.0 + end / ubo.froxel_grid.w)
        * slice / ubo.froxel_grid.z) - 1.0);
}

vec3 froxel_ray(vec2 uv) {
    vec4 far_point = ubo.inverse_view_proj * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    return normalize(far_point.xyz / far_point.w - ubo.camera_position.xyz);
}

vec3 froxel_center(ivec3 cell) {
    vec3 ray = froxel_ray((vec2(cell.xy) + 0.5) / ubo.froxel_grid.xy);
    float cosine = max(-(ubo.view * vec4(ray, 0.0)).z, 1e-4);
    return ubo.camera_position.xyz + ray * froxel_depth_at(float(cell.z) + 0.5) / cosine;
}

/// 程序天空背景：天顶与雾海在地平线处连续，本地散射由 froxel 单独计算。
vec3 atmosphere_color(vec3 dir) {
    const vec3 horizon = vec3(0.72, 0.82, 0.92);
    const float elevation = smoothstep(0.0, 1.0, max(dir.y, 0.0));
    const vec3 sky = mix(horizon, vec3(0.12, 0.32, 0.68), elevation);
    const vec3 sea = mix(horizon, ubo.fog_color.rgb, smoothstep(0.0, 0.35, -dir.y));
    return mix(sky, sea, smoothstep(0.0, 0.02, -dir.y));
}

#endif
