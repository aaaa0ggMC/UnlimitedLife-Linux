#ifndef FOG_DISPLAY_GLSL
#define FOG_DISPLAY_GLSL

vec3 display_encode(vec3 linear_color) {
    if(ubo.display_params.z < 0.5) return linear_color; // sRGB attachment performs the encoding.
    return mix(1.055 * pow(max(linear_color, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055,
        linear_color * 12.92, lessThanEqual(linear_color, vec3(0.0031308)));
}

vec3 display_linear_radiance(vec3 radiance) {
    vec3 exposed = max(radiance, vec3(0.0)) * exp2(clamp(ubo.display_params.x, -20.0, 20.0));
    if(ubo.display_params.y < 0.5) return exposed; // Explicit raw/clipping A/B.
    float luminance = dot(exposed, vec3(0.2126, 0.7152, 0.0722));
    float mapped_y = luminance / (1.0 + luminance);
    vec3 mapped = exposed / (1.0 + luminance);
    // Fit out-of-gamut chroma towards the mapped neutral axis, not channel-by-channel clipping.
    float peak = max(max(mapped.r, mapped.g), mapped.b);
    if(peak > 1.0) mapped = vec3(mapped_y)
        + (mapped - vec3(mapped_y)) * ((1.0 - mapped_y) / max(peak - mapped_y, 1e-6));
    return clamp(mapped, vec3(0.0), vec3(1.0));
}

vec3 display_radiance(vec3 radiance) {
    // Transmittance and visibility are diagnostic quantities, not scene radiance.
    if(ubo.temporal.w > 2.5 || (ubo.temporal.w > 0.5 && ubo.temporal.w < 1.5))
        return display_encode(clamp(radiance, vec3(0.0), vec3(1.0)));
    return display_encode(display_linear_radiance(radiance));
}

vec3 display_emissive_surface(vec3 fogged_reflection, vec3 reflection, vec3 emission, float transmittance) {
    if(ubo.temporal.w > 0.5 || !any(greaterThan(emission, vec3(0.0))))
        return display_radiance(fogged_reflection);
    // Display-referred emission: compress its HDR peak BEFORE applying visibility.
    // Otherwise I/(pi R²)*T can remain saturated even when T is nearly zero.
    // This is a deliberate display operator, not a change to radiometric lighting.
    vec3 base = display_linear_radiance(fogged_reflection);
    // Use the mapped increment, retaining exactly the old no-fog appearance of
    // mixed reflective/emissive materials as well as pure lamp cores.
    vec3 emitted = clamp(display_linear_radiance(reflection + emission), vec3(0.0), vec3(1.0))
        - clamp(display_linear_radiance(reflection), vec3(0.0), vec3(1.0));
    return display_encode(clamp(base + clamp(transmittance, 0.0, 1.0) * emitted, vec3(0.0), vec3(1.0)));
}
#endif
