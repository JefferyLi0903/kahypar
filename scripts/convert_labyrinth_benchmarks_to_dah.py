#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
from pathlib import Path


NET_HEADER = re.compile(r"^(\S+)\s+(\d+)\s+(\d+)\s*$")


def pin_rank(pin: tuple[int, int]) -> tuple[int, int]:
    x, y = pin
    return (y, x)


def parse_modified_file(path: Path) -> tuple[list[list[tuple[int, int]]], int]:
    nets: list[list[tuple[int, int]]] = []
    all_pins: list[tuple[int, int]] = []

    with path.open() as f:
        # Skip benchmark header lines:
        # grid, vertical capacity, horizontal capacity, num net
        _ = f.readline()
        _ = f.readline()
        _ = f.readline()
        _ = f.readline()

        while True:
            line = f.readline()
            if not line:
                break
            s = line.strip()
            if not s:
                continue
            m = NET_HEADER.match(s)
            if not m:
                continue

            degree = int(m.group(3))
            pins: list[tuple[int, int]] = []
            for _ in range(degree):
                p = f.readline().strip().split()
                if len(p) >= 2:
                    pins.append((int(p[0]), int(p[1])))

            unique_pins: list[tuple[int, int]] = []
            seen: set[tuple[int, int]] = set()
            for pin in pins:
                if pin not in seen:
                    seen.add(pin)
                    unique_pins.append(pin)

            if len(unique_pins) >= 2:
                nets.append(unique_pins)
                all_pins.extend(unique_pins)

    unique_coordinates = sorted(set(all_pins), key=lambda p: (p[0], p[1]))
    return nets, len(unique_coordinates)


def convert_file(path: Path, out_dir: Path) -> tuple[Path, int, int]:
    nets, _ = parse_modified_file(path)

    # Global node IDs are unique coordinates.
    all_coords = sorted({pin for net in nets for pin in net}, key=lambda p: (p[0], p[1]))
    node_id_of = {coord: idx + 1 for idx, coord in enumerate(all_coords)}

    directed_hes: list[tuple[list[int], list[int]]] = []
    for net in nets:
        ordered = sorted(net, key=pin_rank)
        source = ordered[0]
        targets = [pin for pin in ordered[1:] if pin != source]
        if not targets:
            continue
        directed_hes.append(([node_id_of[source]], [node_id_of[t] for t in targets]))

    out_path = out_dir / (path.stem.replace(".modified", "") + ".dah")
    with out_path.open("w") as out:
        out.write(f"{len(directed_hes)} {len(all_coords)}\n")
        for src, dst in directed_hes:
            out.write(" ".join(map(str, src)))
            out.write(" -> ")
            out.write(" ".join(map(str, dst)))
            out.write("\n")

    return out_path, len(all_coords), len(directed_hes)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert Labyrinth IBM modified routing benchmarks to DAH format."
    )
    parser.add_argument("--input-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=True)
    files = sorted(args.input_dir.glob("ibm*.modified.txt"))
    if not files:
        raise SystemExit(f"No ibm*.modified.txt files found in {args.input_dir}")

    for fp in files:
        out_path, n, m = convert_file(fp, args.output_dir)
        print(f"{fp.name} -> {out_path.name}: nodes={n}, hyperedges={m}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
