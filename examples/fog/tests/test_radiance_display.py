"""CPU formula mirrors for finite emitters/SDR mapping, not shader execution."""

import math
import unittest


def clamp(x, lo=0.0, hi=1.0):
    return min(max(x, lo), hi)


def hg(mu, g):
    return (1.0 - g * g) / (4.0 * math.pi * (1.0 + g * g - 2.0 * g * mu) ** 1.5)


def cdf(mu, g):
    if abs(g) < 0.0001:
        return (mu + 1.0) * 0.5
    return clamp(
        (1.0 - g * g)
        / (2.0 * g)
        * (1.0 / math.sqrt(1.0 + g * g - 2.0 * g * mu) - 1.0 / (1.0 + g))
    )


def inverse_cdf(u, g):
    if abs(g) < 0.0001:
        return 2.0 * u - 1.0
    q = (1.0 - g * g) / (1.0 - g + 2.0 * g * u)
    return clamp((1.0 + g * g - q * q) / (2.0 * g), -1.0, 1.0)


def cap_probability(mu, x, g):
    mu, x = clamp(mu, -1.0, 1.0), clamp(x)
    cosine = math.sqrt(1.0 - x)
    one_minus = x / (1.0 + cosine)
    if abs(g) < 0.0001:
        return 0.5 * one_minus
    if x < 0.0004:
        return clamp(hg(mu, g) * 2.0 * math.pi * one_minus)
    sine = math.sqrt(max(1.0 - mu * mu, 0.0))
    if sine < 0.0001:
        return 1.0 - cdf(cosine, g) if mu >= 0.0 else cdf(-cosine, g)
    alpha, theta = math.acos(mu), math.asin(math.sqrt(x))
    lo = cdf(math.cos(min(math.pi, alpha + theta)), g)
    hi = cdf(math.cos(max(0.0, alpha - theta)), g)
    nodes = (
        0.019855072,
        0.101666761,
        0.237233795,
        0.408282679,
        0.591717321,
        0.762766205,
        0.898333239,
        0.980144928,
    )
    weights = (
        0.050614268,
        0.111190517,
        0.156853323,
        0.181341892,
        0.181341892,
        0.156853323,
        0.111190517,
        0.050614268,
    )
    total = 0.0
    for node, weight in zip(nodes, weights):
        v = inverse_cdf(lo + (hi - lo) * node, g)
        denominator = sine * math.sqrt(max(1.0 - v * v, 0.0))
        fraction = (
            math.acos(clamp((cosine - mu * v) / denominator, -1.0, 1.0)) / math.pi
            if denominator > 1e-8
            else float(mu * v >= cosine)
        )
        total += weight * fraction
    return clamp((hi - lo) * total)


def scattering(intensity, distance, radius, footprint, mu, g):
    r2 = max(radius * radius + footprint * footprint, 0.0004)
    radiance = intensity / (math.pi * r2)
    return radiance * cap_probability(mu, r2 / max(distance * distance, r2), g)


def tone_map(rgb, ev=0.0):
    exposed = [max(c, 0.0) * 2.0**ev for c in rgb]
    luminance = sum(c * w for c, w in zip(exposed, (0.2126, 0.7152, 0.0722)))
    mapped_y = luminance / (1.0 + luminance)
    mapped = [c / (1.0 + luminance) for c in exposed]
    peak = max(mapped)
    if peak > 1.0:
        scale = (1.0 - mapped_y) / max(peak - mapped_y, 1e-6)
        mapped = [mapped_y + (c - mapped_y) * scale for c in mapped]
    return tuple(clamp(c) for c in mapped)


class RadianceDisplayRegression(unittest.TestCase):
    def test_hg_cdf_and_inverse_are_consistent(self):
        for g in (-0.85, -0.45, 0.0, 0.45, 0.85):
            for u in (0.0, 0.01, 0.2, 0.5, 0.9, 0.99, 1.0):
                self.assertAlmostEqual(cdf(inverse_cdf(u, g), g), u, places=10)

    def test_cap_probability_is_bounded(self):
        for g in (-0.85, 0.0, 0.45, 0.85):
            for mu in (-1.0, -0.9, -0.3, 0.0, 0.4, 0.9, 1.0):
                for x in (1e-6, 0.0004, 0.01, 0.2, 0.9, 1.0):
                    probability = cap_probability(mu, x, g)
                    self.assertGreaterEqual(probability, 0.0)
                    self.assertLessEqual(probability, 1.0)

    def test_isotropic_and_axis_aligned_caps_match_analytic_integrals(self):
        for x in (0.01, 0.2, 0.9, 1.0):
            cosine = math.sqrt(1.0 - x)
            self.assertAlmostEqual(cap_probability(0.3, x, 0.0), (1.0 - cosine) * 0.5)
            for g in (-0.85, 0.45, 0.85):
                self.assertAlmostEqual(cap_probability(1.0, x, g), 1.0 - cdf(cosine, g))
                self.assertAlmostEqual(cap_probability(-1.0, x, g), cdf(-cosine, g))

    def test_far_emitter_approaches_point_source(self):
        for mu in (-0.5, 0.0, 0.9):
            expected = 24.0 / (100.0**2) * hg(mu, 0.45)
            actual = scattering(24.0, 100.0, 0.11, 0.0, mu, 0.45)
            self.assertAlmostEqual(actual / expected, 1.0, delta=1e-5)

    def test_near_field_cannot_exceed_emitter_radiance(self):
        emitted = 24.0 / (math.pi * 0.11**2)
        for distance in (0.11, 0.12, 0.2, 0.5, 1.0):
            for footprint in (0.0, 0.1, 0.5):
                for mu in (-1.0, 0.0, 1.0):
                    incoming = scattering(24.0, distance, 0.11, footprint, mu, 0.85)
                    self.assertLessEqual(incoming, emitted + 1e-8)
                    # Passive single-source medium: emitted*T + source*(1-T) <= emitted.
                    for sigma in (0.0, 0.475, 2.0):
                        t = math.exp(-sigma * 3.0)
                        self.assertLessEqual(
                            emitted * t + incoming * 0.9 * (1.0 - t), emitted + 1e-8
                        )

    def test_tone_mapping_is_finite_bounded_and_black_preserving(self):
        self.assertEqual(tone_map((0.0, 0.0, 0.0)), (0.0, 0.0, 0.0))
        for power in (0.01, 1.0, 10.0, 631.0, 1e6):
            mapped = tone_map(tuple(c * power for c in (0.35, 0.65, 1.0)))
            self.assertTrue(all(math.isfinite(c) and 0.0 <= c <= 1.0 for c in mapped))
            self.assertLessEqual(mapped[0], mapped[1])
            self.assertLessEqual(mapped[1], mapped[2])

    def test_source_is_bright_without_scattering(self):
        emitted = 24.0 / (math.pi * 0.11**2)
        old_preview = tone_map((0.35, 0.65, 1.0))
        source = tone_map(tuple(c * emitted for c in (0.35, 0.65, 1.0)))
        self.assertGreater(min(source), 0.95)
        self.assertGreater(sum(source), sum(old_preview))


if __name__ == "__main__":
    unittest.main()
