#version 460
#extension GL_GOOGLE_include_directive : require
#include "fog_common.glsl"
#include "lighting.glsl"
#ifndef FOG_MOBILE
// On mobile, do not retain the general interval solver in the fragment shader.
// A uniform runtime branch still leaves its arrays/loops in the compiled program.
#include "fog_surface.glsl"
#define FOG_SURFACE_REFINEMENT
#endif
#include "froxel_sample.glsl"
#include "display.glsl"

layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_normal;
layout(location = 3) in vec3 v_world_pos;
layout(location = 4) flat in vec4 v_params;
layout(location = 5) flat in vec4 v_base_color;
layout(location = 6) flat in vec3 v_stable_pos;
layout(location = 7) flat in vec4 v_material_params;
layout(location = 0) out vec4 out_color;
layout(set = 0, binding = 1) uniform sampler2D tex;

/// 可平铺的水面细法线（tangent space，xy 为坡度）。
layout(set = 0, binding = 15) uniform sampler2D water_normal_tex;
layout(set = 0, binding = 16) uniform sampler2D n_grass_top;
layout(set = 0, binding = 17) uniform sampler2D n_grass_side;
layout(set = 0, binding = 18) uniform sampler2D n_stone;
layout(set = 0, binding = 19) uniform sampler2D n_wood;
layout(set = 0, binding = 20) uniform sampler2D n_leaves;
layout(set = 0, binding = 21) uniform sampler2D n_sand;
layout(set = 0, binding = 22) uniform sampler2D n_roof;

/// 用世界空间 UV + 方块面法线构造切线基，避免每个面各自 0..1 UV 被拉伸。
vec3 apply_block_normal(int layer, vec3 base_normal) {
    vec3 T;
    vec3 B;
    if(abs(base_normal.y) > 0.5) {
        B = vec3(0.0, 0.0, base_normal.y > 0.0 ? -1.0 : 1.0);
    } else {
        B = vec3(0.0, 1.0, 0.0);
    }
    T = normalize(cross(B, base_normal));

    float scale = layer == 1 ? 0.40 : layer == 2 ? 0.30 : layer == 3 ? 0.30
        : layer == 4 ? 0.55 : layer == 5 ? 0.40 : 0.30;
    vec2 uv = vec2(dot(v_world_pos, T), dot(v_world_pos, B)) * scale;
    vec3 encoded;
    if(layer == 1) {
        encoded = base_normal.y > 0.5 ? texture(n_grass_top, uv).xyz
                                      : texture(n_grass_side, uv).xyz;
    } else if(layer == 2) {
        encoded = texture(n_stone, uv).xyz;
    } else if(layer == 3) {
        encoded = texture(n_wood, uv).xyz;
    } else if(layer == 4) {
        encoded = texture(n_leaves, uv).xyz;
    } else if(layer == 5) {
        encoded = texture(n_sand, uv).xyz;
    } else if(layer == 6) {
        // 瓦片法线只用于屋顶顶面；屋檐侧面/底面保持平滑的漆面。
        if(base_normal.y < 0.5) return base_normal;
        encoded = texture(n_roof, uv).xyz;
    } else {
        return base_normal;
    }
    vec3 nt = encoded * 2.0 - 1.0;
    float strength = layer == 1 ? 0.70 : layer == 2 ? 0.70 : layer == 3 ? 0.50
        : layer == 4 ? 0.60 : layer == 5 ? 0.35 : 0.45;
    // 没有 mipmap 的细节在远处会闪烁；淡化到 25% 而不是完全消失。
    float view_distance = length(ubo.camera_position.xyz - v_world_pos);
    float detail = 1.0 - smoothstep(18.0, 60.0, view_distance);
    nt.xy *= strength * mix(0.25, 1.0, detail);
    return normalize(T * nt.x + B * nt.y + base_normal * nt.z);
}


void main() {
    // Reserved marker material: ignore sun/local lighting, but retain fog and debug views.
    if(v_params.w < -0.5) {
        // All surfaces, including HDR lamp cores, use the same full-view volume.
        vec4 fog = froxel_sample(v_world_pos);
        out_color = vec4(display_emissive_surface(fog_composite_sample(vec3(0.0), fog, 1.0),
            vec3(0.0), v_base_color.rgb * max(v_base_color.w, 0.0), fog.a), 1.0);
        return;
    }
    vec3 N = normalize(v_normal);
    vec3 albedo = v_base_color.rgb;
    if(v_params.x < 0.5) albedo *= texture(tex, v_uv).rgb;
    int voxel = int(v_params.w + 0.5);
    if(voxel == 5) {
        // 水面是薄层：只保留朝向相机的一侧。否则每个水块的侧面/底面也会
        // 透过半透明顶面叠加，形成经典的水方块网格。
        const bool above_water = ubo.camera_position.y > v_world_pos.y;
        if(above_water ? N.y < 0.5 : N.y > -0.5) discard;
    }
    if(voxel == 5 && N.y > 0.5) {
        // 预生成噪声法线做两层不同尺度的流动，再叠加低频 sin 波形成大浪。
        float t = ubo.surface_params.x;
        vec2 p = v_world_pos.xz;
        vec2 uv1 = p * 0.36 + vec2(t * 0.032, t * 0.019);
        vec2 uv2 = p * 0.93 + vec2(-t * 0.051, t * 0.031);
        vec3 n1 = texture(water_normal_tex, uv1).xyz * 2.0 - 1.0;
        vec3 n2 = texture(water_normal_tex, uv2).xyz * 2.0 - 1.0;
        vec2 sine = vec2(
            cos(p.x * 3.7 + t * 1.9) * 0.080 + cos((p.x + p.y) * 2.3 - t * 1.3) * 0.050,
            sin(p.y * 3.1 - t * 1.5) * 0.080 + sin((p.x - p.y) * 2.7 + t * 1.1) * 0.050);
        vec2 slope = (n1.xy * 0.45 + n2.xy * 0.28 + sine) * ubo.surface_params.z;
        N = normalize(N + vec3(slope.x, 0.0, slope.y));
    }
    if(voxel == 4) {
        // Minecraft 式树叶：规则网格内挖洞，保留方块轮廓但透出天空/后方。
        // 图案只依赖面 UV 与实例原点；不能使用摇摆后的世界坐标，否则洞会随动画游动。
        vec2 cell = floor(v_uv * 8.0);
        float seed = dot(floor(v_stable_pos.xz * 4.0) + floor(v_stable_pos.y * 4.0), vec2(13.7, 7.3));
        float hole = fog_hash21(cell + seed);
        if(hole < 0.26) discard;
    }
    // 水面不参与方块噪点/描边，否则每个水块都会出现网格，掩盖法线波纹。
    if(voxel > 0 && voxel != 5) {
        vec2 pixel = floor(v_uv * 16.0);
        float grain = fog_hash21(pixel);
        if(voxel == 1 && N.y < 0.5) {
            // 草方块侧面的草皮边缘，下面为泥土；底面不涂草。
            albedo = N.y > -0.5 && v_uv.y > 0.78 + grain * 0.1
                ? albedo : vec3(0.36, 0.23, 0.12);
        }
        if(voxel == 3) grain = fog_hash21(vec2(pixel.x, floor(pixel.y / 4.0)));
        albedo *= 0.82 + grain * 0.30;
        // 明确块边界，不依赖第三方 Minecraft 贴图。
        float edge = min(min(v_uv.x, 1.0 - v_uv.x), min(v_uv.y, 1.0 - v_uv.y));
        albedo *= mix(0.78, 1.0, step(0.025, edge));
    }
    // 方块法线贴图：草/石/木/叶/沙/瓦各自一张，世界空间平铺。
    int normal_layer = int(v_material_params.x + 0.5);
    if(voxel > 0 && voxel != 5 && normal_layer > 0)
        N = apply_block_normal(normal_layer, normalize(v_normal));

    vec3 L = normalize(ubo.light_direction.xyz);
    vec3 to_eye = ubo.camera_position.xyz - v_world_pos;
    vec3 V = to_eye * inversesqrt(max(dot(to_eye, to_eye), 1e-8));
    // 水面用反射天空色体现法线扰动；菲涅耳让掠射角更亮，近看仍保留水色。
    float water_mix = 0.0;
    vec3 water_sky = vec3(0.0);
    if(voxel == 5 && N.y > 0.5) {
        vec3 R = reflect(-V, N);
        water_sky = atmosphere_color(R);
        float fresnel = pow(1.0 - clamp(dot(N, V), 0.0, 1.0), 5.0);
        water_mix = 0.08 + 0.35 * fresnel;
    }
    vec3 half_vector = L + V;
    vec3 H = half_vector * inversesqrt(max(dot(half_vector, half_vector), 1e-8));
    float diffuse = max(dot(N, L), 0.0);
    // 背光面禁止高光，L 与 V 相反时保护半角向量归一化。
    float specular = diffuse > 0.0
        ? pow(max(dot(N, H), 0.0), max(v_params.z, 1.0)) * v_params.y * ubo.light_color.w
        : 0.0;
    vec3 shadow_position = v_world_pos + N * max(ubo.shadow_params.z, 0.0);
    // 表面 PCF 半径经 shadow_params.w 下发：桌面 2（5×5 = 25 tap），移动端 0（单 tap）。
    int surface_radius = int(ubo.shadow_params.w);
    float sun_visibility = shadow_visibility(shadow_position, 0, ubo.shadow_params.y, surface_radius);
    vec3 scene = albedo * ubo.light_direction.w
        + sun_visibility * ubo.light_color.rgb * (albedo * diffuse + vec3(specular));
    vec3 delta;
    float distance;
    vec3 local_L;
    float local_diffuse;
    const uint local_count = min(uint(ubo.local_light_info.x), 8u);
    for(uint i = 0; i < local_count; ++i) {
        vec4 light_range = ubo.local_light_position_range[i];
        vec4 light_color = ubo.local_light_color_intensity[i];
        if(light_color.w <= 0.0) continue;
        delta = light_range.xyz - v_world_pos;
        distance = length(delta);
        if(distance >= light_range.w) continue;
        local_L = delta / max(distance, 1e-4);
        local_diffuse = max(dot(N, local_L), 0.0);
        if(local_diffuse <= 0.0) continue;
        float point = light_attenuation(distance, light_range.w,
            max(ubo.local_light_params[i].x, 0.001)) * light_color.w;
        if(point <= 0.0) continue;
        float visibility = point_shadow_visibility(shadow_position, light_range.xyz, light_range.w,
            int(ubo.local_light_params[i].y), ubo.shadow_params.y, surface_radius);
        if(visibility <= 0.0) continue;
        vec3 local_H = local_L + V;
        local_H *= inversesqrt(max(dot(local_H, local_H), 1e-8));
        float local_specular = pow(max(dot(N, local_H), 0.0), max(v_params.z, 1.0)) * v_params.y;
        scene += (albedo * local_diffuse + vec3(local_specular)) * light_color.rgb * point * visibility
            * medium_visibility(v_world_pos, local_L, distance) / FOG_PI;
    }

    if(water_mix > 0.0) scene = mix(scene, water_sky, water_mix);
    vec4 fog = froxel_sample(v_world_pos);
    vec3 final_color = display_emissive_surface(fog_composite_sample(scene, fog, sun_visibility),
        scene, albedo * max(v_base_color.w, 0.0), fog.a);
    // 水面在半透明批次绘制；其他材质始终不透明。
    float surface_alpha = voxel == 5 ? clamp(ubo.surface_params.w, 0.05, 1.0) : 1.0;
    out_color = vec4(final_color, surface_alpha);
}
