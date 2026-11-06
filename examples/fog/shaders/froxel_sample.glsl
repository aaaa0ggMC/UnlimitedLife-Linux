#ifndef FROXEL_SAMPLE_GLSL
#define FROXEL_SAMPLE_GLSL
layout(std430, set = 0, binding = 3) readonly buffer FroxelVolume {
    vec4 cells[];
} volume;

vec4 froxel_fetch(ivec3 p) {
    ivec3 grid = ivec3(ubo.froxel_grid.xyz);
    p = clamp(p, ivec3(0), ivec3(grid.xy - 1, grid.z));
    return volume.cells[(p.z * grid.y + p.y) * grid.x + p.x];
}

vec4 froxel_sample(vec3 world_position) {
    // Diagnostic no-volume mode: no SSBO reads or per-pixel refinement.
    if(ubo.local_fog_info.z > 0.5) return vec4(0.0, 0.0, 0.0, 1.0);
#ifdef FOG_SURFACE_REFINEMENT
    float refinement_weight = fog_surface_refinement_weight(world_position);
#endif
    vec4 clip = ubo.proj * ubo.view * vec4(world_position, 1.0);
    vec2 uv = clip.xy / max(clip.w, 1e-5) * 0.5 + 0.5;
    float depth = max(-(ubo.view * vec4(world_position, 1.0)).z, 0.0);
    float end = max(ubo.fog_params.y, 0.001);
    float slice = log(1.0 + min(depth, end) / ubo.froxel_grid.w)
        / log(1.0 + end / ubo.froxel_grid.w) * ubo.froxel_grid.z;
    // Select a pair of integration boundaries, including the final boundary at grid.z.
    slice = clamp(slice, 0.0, max(ubo.froxel_grid.z - 0.0001, 0.0));
    vec3 position = vec3(uv * ubo.froxel_grid.xy - 0.5, slice);
    ivec3 base = ivec3(floor(position));
    vec3 f = fract(position);
    vec4 lower = mix(mix(froxel_fetch(base), froxel_fetch(base + ivec3(1, 0, 0)), f.x),
        mix(froxel_fetch(base + ivec3(0, 1, 0)), froxel_fetch(base + ivec3(1, 1, 0)), f.x), f.y);
    float depth_a = froxel_depth_at(float(base.z));
    float depth_b = froxel_depth_at(float(base.z + 1));
    base.z += 1;
    vec4 upper = mix(mix(froxel_fetch(base), froxel_fetch(base + ivec3(1, 0, 0)), f.x),
        mix(froxel_fetch(base + ivec3(0, 1, 0)), froxel_fetch(base + ivec3(1, 1, 0)), f.x), f.y);
    // Log-slice interpolation of already integrated S/T is not a partial ray integral.
    // Reconstruct the same constant-coefficient segment used by the integration pass.
    float distance = length(world_position - ubo.camera_position.xyz);
    float cosine = max(depth / max(distance, 1e-5), 1e-4);
    float start = fog_view_integration_start();
    float a = max(depth_a / cosine, start);
    float b = max(depth_b / cosine, start);
    float fraction = b > a ? clamp((distance - a) / (b - a), 0.0, 1.0) : 0.0;
    float step_t = clamp(upper.a / max(lower.a, 1e-30), 0.0, 1.0);
    float tau = -log(max(step_t, 1e-30));
    float partial_t = exp(-tau * fraction);
    float scatter_fraction = tau < 0.001 ? fraction
        : (1.0 - partial_t) / max(1.0 - step_t, 1e-8);
    vec4 result = vec4(mix(lower.rgb, upper.rgb, scatter_fraction), lower.a * partial_t);
#ifdef FOG_SURFACE_REFINEMENT
    // Reuse the integrated prefix and resolve the final partial interval.
    // Match the cached upper boundary continuously before switching cells.
    refinement_weight *= 1.0 - smoothstep(0.75, 1.0, fraction);
    if(refinement_weight > 0.0 && distance > a) {
        vec3 ray = (world_position - ubo.camera_position.xyz) / max(distance, 1e-5);
        float footprint = 2.0 * depth_b * length(1.0 / (ubo.froxel_grid.xy
            * vec2(ubo.proj[0][0], ubo.proj[1][1])));
        if(fog_surface_cell_has_local(ray, a, b, footprint)) {
            vec4 tail = fog_integrate_interval(world_position, a, 2);
            vec4 refined = vec4(lower.rgb + lower.a * tail.rgb, lower.a * tail.a);
            result = mix(result, refined, refinement_weight);
        }
    }
#endif
    return result;
}

vec4 froxel_sky_sample(vec3 direction) {
    if(ubo.local_fog_info.z > 0.5) return vec4(0.0, 0.0, 0.0, 1.0);
    vec4 clip = ubo.proj * ubo.view * vec4(direction, 0.0);
    vec2 position = (clip.xy / max(clip.w, 1e-5) * 0.5 + 0.5) * ubo.froxel_grid.xy - 0.5;
    ivec2 base = ivec2(floor(position));
    vec2 f = fract(position);
    ivec2 hi = ivec2(ubo.froxel_grid.xy) - 1;
    uint stride = uint(ubo.froxel_grid.x * ubo.froxel_grid.y);
    uint layer = uint(ubo.froxel_grid.z + 1.0) * stride;
    ivec2 a = clamp(base, ivec2(0), hi);
    ivec2 b = clamp(base + ivec2(1, 0), ivec2(0), hi);
    ivec2 c = clamp(base + ivec2(0, 1), ivec2(0), hi);
    ivec2 d = clamp(base + ivec2(1, 1), ivec2(0), hi);
    uint width = uint(ubo.froxel_grid.x);
    return mix(mix(volume.cells[layer + uint(a.y) * width + uint(a.x)],
                   volume.cells[layer + uint(b.y) * width + uint(b.x)], f.x),
               mix(volume.cells[layer + uint(c.y) * width + uint(c.x)],
                   volume.cells[layer + uint(d.y) * width + uint(d.x)], f.x), f.y);
}

vec3 fog_composite_sample(vec3 scene, vec4 fog, float sun_visibility) {
    if(ubo.temporal.w > 2.5) return vec3(sun_visibility);
    if(ubo.temporal.w > 1.5) return fog.rgb;
    if(ubo.temporal.w > 0.5) return vec3(fog.a);
    return scene * fog.a + fog.rgb;
}

vec3 fog_composite(vec3 scene, vec3 world_position, float sun_visibility) {
    return fog_composite_sample(scene, froxel_sample(world_position), sun_visibility);
}
#endif
