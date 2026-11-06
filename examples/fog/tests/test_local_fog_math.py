"""CPU mirrors for local-medium masks/packing; not a GPU or ECS integration test."""

import math
import unittest


def shape_mask(local, shape, fade):
    q = [abs(v) * 2.0 for v in local]
    radius = max(q) if shape == 0 else math.sqrt(sum(v * v for v in q))
    if radius >= 1.0:
        return 0.0
    if fade <= 0.0001:
        return 1.0
    t = min(max((radius - (1.0 - fade)) / fade, 0.0), 1.0)
    return 1.0 - t * t * (3.0 - 2.0 * t)


class LocalFogMath(unittest.TestCase):
    def test_shapes_are_bounded_and_have_full_center_density(self):
        for shape in (0, 1):
            self.assertEqual(shape_mask((0, 0, 0), shape, 0.2), 1.0)
            self.assertEqual(shape_mask((0.5, 0, 0), shape, 0.2), 0.0)
            self.assertEqual(shape_mask((4, 0, 0), shape, 0.2), 0.0)

    def test_box_corner_is_outside_ellipsoid(self):
        self.assertEqual(shape_mask((0.4, 0.4, 0.4), 0, 0.0), 1.0)
        self.assertEqual(shape_mask((0.4, 0.4, 0.4), 1, 0.0), 0.0)

    def test_edge_density_decreases_smoothly(self):
        values = [
            shape_mask((x, 0, 0), 0, 0.3) for x in (0.0, 0.35, 0.4, 0.45, 0.49, 0.5)
        ]
        self.assertEqual(values, sorted(values, reverse=True))

    def test_overlaps_add_extinction_and_scattering_not_average_colors(self):
        # Blue absorbing volume and red scattering volume overlap.
        media = [(0.3, (0, 0, 1), 0.0), (0.2, (1, 0, 0), 0.5)]
        extinction = sum(sigma for sigma, _, _ in media)
        scattering = tuple(
            sum(sigma * color[c] * albedo for sigma, color, albedo in media)
            for c in range(3)
        )
        self.assertAlmostEqual(extinction, 0.5)
        self.assertEqual(scattering, (0.1, 0.0, 0.0))

    def test_atlas_uvs_stay_on_their_own_interior_texel_centers(self):
        tile, size, slots = 34, 32, 4
        for slot in range(slots):
            for uv in (0.0, 0.1, 0.5, 0.9, 1.0):
                coordinate = (slot * tile + 1.5 + uv * (size - 1)) / (tile * slots)
                texel_coordinate = coordinate * (tile * slots) - 0.5
                self.assertGreaterEqual(texel_coordinate, slot * tile + 1.0 - 1e-10)
                self.assertLessEqual(texel_coordinate, slot * tile + 32.0 + 1e-10)


if __name__ == "__main__":
    unittest.main()
