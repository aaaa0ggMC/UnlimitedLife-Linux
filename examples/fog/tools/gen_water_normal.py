#!/usr/bin/env python3
"""Generate a seamless tangent-space water normal map.

Runtime shader samples this twice with different scales/scroll speeds and adds
low-frequency sine swell, so the texture only needs broad, tileable detail.
"""
from pathlib import Path

import numpy as np
from PIL import Image


def periodic_value_noise(size: int, frequency: int, rng: np.random.Generator) -> np.ndarray:
    grid = rng.random((frequency, frequency), dtype=np.float32)
    coords = np.arange(size, dtype=np.float32) * frequency / size
    x0 = np.floor(coords).astype(np.int32)
    y0 = np.floor(coords).astype(np.int32)
    fx = coords - x0
    fy = coords - y0
    # Smoothstep keeps the lattice cells from showing as diamond creases.
    fx = fx * fx * (3.0 - 2.0 * fx)
    fy = fy * fy * (3.0 - 2.0 * fy)
    x0 %= frequency
    y0 %= frequency
    x1 = (x0 + 1) % frequency
    y1 = (y0 + 1) % frequency
    a = grid[np.ix_(y0, x0)]
    b = grid[np.ix_(y0, x1)]
    c = grid[np.ix_(y1, x0)]
    d = grid[np.ix_(y1, x1)]
    top = a + (b - a) * fx[None, :]
    bottom = c + (d - c) * fx[None, :]
    return top + (bottom - top) * fy[:, None]


def main() -> None:
    size = 256
    rng = np.random.default_rng(20261027)
    height = np.zeros((size, size), dtype=np.float32)
    amplitude = 1.0
    for frequency in (4, 8, 16, 32, 64, 128):
        height += periodic_value_noise(size, frequency, rng) * amplitude
        amplitude *= 0.52
    height -= height.mean()

    # Central differences on the wrapped height field.
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5
    strength = 18.0
    nx = -dx * strength
    ny = -dy * strength
    nz = np.ones_like(height)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    normal = np.stack((nx / length, ny / length, nz / length), axis=-1)
    encoded = np.clip(normal * 0.5 + 0.5, 0.0, 1.0)
    image = Image.fromarray((encoded * 255.0 + 0.5).astype(np.uint8), mode="RGB")
    output = Path(__file__).resolve().parents[1] / "textures" / "water_normal.png"
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output)
    print(f"wrote {output} ({size}x{size}); normal xy std="
          f"{normal[..., 0].std():.3f}/{normal[..., 1].std():.3f}")


if __name__ == "__main__":
    main()
