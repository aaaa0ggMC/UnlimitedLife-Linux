"""CPU mirrors of the volume math; running these does not validate GPU shaders."""

import math
import unittest


def hg(mu, g):
    return (1.0 - g * g) / (4.0 * math.pi * (1.0 + g * g - 2.0 * g * mu) ** 1.5)


def integrate(segments):
    scattering, transmittance = 0.0, 1.0
    for sigma, source, length in segments:
        tau = sigma * length
        segment_t = math.exp(-tau)
        integral = length if sigma == 0.0 else -math.expm1(-tau) / sigma
        scattering += transmittance * source * integral
        transmittance *= segment_t
    return scattering, transmittance


def partial_segment(lower, upper, fraction):
    ratio = upper[1] / lower[1]
    tau = -math.log(ratio)
    partial_t = math.exp(-tau * fraction)
    weight = fraction if tau < 0.001 else -math.expm1(-tau * fraction) / (1.0 - ratio)
    return lower[0] + (upper[0] - lower[0]) * weight, lower[1] * partial_t


def attenuation(distance, radius):
    return 1.0 / max(distance * distance, radius * radius)


def temporal_response(neighborhood, old):
    mean = sum(neighborhood) / len(neighborhood)
    variance = max(
        sum(x * x for x in neighborhood) / len(neighborhood) - mean * mean, 0.0
    )
    excess = max(abs(old - mean) - 2.0 * math.sqrt(variance), 0.0)
    relative = excess / max(old, mean, 0.001)
    t = min(max((relative - 0.15) / (0.65 - 0.15), 0.0), 1.0)
    return 1.0 - t * t * (3.0 - 2.0 * t)


class VolumeMathRegression(unittest.TestCase):
    def test_hg_has_unit_solid_angle_integral(self):
        # Integral over azimuth and cos(theta); catches the former missing 1/(4*pi).
        n = 20000
        for g in (-0.85, 0.0, 0.45, 0.85):
            integral = sum(hg(-1.0 + (i + 0.5) * 2.0 / n, g) for i in range(n))
            self.assertAlmostEqual(integral * 4.0 * math.pi / n, 1.0, delta=0.0001)
        self.assertAlmostEqual(hg(0.0, 0.0), 1.0 / (4.0 * math.pi))

    def test_constant_medium_is_independent_of_cell_count(self):
        sigma, illumination, length = 0.05, 0.7, 55.0
        expected_t = math.exp(-sigma * length)
        for cells in (1, 16, 32, 64, 96):
            result = integrate([(sigma, sigma * illumination, length / cells)] * cells)
            self.assertAlmostEqual(result[0], illumination * (1.0 - expected_t))
            self.assertAlmostEqual(result[1], expected_t)

    def test_partial_cell_matches_actual_path_not_log_slice_fraction(self):
        lower = integrate([(0.12, 0.4, 7.0)])
        sigma, source, length = 0.3, 1.2, 4.0
        segment = integrate([(sigma, source, length)])
        upper = lower[0] + lower[1] * segment[0], lower[1] * segment[1]
        for fraction in (0.0, 0.1, 0.5, 0.9, 1.0):
            expected = integrate([(0.12, 0.4, 7.0), (sigma, source, length * fraction)])
            actual = partial_segment(lower, upper, fraction)
            self.assertAlmostEqual(actual[0], expected[0])
            self.assertAlmostEqual(actual[1], expected[1])

    def test_light_in_background_cannot_overwrite_foreground_segment(self):
        foreground = integrate([(0.05, 0.01, 5.0)])
        bright = integrate([(0.05, 20.0, 2.0)])
        full = foreground[0] + foreground[1] * bright[0], foreground[1] * bright[1]
        self.assertEqual(partial_segment(foreground, full, 0.0), foreground)

    def test_finite_surface_irradiance_has_no_zero_distance_singularity(self):
        for radius in (0.02, 0.11, 1.0):
            values = [
                attenuation(distance, radius)
                for distance in (0.0, 0.01, 0.1, 1.0, 10.0)
            ]
            self.assertEqual(values, sorted(values, reverse=True))
            self.assertAlmostEqual(values[0], 1.0 / (radius * radius))
            self.assertAlmostEqual(values[-1], 0.01)

    def test_zero_extinction_does_not_divide_by_zero(self):
        self.assertEqual(integrate([(0.0, 0.0, 20.0)]), (0.0, 1.0))

    def test_jittered_beam_edge_keeps_valid_history(self):
        self.assertEqual(
            temporal_response([0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0], 0.5), 1.0
        )

    def test_uniform_new_shadow_rejects_old_lighting(self):
        self.assertEqual(temporal_response([0.0] * 7, 1.0), 0.0)

    def test_surface_before_local_fog_entry_has_no_local_scattering(self):
        # The coarse cell [5,7] straddles the fog entry at 6.05, but the surface is at 6.
        surface, entry, exit = 6.0, 6.05, 7.0
        visible_span = max(min(surface, exit) - entry, 0.0)
        self.assertEqual(integrate([(0.52, 5.2, visible_span)]), (0.0, 1.0))
        coverage = (exit - entry) / (7.0 - 5.0)
        leaked = integrate([(0.52 * coverage, 5.2 * coverage, surface - 5.0)])
        self.assertGreater(
            leaked[0], 0.0
        )  # A partial constant-cell reconstruction is insufficient.

    def test_near_boundary_scattering_uses_only_actual_visible_span(self):
        surface, entry, exit = 6.0, 5.98, 7.0
        span = max(min(surface, exit) - entry, 0.0)
        actual = integrate([(0.52, 5.2, span)])
        self.assertAlmostEqual(actual[0], 10.0 * (1.0 - math.exp(-0.52 * 0.02)))
        self.assertAlmostEqual(actual[1], math.exp(-0.52 * 0.02))

    def test_default_global_medium_has_no_eight_unit_camera_void(self):
        for distance in (0.5, 2.0, 7.9, 8.1):
            physical = integrate([(0.05, 0.035, distance)])
            self.assertLess(physical[1], 1.0)
            self.assertGreater(physical[0], 0.0)
            legacy = integrate([(0.05, 0.035, max(distance - 8.0, 0.0))])
            if distance <= 8.0:
                self.assertEqual(legacy, (0.0, 1.0))
            else:
                self.assertGreater(legacy[0], 0.0)
                self.assertLess(legacy[1], 1.0)

    def test_local_medium_keeps_near_path_when_global_cutoff_is_enabled(self):
        # A local slab occupies [2,4], the surface is at 3, global start is 8.
        local_span = max(min(3.0, 4.0) - 2.0, 0.0)
        local = integrate([(0.5, 0.25, local_span)])
        global_medium = integrate([(0.05, 0.035, max(3.0 - 8.0, 0.0))])
        self.assertEqual(global_medium, (0.0, 1.0))
        self.assertAlmostEqual(local[1], math.exp(-0.5))
        self.assertGreater(local[0], 0.0)


if __name__ == "__main__":
    unittest.main()
