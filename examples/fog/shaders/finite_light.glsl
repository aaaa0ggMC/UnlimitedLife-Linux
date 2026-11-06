#ifndef FOG_FINITE_LIGHT_GLSL
#define FOG_FINITE_LIGHT_GLSL
const float FOG_PI = 3.141592653589793;

float phase_hg(float mu) {
    float g = clamp(ubo.volume_params.x, -0.85, 0.85);
    return (1.0 - g * g) / (4.0 * FOG_PI
        * pow(max(1.0 + g * g - 2.0 * g * clamp(mu, -1.0, 1.0), 0.001), 1.5));
}

float hg_cdf(float mu, float g) {
    if(abs(g) < 0.0001) return (mu + 1.0) * 0.5;
    float value = (1.0 - g * g) / (2.0 * g)
        * (inversesqrt(max(1.0 + g * g - 2.0 * g * mu, 1e-6)) - 1.0 / (1.0 + g));
    return clamp(value, 0.0, 1.0);
}

float hg_inverse_cdf(float u, float g) {
    if(abs(g) < 0.0001) return 2.0 * u - 1.0;
    float q = (1.0 - g * g) / (1.0 - g + 2.0 * g * u);
    return clamp((1.0 + g * g - q * q) / (2.0 * g), -1.0, 1.0);
}

// Integral of the unit-normalized HG over the visible spherical emitter's cap.
// Integrate in HG-CDF coordinates: positive quadrature weights ensure a probability
// in [0,1], unlike multiplying one near-source delta-direction peak by a hemisphere.
float emitter_phase_probability(float mu, float radius_ratio_squared) {
    float g = clamp(ubo.volume_params.x, -0.85, 0.85);
    float x = clamp(radius_ratio_squared, 0.0, 1.0);
    float cos_theta = sqrt(1.0 - x);
    float one_minus_cos = x / (1.0 + cos_theta);
    if(abs(g) < 0.0001) return one_minus_cos * 0.5;
    if(x < 0.0004) return clamp(phase_hg(mu) * (2.0 * FOG_PI * one_minus_cos), 0.0, 1.0);
    float sin_alpha = sqrt(max(1.0 - mu * mu, 0.0));
    if(sin_alpha < 0.0001) return mu >= 0.0 ? 1.0 - hg_cdf(cos_theta, g) : hg_cdf(-cos_theta, g);
    float alpha = acos(clamp(mu, -1.0, 1.0));
    float theta = asin(sqrt(x));
    float lo = hg_cdf(cos(min(FOG_PI, alpha + theta)), g);
    float hi = hg_cdf(cos(max(0.0, alpha - theta)), g);
    const float nodes[8] = float[8](0.019855072, 0.101666761, 0.237233795, 0.408282679,
        0.591717321, 0.762766205, 0.898333239, 0.980144928);
    const float weights[8] = float[8](0.050614268, 0.111190517, 0.156853323, 0.181341892,
        0.181341892, 0.156853323, 0.111190517, 0.050614268);
    float probability = 0.0;
    for(int i = 0; i < 8; ++i) {
        float v = hg_inverse_cdf(mix(lo, hi, nodes[i]), g);
        float denominator = sin_alpha * sqrt(max(1.0 - v * v, 0.0));
        float fraction = denominator > 1e-8
            ? acos(clamp((cos_theta - mu * v) / denominator, -1.0, 1.0)) / FOG_PI
            : (mu * v >= cos_theta ? 1.0 : 0.0);
        probability += weights[i] * fraction;
    }
    return clamp((hi - lo) * probability, 0.0, 1.0);
}

float finite_light_scattering(float intensity, float distance, float radius, float footprint, float mu) {
    // Footprint filtering spreads the apparent source at constant I, not constant radiance.
    float r2 = max(radius * radius + footprint * footprint, 0.0004);
    float radiance = max(intensity, 0.0) / (FOG_PI * r2);
    return radiance * emitter_phase_probability(clamp(mu, -1.0, 1.0), r2 / max(distance * distance, r2));
}
#endif
