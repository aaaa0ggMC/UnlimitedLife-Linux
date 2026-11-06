#ifndef FOG_SURFACE_GLSL
#define FOG_SURFACE_GLSL

// A coarse integrated cell cannot locate its source inside the cell: using its
// mean coefficients on a foreground surface can allocate fog behind that surface
// to the visible prefix. Near local lamps, refine only the final partial cell.
float fog_surface_refinement_weight(vec3 p) {
    if(ubo.local_fog_info.x < 0.5 || ubo.local_fog_info.y < 0.5) return 0.0;
    float weight = 0.0;
    if(ubo.local_light_info.z < 0.5) return weight;
    uint count = min(uint(ubo.local_light_info.x), 8u);
    for(uint i = 0; i < count; ++i) {
        vec4 light_range = ubo.local_light_position_range[i];
        if(ubo.local_light_params[i].z < 0.5 || ubo.local_light_color_intensity[i].w <= 0.0) continue;
        float radius = max(1.0, ubo.local_light_params[i].x);
        vec3 b = p - light_range.xyz;
        weight = max(weight, 1.0 - smoothstep(1.0, 4.0, dot(b, b) / (radius * radius)));
    }
    return weight;
}

bool fog_surface_cell_has_local(vec3 ray, float begin, float end, float footprint) {
    // Include the entire cell, even behind the surface: that is precisely where
    // the coarse source can leak forward. Expand bounds for the bilinear columns.
    for(uint i = 0; i < min(local_fog.counts.x, 16u); ++i) {
        vec4 bounds = local_fog.entries[i].bounds;
        vec3 delta = bounds.xyz - ubo.camera_position.xyz;
        vec3 separation = delta - ray * clamp(dot(delta, ray), begin, end);
        float radius = bounds.w + footprint;
        if(dot(separation, separation) <= radius * radius) return true;
    }
    return false;
}

// t is world-ray distance: do not normalize the transformed ray under nonuniform scale.
vec2 fog_shape_interval(GpuFogVolume medium, vec3 origin, vec3 ray, float radius) {
    if(radius <= 0.00001) return vec2(-1.0);
    vec3 o = (medium.world_to_local * vec4(origin, 1.0)).xyz;
    vec3 d = (medium.world_to_local * vec4(ray, 0.0)).xyz;
    if(medium.params.z > 0.5) {
        float a = dot(d, d);
        if(a <= 1e-12) return vec2(-1.0);
        float center_t = -dot(o, d) / a;
        vec3 closest = o + d * center_t;
        float height_squared = radius * radius - dot(closest, closest);
        if(height_squared <= 0.0) return vec2(-1.0);
        float half_span = sqrt(height_squared / a);
        return vec2(center_t - half_span, center_t + half_span);
    }
    vec2 interval = vec2(-1e20, 1e20);
    for(int axis = 0; axis < 3; ++axis) {
        if(abs(d[axis]) < 1e-8) {
            if(abs(o[axis]) >= radius) return vec2(-1.0);
        } else {
            vec2 bounds = (vec2(-radius, radius) - o[axis]) / d[axis];
            interval.x = max(interval.x, min(bounds.x, bounds.y));
            interval.y = min(interval.y, max(bounds.x, bounds.y));
        }
    }
    return interval.y > interval.x ? interval : vec2(-1.0);
}

vec4 fog_integrate_interval(vec3 world_position, float begin, int steps) {
    vec3 delta = world_position - ubo.camera_position.xyz;
    float distance = length(delta);
    if(distance < 1e-5) return vec4(0.0, 0.0, 0.0, 1.0);
    vec3 ray = delta / distance;
    float end = distance;
    uint count = min(local_fog.counts.x, 16u);
    vec4 intervals[16];
    for(uint i = 0; i < count; ++i) {
        GpuFogVolume medium = local_fog.entries[i];
        intervals[i] = vec4(fog_shape_interval(medium, ubo.camera_position.xyz, ray, 0.5),
            fog_shape_interval(medium, ubo.camera_position.xyz, ray, 0.5 * (1.0 - medium.params.y)));
    }
    // Also resolve the finite light cores instead of stepping across their steep near fields.
    float t = max(begin, 0.0);
    float transmittance = 1.0;
    vec3 scattering = vec3(0.0);
    float step_length = max((end - t) / float(max(steps, 1)), 0.00001);
    float global_start = fog_global_view_start();
    // <=48 regular steps + <=64 shape events + 9 height events + 1 start + 6 light-core events.
    for(int step = 0; step < 160 && t < end; ++step) {
        float next = min(t + step_length, end);
        if(global_start > t + 1e-5) next = min(next, global_start);
        // Growing sky intervals must not jump over a finite high-altitude layer.
        if(ubo.fog_shape.x > 2.5 && abs(ray.y) > 1e-6) {
            const float heights[9] = float[9](-8.0, -4.0, -2.0, -1.0, 0.0, 1.0, 2.0, 4.0, 8.0);
            for(int j = 0; j < 9; ++j) {
                float height = ubo.fog_shape.y + heights[j] * max(ubo.fog_shape.z, 0.1);
                float boundary = (height - ubo.camera_position.y) / ray.y;
                if(boundary > t + 1e-5) next = min(next, boundary);
            }
        }
        for(uint i = 0; i < count; ++i) for(int j = 0; j < 4; ++j) {
            float boundary = intervals[i][j];
            if(boundary > t + 1e-5) next = min(next, boundary);
        }
        if(ubo.local_light_info.z > 0.5) {
            uint light_count = min(uint(ubo.local_light_info.x), 8u);
            for(uint i = 0; i < light_count; ++i) {
                if(ubo.local_light_color_intensity[i].w <= 0.0) continue;
                float light_t = dot(ubo.local_light_position_range[i].xyz - ubo.camera_position.xyz, ray);
                float radius = max(ubo.local_light_params[i].x, 0.001);
                for(int j = -1; j <= 1; ++j) {
                    float boundary = light_t + float(j) * radius;
                    if(boundary > t + 1e-5) next = min(next, boundary);
                }
            }
        }
        float ds = (next - t) * 0.25;
        for(int sample_index = 0; sample_index < 4; ++sample_index) {
            float s = t + (float(sample_index) + 0.5) * ds;
            vec3 p = ubo.camera_position.xyz + ray * s;
            // Unlike the froxel pass this solver starts at zero even without locals.
            // Apply the optional global clip explicitly; local volumes stay independent.
            vec4 medium = fog_medium(p, s < global_start ? 0.0 : 1.0);
            if(medium.a > 0.0) {
                float tau = min(medium.a * ds, 80.0);
                float segment_t = exp(-tau);
                float integral = tau < 0.001 ? ds * (1.0 - 0.5 * tau)
                    : (1.0 - segment_t) / medium.a;
                if(any(greaterThan(medium.rgb, vec3(0.0))))
                    scattering += transmittance * medium.rgb * volume_illumination(p, ray, 0.0) * integral;
                transmittance *= segment_t;
            }
        }
        t = next;
    }
    return vec4(scattering, transmittance);
}

vec4 fog_integrate_to_surface(vec3 world_position) {
    return fog_integrate_interval(world_position, 0.0, 48);
}

// The sky has no surface distance. Extend by exponentially growing intervals
// until the remaining background is optically negligible, not until fog.end.
// Run per froxel column, never once per sky pixel.
vec4 fog_integrate_sky(vec3 ray, float begin) {
    vec4 result = vec4(0.0, 0.0, 0.0, 1.0);
    if(ubo.fog_extra.w <= 0.0 && ubo.fog_params.z <= 0.0 && local_fog.counts.x == 0u) return result;
    float a = begin;
    float span = max(begin * 0.125, 1.0);
    for(int i = 0; i < 24; ++i) {
        vec4 segment = fog_integrate_interval(ubo.camera_position.xyz + ray * (a + span), a, 2);
        result.rgb += result.a * segment.rgb;
        result.a *= segment.a;
        if(result.a < 1e-6) return result;
        a += span;
        span *= 2.0;
    }
    // Numerical fallback for extremely weak, unbounded media: continue the last
    // far-field coefficients analytically to infinity rather than freeze T.
    vec3 p = ubo.camera_position.xyz + ray * a;
    vec4 medium = fog_medium(p, a < fog_global_view_start() ? 0.0 : 1.0);
    if(medium.a > 0.0) {
        result.rgb += result.a * medium.rgb / medium.a * volume_illumination(p, ray, 0.0);
        result.a = 0.0;
    }
    return result;
}
#endif
