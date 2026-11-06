#ifndef FOG_LIGHTING_GLSL
#define FOG_LIGHTING_GLSL
#include "fog_media.glsl"
layout(set = 0, binding = 4) uniform sampler2D sun_shadow;
/// 点光 cube shadow 图集：每盏灯的 6 个面依次占 base+0..base+5 层。
layout(set = 0, binding = 14) uniform sampler2DArray point_shadow_atlas;

float shadow_visibility(vec3 p, int light, float bias, int radius) {
    if(ubo.shadow_params.x < 0.5) return 1.0;
    vec4 clip = ubo.light_view_proj[light] * vec4(p, 1.0);
    if(clip.w <= 0.0) return 1.0;
    vec3 projected = clip.xyz / clip.w;
    vec2 uv = projected.xy * 0.5 + 0.5;
    if(any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))
        || projected.z < 0.0 || projected.z > 1.0) return 1.0;
    vec2 texel = 1.0 / vec2(textureSize(sun_shadow, 0));
    float visible = 0.0;
    // Manual PCF works with nearest-filter depth formats, without optional linear filtering.
    for(int y = -radius; y <= radius; ++y) for(int x = -radius; x <= radius; ++x) {
        vec2 tap = uv + vec2(x, y) * texel;
        float depth = textureLod(sun_shadow, tap, 0.0).r;
        visible += projected.z - bias <= depth ? 1.0 : 0.0;
    }
    return visible / float((radius * 2 + 1) * (radius * 2 + 1));
}

/// 体积积分每步调用，保持 3×3；表面着色显式传 radius=2 得到 5×5。
float shadow_visibility(vec3 p, int light, float bias) {
    return shadow_visibility(p, light, bias, 1);
}

// The six faces are rendered with lookAt(pos, pos + dir, up) and a 90° Y-flipped
// projection; this is the matching direction -> (face, uv) mapping.
const vec3 CUBE_DIR[6] = vec3[6](vec3(1, 0, 0), vec3(-1, 0, 0), vec3(0, 1, 0),
                                 vec3(0, -1, 0), vec3(0, 0, 1), vec3(0, 0, -1));
const vec3 CUBE_UP[6] = vec3[6](vec3(0, -1, 0), vec3(0, -1, 0), vec3(0, 0, 1),
                                vec3(0, 0, -1), vec3(0, -1, 0), vec3(0, -1, 0));

vec2 cube_face_uv(vec3 d, out int face) {
    vec3 a = abs(d);
    if(a.x >= a.y && a.x >= a.z) face = d.x >= 0.0 ? 0 : 1;
    else if(a.y >= a.z) face = d.y >= 0.0 ? 2 : 3;
    else face = d.z >= 0.0 ? 4 : 5;
    vec3 axis = CUBE_DIR[face];
    vec3 right = normalize(cross(axis, CUBE_UP[face]));
    vec3 up = cross(right, axis);
    float depth = max(dot(d, axis), 1e-5);
    return vec2(0.5 + 0.5 * dot(d, right) / depth, 0.5 + 0.5 * (-dot(d, up)) / depth);
}

// Reconstruct the same RH_ZO depth the face projection wrote, so a manual
// comparison works without a cube comparison sampler.
float cube_shadow_ndc(float distance, float light_range, float bias) {
    float near_plane = 0.05;
    float far_plane = max(light_range, near_plane + 0.1);
    float ndc = far_plane / (far_plane - near_plane)
        - far_plane * near_plane / ((far_plane - near_plane) * max(distance, near_plane));
    return ndc - bias;
}

float point_shadow_hard(vec3 p, vec3 light_position, float light_range, int base_layer, float bias) {
    if(base_layer < 0 || ubo.local_light_info.w < 0.5 || ubo.shadow_params.x < 0.5) return 1.0;
    vec3 d = p - light_position;
    int face;
    vec2 uv = cube_face_uv(d, face);
    // The face projection stores depth along the face axis, not Euclidean distance.
    float axis_distance = max(dot(d, CUBE_DIR[face]), 1e-5);
    vec2 texel = 1.0 / vec2(textureSize(point_shadow_atlas, 0).xy);
    // Keep taps inside this face: sampling the white border would draw bright seams.
    uv = clamp(uv, texel * 0.5, vec2(1.0) - texel * 0.5);
    float stored = textureLod(point_shadow_atlas, vec3(uv, float(base_layer + face)), 0.0).r;
    return cube_shadow_ndc(axis_distance, light_range, bias) <= stored ? 1.0 : 0.0;
}

float point_shadow_visibility(vec3 p, vec3 light_position, float light_range, int base_layer, float bias, int radius) {
    if(base_layer < 0 || ubo.local_light_info.w < 0.5 || ubo.shadow_params.x < 0.5) return 1.0;
    vec3 d = p - light_position;
    int face;
    vec2 uv = cube_face_uv(d, face);
    float axis_distance = max(dot(d, CUBE_DIR[face]), 1e-5);
    float compare = cube_shadow_ndc(axis_distance, light_range, bias);
    float layer = float(base_layer + face);
    vec2 texel = 1.0 / vec2(textureSize(point_shadow_atlas, 0).xy);
    float visible = 0.0;
    for(int y = -radius; y <= radius; ++y) for(int x = -radius; x <= radius; ++x) {
        vec2 tap = clamp(uv + vec2(x, y) * texel, texel * 0.5, vec2(1.0) - texel * 0.5);
        float stored = textureLod(point_shadow_atlas, vec3(tap, layer), 0.0).r;
        visible += compare <= stored ? 1.0 : 0.0;
    }
    return visible / float((radius * 2 + 1) * (radius * 2 + 1));
}

float light_range_window(float distance, float range) {
    float window = clamp(1.0 - pow(distance / max(range, 0.1), 4.0), 0.0, 1.0);
    return window * window;
}

float light_attenuation(float distance, float range, float radius) {
    return light_range_window(distance, range) / max(distance * distance, radius * radius);
}

#include "finite_light.glsl"

float medium_visibility(vec3 p, vec3 to_light, float distance) {
    int steps = clamp(int(ubo.volume_params.w), 0, 8);
    if(steps == 0) return 1.0;
    float span = max(distance, 0.0) / float(steps);
    float tau = 0.0;
    for(int i = 0; i < steps; ++i)
        tau += fog_density(p + to_light * ((float(i) + 0.5) * span)) * span;
    return exp(-min(tau, 80.0));
}

vec3 local_light_scattering(vec3 p, vec3 ray, float footprint, uint mask) {
    vec3 result = vec3(0.0);
    if(ubo.local_light_info.z < 0.5) return result;
    uint count = min(uint(ubo.local_light_info.x), 8u);
    for(uint i = 0; i < count; ++i) {
        if((mask & (1u << i)) == 0u) continue;
        vec4 pr = ubo.local_light_position_range[i];
        vec4 ci = ubo.local_light_color_intensity[i];
        if(ci.w <= 0.0) continue;
        vec3 delta = pr.xyz - p;
        float distance = length(delta);
        if(distance >= pr.w) continue;
        vec3 L = delta / max(distance, 1e-4);
        float radius = max(ubo.local_light_params[i].x, 0.001);
        float visibility = point_shadow_hard(p, pr.xyz, pr.w,
            int(ubo.local_light_params[i].y), ubo.shadow_params.y);
        if(visibility <= 0.0) continue;
        result += ci.rgb * finite_light_scattering(ci.w, distance, radius, footprint, dot(ray, L))
            * visibility * medium_visibility(p, L, distance);
    }
    return result;
}

vec3 volume_illumination_masked(vec3 p, vec3 ray, float footprint, uint mask) {
    vec3 sun = ubo.light_direction.xyz;
    // local_fog_info.w：mascot 截帧把雾当作舞台薄纱，让太阳不受龙体阴影遮蔽，
    // 并用雾色给雾一个偏蓝的环境底光；表面着色仍使用正常的太阳阴影。
    const bool veil = ubo.local_fog_info.w > 0.5;
    vec3 illumination = veil
        ? ubo.fog_color.rgb * max(ubo.light_direction.w, 0.82)
        : vec3(max(ubo.light_direction.w, 0.0));
    float visibility = veil ? 1.0 : shadow_visibility(p, 0, ubo.shadow_params.y);
    if(visibility > 0.0) illumination += ubo.light_color.rgb * phase_hg(dot(ray, sun))
        // The legacy sun color is diffuse irradiance / pi (surface shader convention).
        * visibility * medium_visibility(p, sun, ubo.volume_params.z) * FOG_PI;
    illumination += local_light_scattering(p, ray, footprint, mask);
    return max(illumination, vec3(0.0));
}

vec3 volume_illumination(vec3 p, vec3 ray, float footprint) {
    return volume_illumination_masked(p, ray, footprint, 0xffffffffu);
}
#endif
