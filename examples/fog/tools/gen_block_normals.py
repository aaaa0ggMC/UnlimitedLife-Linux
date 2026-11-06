#!/usr/bin/env python3
"""Generate seamless tangent-space normal maps for the voxel materials.

The maps are sampled in world space (with per-material tiling), so they only
need to be seamless and relatively high-frequency; exact block alignment is
not required.
"""
from pathlib import Path

import numpy as np
from PIL import Image


SIZE = 256


def periodic_value_noise(frequency: int, rng: np.random.Generator) -> np.ndarray:
    grid = rng.random((frequency, frequency), dtype=np.float32)
    coords = np.arange(SIZE, dtype=np.float32) * frequency / SIZE
    x0 = np.floor(coords).astype(np.int32)
    y0 = np.floor(coords).astype(np.int32)
    fx = coords - x0
    fy = coords - y0
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


def fbm(frequencies, amplitudes, rng) -> np.ndarray:
    total = np.zeros((SIZE, SIZE), dtype=np.float32)
    for frequency, amplitude in zip(frequencies, amplitudes):
        total += periodic_value_noise(frequency, rng) * amplitude
    total -= total.mean()
    peak = np.max(np.abs(total))
    return total / peak if peak > 0 else total


def smooth(height: np.ndarray, radius: int) -> np.ndarray:
    """Tileable box blur: removes pixel-level noise that reads as visual grit."""
    for axis in (0, 1):
        total = np.zeros_like(height)
        for shift in range(-radius, radius + 1):
            total += np.roll(height, shift, axis=axis)
        height = total / (2 * radius + 1)
    return height


def normal_map(height: np.ndarray, strength: float) -> np.ndarray:
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5
    nx = -dx * strength
    ny = -dy * strength
    nz = np.ones_like(height)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    return np.stack((nx / length, ny / length, nz / length), axis=-1)


def save_normal(name: str, height: np.ndarray, strength: float) -> None:
    normal = normal_map(height, strength)
    encoded = np.clip(normal * 0.5 + 0.5, 0.0, 1.0)
    image = Image.fromarray((encoded * 255.0 + 0.5).astype(np.uint8), mode="RGB")
    path = Path(__file__).resolve().parents[1] / "textures" / f"{name}_normal.png"
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)
    print(f"{path.name:24s} xy_std={normal[..., 0].std():.3f}/{normal[..., 1].std():.3f}")


def main() -> None:
    rng = np.random.default_rng(20261027)
    x = np.linspace(0.0, 1.0, SIZE, endpoint=False, dtype=np.float32)
    xx, yy = np.meshgrid(x, x)

    # 低频、低对比：目标是在正常视距下“有材质”，而不是近距离一片砂纸。
    grass_top = smooth(fbm((3, 6, 12, 24), (1.0, 0.45, 0.18, 0.08), rng), 2)
    save_normal("grass_top", grass_top, 16.0)

    dirt = smooth(fbm((3, 6, 12, 24), (1.0, 0.55, 0.22, 0.10), rng), 2)
    roots = np.sin(2 * np.pi * (2 * xx + 5 * yy)) * smooth(fbm((4, 8), (0.25, 0.12), rng), 2)
    save_normal("grass_side", dirt + roots * 0.35, 16.0)

    stone = smooth(fbm((2, 4, 8, 16, 32), (1.0, 0.55, 0.25, 0.10, 0.04), rng), 2)
    # 少量宽裂缝即可，避免每个像素都像碎石。
    cracks = smooth(fbm((4, 8, 16), (1.0, 0.40, 0.16), rng), 2)
    stone -= np.clip(0.35 - np.abs(cracks) * 3.0, 0.0, None) * 0.20
    save_normal("stone", stone, 16.0)

    grain = np.sin(2 * np.pi * (2.5 * xx + 1.5 * yy) + fbm((4, 8), (3.0, 1.4), rng))
    wood = smooth(grain * 0.65 + fbm((2, 4, 8, 16), (0.45, 0.25, 0.12, 0.05), rng), 2)
    save_normal("wood", wood, 14.0)

    leaves = smooth(fbm((3, 6, 12, 24), (1.0, 0.50, 0.20, 0.08), rng), 1)
    save_normal("leaves", leaves, 14.0)

    sand = smooth(fbm((3, 6, 12, 24), (1.0, 0.40, 0.15, 0.05), rng), 2)
    sand += np.sin(2 * np.pi * (2 * xx + 1 * yy)) * 0.11
    save_normal("sand", sand, 10.0)

    # 交错砖瓦：每块中央平坦、边缘倒角；只会在顶面使用。
    rows = 3.0
    v = yy * rows
    row = np.floor(v)
    u = xx * 3.0 + 0.5 * (row % 2.0)
    fu = u - np.floor(u)
    fv = v - row
    edge = np.minimum(np.minimum(fu, 1.0 - fu), np.minimum(fv, 1.0 - fv))
    bevel = np.clip(edge / 0.12, 0.0, 1.0)
    roof = bevel * bevel * (3.0 - 2.0 * bevel)
    roof += smooth(fbm((4, 8), (0.04, 0.02), rng), 2)
    save_normal("roof", roof, 10.0)

if __name__ == "__main__":
    main()
