#ifndef FOG_MEDIA_GLSL
#define FOG_MEDIA_GLSL
#include "control_density.glsl"
struct GpuFogVolume {
    mat4 world_to_local;
    vec4 scattering;
    vec4 params;
    vec4 tex_tiling;
    vec4 tex_offset;
    vec4 atlas_offset;
    vec4 atlas_scale;
    vec4 bounds;
};
layout(std430, set = 0, binding = 9) readonly buffer LocalFogVolumes {
    uvec4 counts;
    GpuFogVolume entries[];
} local_fog;
layout(set = 0, binding = 10) uniform sampler3D fog_density_atlas;

bool fog_inside_emitter(vec3 p) {
    if(ubo.local_light_info.z < 0.5) return false;
    uint count = min(uint(ubo.local_light_info.x), 8u);
    for(uint i = 0; i < count; ++i) {
        if(ubo.local_light_params[i].z < 0.5 || ubo.local_light_color_intensity[i].w <= 0.0) continue;
        vec4 light_range = ubo.local_light_position_range[i];
        float radius = ubo.local_light_params[i].x;
        vec3 b = p - light_range.xyz;
        if(dot(b, b) < radius * radius) return true;
    }
    return false;
}

float local_fog_density(GpuFogVolume medium, vec3 p) {
    vec3 delta = p - medium.bounds.xyz;
    if(dot(delta, delta) > medium.bounds.w * medium.bounds.w) return 0.0;
    vec3 local = (medium.world_to_local * vec4(p, 1.0)).xyz;
    vec3 q = abs(local) * 2.0;
    float radius = medium.params.z < 0.5 ? max(max(q.x, q.y), q.z) : length(q);
    if(radius >= 1.0) return 0.0;
    float fade = medium.params.y > 0.0001
        ? 1.0 - smoothstep(1.0 - medium.params.y, 1.0, radius) : 1.0;
    float modulation = 1.0;
    if(medium.atlas_scale.w > 0.5) {
        modulation = control_density_sample(uint(medium.atlas_scale.w + 0.5) - 1u, local + 0.5);
    } else if(medium.atlas_offset.w > 0.5 && medium.params.w > 0.0) {
        vec3 uv = (local + 0.5) * medium.tex_tiling.xyz + medium.tex_offset.xyz;
        uv = medium.tex_tiling.w > 0.5 ? fract(uv) : clamp(uv, vec3(0.0), vec3(1.0));
        float texture_density = textureLod(fog_density_atlas,
            medium.atlas_offset.xyz + uv * medium.atlas_scale.xyz, 0.0).r;
        modulation = mix(1.0, texture_density, medium.params.w);
    }
    return medium.params.x * fade * modulation;
}

// RGB is the scattering coefficient (including color/albedo), A is scalar extinction.
// Overlapping media add coefficients; they do not overwrite each other's color.
vec4 fog_medium(vec3 p, float global_weight) {
    if(fog_inside_emitter(p)) return vec4(0.0);
    float sigma = fog_global_density(p) * global_weight;
    vec4 result = vec4(sigma * clamp(ubo.fog_color.rgb, vec3(0.0), vec3(1.0))
        * clamp(ubo.volume_params.y, 0.0, 1.0), sigma);
    for(uint i = 0; i < min(local_fog.counts.x, 16u); ++i) {
        GpuFogVolume medium = local_fog.entries[i];
        float density = local_fog_density(medium, p);
        result += vec4(density * medium.scattering.rgb * medium.scattering.w, density);
    }
    return result;
}

float fog_density(vec3 p) {
    if(fog_inside_emitter(p)) return 0.0;
    float result = fog_global_density(p);
    for(uint i = 0; i < min(local_fog.counts.x, 16u); ++i)
        result += local_fog_density(local_fog.entries[i], p);
    return result;
}
#endif
