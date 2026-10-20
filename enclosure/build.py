#!/usr/bin/env python3
"""Export every enclosure part to stl/ and check it:
  - manifold: every edge shared by exactly two faces, consistently oriented, positive volume
  - fits the print bed (BED mm square) in its print orientation
  - assembled parts do not overlap (only the diffuser crush ribs may, by a sliver)
Also renders front.png and exploded.png.  Usage:  python build.py   (set OPENSCAD=... if it isn't on PATH)"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
SCAD = HERE / "sunrise_clock.scad"
OUT = HERE / "stl"
BED = 220.0
PARTS = ["shell", "frame", "diffuser", "light_box", "back_cover", "speaker_lid", "knob"]
PAIRS = [("frame", "shell"), ("frame", "diffuser"), ("shell", "diffuser"), ("light_box", "frame"),
         ("light_box", "diffuser"), ("light_box", "shell"), ("back_cover", "shell"), ("back_cover", "light_box"),
         ("back_cover", "frame"), ("speaker_lid", "back_cover"), ("speaker_lid", "light_box"), ("knob", "shell")]
PRESS_FIT = {("frame", "diffuser"): 3.0}  # mm^3 allowed: crush ribs, 0.1 mm interference by design


def openscad():
    exe = os.environ.get("OPENSCAD") or shutil.which("openscad")
    for guess in (r"C:\Program Files\OpenSCAD\openscad.exe", "/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD"):
        if not exe and Path(guess).exists():
            exe = guess
    if not exe:
        sys.exit("OpenSCAD not found: set OPENSCAD=/path/to/openscad")
    return exe


def run(out, *defs, extra=(), may_be_empty=False):
    args = [openscad(), "-o", str(out), *extra]
    for d in defs:
        args += ["-D", d]
    r = subprocess.run(args + [str(SCAD)], capture_output=True, text=True)
    log = r.stdout + r.stderr
    if may_be_empty and "empty" in log.lower():
        return log
    if r.returncode or "ERROR" in log or "Assertion" in log:
        sys.exit(f"OpenSCAD failed for {out.name}:\n{log}")
    return log


def load_stl(path):
    data = path.read_bytes()
    if data[:5] == b"solid" and b"facet" in data[:400]:
        nums = [line.split()[1:] for line in data.decode().splitlines() if line.strip().startswith("vertex")]
        return np.array(nums, dtype=float).reshape(-1, 3, 3)
    n = int.from_bytes(data[80:84], "little")
    rec = np.frombuffer(data[84:84 + 50 * n], dtype=np.dtype([("n", "<f4", 3), ("v", "<f4", (3, 3)), ("a", "<u2")]))
    return rec["v"].astype(float)


def volume(tris):
    return float(np.einsum("ij,ij->i", tris[:, 0], np.cross(tris[:, 1], tris[:, 2])).sum() / 6)


def manifold_errors(tris):
    verts, idx = np.unique(tris.reshape(-1, 3), axis=0, return_inverse=True)
    f = idx.reshape(-1, 3).astype(np.int64)
    if np.any((f[:, 0] == f[:, 1]) | (f[:, 1] == f[:, 2]) | (f[:, 0] == f[:, 2])):
        return "degenerate triangles"
    n = len(verts)
    fwd = np.concatenate([f[:, 0] * n + f[:, 1], f[:, 1] * n + f[:, 2], f[:, 2] * n + f[:, 0]])
    rev = np.concatenate([f[:, 1] * n + f[:, 0], f[:, 2] * n + f[:, 1], f[:, 0] * n + f[:, 2]])
    if len(np.unique(fwd)) != len(fwd):
        return "an edge is used twice in the same direction (non-manifold or flipped face)"
    if not np.array_equal(np.sort(fwd), np.sort(rev)):
        return "open edges (holes in the mesh)"
    return None


def main():
    sys.stdout.reconfigure(line_buffering=True)
    OUT.mkdir(exist_ok=True)
    ok = True
    for name in PARTS:
        path = OUT / f"{name}.stl"
        run(path, f'part="{name}"')
        tris = load_stl(path)
        err = manifold_errors(tris)
        size = tris.reshape(-1, 3).max(0) - tris.reshape(-1, 3).min(0)
        fits = size[0] <= BED and size[1] <= BED
        vol = volume(tris)
        ok &= err is None and fits and vol > 0
        print(f"{name:12} {len(tris):6} tris  {size[0]:6.1f} x {size[1]:6.1f} x {size[2]:6.1f} mm  "
              f"{vol / 1000:7.1f} cm3  manifold: {err or 'yes'}  bed: {'fits' if fits else 'TOO BIG'}")

    for a, b in PAIRS:
        path = OUT / f"_x_{a}_{b}.stl"
        path.unlink(missing_ok=True)
        log = run(path, 'part="intersect"', f'check=["{a}","{b}"]', may_be_empty=True)
        v = 0.0 if "empty" in log.lower() or not path.exists() else abs(volume(load_stl(path)))
        path.unlink(missing_ok=True)
        allowed = PRESS_FIT.get((a, b), 0.01)
        ok &= v <= allowed
        print(f"overlap {a} / {b}: {v:.3f} mm3 (allowed {allowed})")

    for name, cam in (("front", "0,0,0,90,0,0,0"), ("exploded", "0,0,0,70,0,140,0")):
        run(HERE / f"{name}.png", f'part="{name}"', extra=["--imgsize=1400,1100", "--viewall", "--autocenter",
                                                          f"--camera={cam}", "--colorscheme=Tomorrow"])
    print("rendered front.png, exploded.png")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
