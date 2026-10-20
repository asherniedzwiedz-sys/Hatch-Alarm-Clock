#!/usr/bin/env python3
"""List downward-facing surfaces that need support, per STL as printed (flat side on the bed).
    python overhangs.py [part ...]      Faces steeper than 45 deg from vertical, above the first layer."""
import sys

import numpy as np

from build import OUT, PARTS, load_stl


def overhangs(name, max_bridge=8.0):
    t = load_stl(OUT / f"{name}.stl")
    n = np.cross(t[:, 1] - t[:, 0], t[:, 2] - t[:, 0])
    area = np.linalg.norm(n, axis=1) / 2
    nz = n[:, 2] / np.maximum(2 * area, 1e-12)
    zmin = t[:, :, 2].min(1)
    bad = (nz < -0.71) & (zmin > 0.3) & (area > 1e-6)  # facing down more than 45 deg, not on the bed
    groups = {}
    for z, a, tri in zip(np.round(zmin[bad], 1), area[bad], t[bad]):
        g = groups.setdefault(z, [0.0, tri.reshape(-1, 3)[:, :2].min(0), tri.reshape(-1, 3)[:, :2].max(0)])
        g[0] += a
        g[1] = np.minimum(g[1], tri.reshape(-1, 3)[:, :2].min(0))
        g[2] = np.maximum(g[2], tri.reshape(-1, 3)[:, :2].max(0))
    return groups


if __name__ == "__main__":
    for name in sys.argv[1:] or PARTS:
        g = overhangs(name)
        print(f"{name}: {sum(v[0] for v in g.values()):.0f} mm2 facing down")
        for z, (a, lo, hi) in sorted(g.items(), key=lambda kv: -kv[1][0])[:8]:
            print(f"   z={z:6.1f}  {a:7.1f} mm2  x {lo[0]:6.1f}..{hi[0]:6.1f}  y {lo[1]:6.1f}..{hi[1]:6.1f}")
