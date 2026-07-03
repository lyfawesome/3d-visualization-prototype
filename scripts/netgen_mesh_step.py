#!/usr/bin/env python
"""Generate a Netgen .vol mesh from a STEP/STP file.

This bridge is intentionally small: the C++ application owns marker remapping,
VTK export and CalculiX input generation. Netgen only
produces the higher-quality surface/volume mesh in its native .vol format.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from netgen.occ import OCCGeometry


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate Netgen .vol from STEP/STP")
    parser.add_argument("step_file", type=Path)
    parser.add_argument("vol_file", type=Path)
    parser.add_argument("--maxh", type=float, default=None)
    parser.add_argument("--grading", type=float, default=None)
    parser.add_argument("--segmentsperedge", type=float, default=None)
    parser.add_argument("--optsteps2d", type=int, default=None)
    parser.add_argument("--optsteps3d", type=int, default=None)
    parser.add_argument("--quad-dominated", action="store_true")
    parser.add_argument("--no-heal", action="store_true")
    parser.add_argument("--heal-tolerance", type=float, default=0.001)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not args.step_file.exists():
        raise FileNotFoundError(f"STEP file does not exist: {args.step_file}")

    mesh_kwargs = {}
    for key in ("maxh", "grading", "segmentsperedge", "optsteps2d", "optsteps3d"):
        value = getattr(args, key)
        if value is not None:
            mesh_kwargs[key] = value
    if args.quad_dominated:
        mesh_kwargs["quad_dominated"] = True

    args.vol_file.parent.mkdir(parents=True, exist_ok=True)
    geometry = OCCGeometry(str(args.step_file))
    if not args.no_heal:
        geometry.Heal(tolerance=args.heal_tolerance, sewfaces=True, makesolids=True)
    mesh = geometry.GenerateMesh(**mesh_kwargs)
    mesh.Save(str(args.vol_file))

    print(f"Netgen wrote {args.vol_file}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
