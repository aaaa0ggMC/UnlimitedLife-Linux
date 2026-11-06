#ifndef FOG_CONTROL_DENSITY_GLSL
#define FOG_CONTROL_DENSITY_GLSL
// Keep in sync with fog_control_grid_size and max_local_fog_volumes in app.h.
const uint CONTROL_GRID_SIZE = 64u;
const uint CONTROL_GRID_CELLS = 64u * 64u * 64u;
#ifdef FOG_CONTROL_DENSITY_WRITE
layout(std430, set = 0, binding = 11) writeonly buffer ControlDensity { float values[]; } control_density;
#else
layout(std430, set = 0, binding = 11) readonly buffer ControlDensity { float values[]; } control_density;

float control_density_sample(uint slot, vec3 uv) {
    vec3 p = clamp(uv, vec3(0.0), vec3(1.0)) * float(CONTROL_GRID_SIZE - 1u);
    uvec3 base = uvec3(floor(p));
    vec3 f = fract(p);
    float result = 0.0;
    for(uint z = 0u; z < 2u; ++z) for(uint y = 0u; y < 2u; ++y) for(uint x = 0u; x < 2u; ++x) {
        uvec3 q = min(base + uvec3(x, y, z), uvec3(CONTROL_GRID_SIZE - 1u));
        vec3 weight = mix(1.0 - f, f, vec3(x, y, z));
        result += weight.x * weight.y * weight.z * control_density.values[
            slot * CONTROL_GRID_CELLS + (q.z * CONTROL_GRID_SIZE + q.y) * CONTROL_GRID_SIZE + q.x];
    }
    return result;
}
#endif
#endif
