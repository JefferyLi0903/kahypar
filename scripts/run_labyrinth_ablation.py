#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import math
import shutil
import subprocess
import time
from collections import deque
from pathlib import Path


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(
        description=(
            "Run serial cut-vs-tob ablation experiments on Labyrinth IBM DAH cases. "
            "Optionally configures/builds KaHyPar before launching experiments."
        )
    )
    parser.add_argument(
        "--cases",
        nargs="+",
        default=[f"ibm{idx:02d}" for idx in range(1, 11)],
        help="Case names without extension, e.g. ibm01 ibm02",
    )
    parser.add_argument(
        "--benchmark-dir",
        type=Path,
        default=root / "benchmarks/labyrinth/dah",
        help="Directory containing ibmXX.dah files",
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=root / "build",
        help="CMake build directory",
    )
    parser.add_argument(
        "--config",
        type=Path,
        default=root / "config/cut_kKaHyPar_sea20.ini",
        help="KaHyPar ini configuration file",
    )
    parser.add_argument("--k", type=int, default=8, help="Number of blocks")
    parser.add_argument("--epsilon", type=float, default=0.03, help="Balance epsilon")
    parser.add_argument("--seed", type=int, default=1, help="Random seed")
    parser.add_argument(
        "--time-limit",
        type=int,
        default=180,
        help="KaHyPar internal time limit in seconds",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=420,
        help="External per-run timeout in seconds",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=root / "benchmarks/labyrinth/results/labyrinth_tob_vs_cut_full.json",
        help="JSON file to write aggregated results to",
    )
    parser.add_argument(
        "--cmake-build-type",
        default="Release",
        help="CMake build type used when --build is enabled",
    )
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="Skip CMake configure/build and use an existing KaHyPar binary",
    )
    return parser.parse_args()


def epsilon_str(epsilon: float) -> str:
    return f"{epsilon:.12f}".rstrip("0").rstrip(".")


def ensure_kahypar_binary(args: argparse.Namespace) -> Path:
    binary = args.build_dir / "kahypar/application/KaHyPar"
    if args.skip_build and binary.exists():
        return binary

    args.build_dir.mkdir(parents=True, exist_ok=True)
    configure_cmd = [
        "cmake",
        "-S",
        str(repo_root()),
        "-B",
        str(args.build_dir),
        f"-DCMAKE_BUILD_TYPE={args.cmake_build_type}",
    ]
    build_cmd = [
        "cmake",
        "--build",
        str(args.build_dir),
        "-j",
    ]
    subprocess.run(configure_cmd, check=True, cwd=str(repo_root()))
    subprocess.run(build_cmd, check=True, cwd=str(repo_root()))
    if not binary.exists():
        raise SystemExit(f"KaHyPar binary not found after build: {binary}")
    return binary


def parse_dah(path: Path) -> tuple[int, list[list[int]], list[int]]:
    with path.open() as f:
        num_hyperedges, num_nodes = map(int, f.readline().split()[:2])
        edges: list[list[int]] = []
        successors = [set() for _ in range(num_nodes)]
        indegree = [0] * num_nodes
        for _ in range(num_hyperedges):
            tokens = f.readline().split()
            if "->" in tokens:
                sep = tokens.index("->")
                sources = [int(x) - 1 for x in tokens[:sep]]
                targets = [int(x) - 1 for x in tokens[sep + 1 :]]
                pins = sources + targets
                for target in targets:
                    for source in sources:
                        if source != target and target not in successors[source]:
                            successors[source].add(target)
                            indegree[target] += 1
            else:
                pins = [int(x) - 1 for x in tokens]
            edges.append(pins)

    levels = [0] * num_nodes
    queue = deque(node for node, deg in enumerate(indegree) if deg == 0)
    while queue:
        node = queue.popleft()
        for succ in successors[node]:
            indegree[succ] -= 1
            levels[succ] = max(levels[succ], levels[node] + 1)
            if indegree[succ] == 0:
                queue.append(succ)
    return num_nodes, edges, levels


def read_partition(path: Path, num_nodes: int) -> list[int]:
    values = [int(x) for x in path.read_text().split()]
    if len(values) != num_nodes:
        raise ValueError(f"Partition size mismatch for {path}: {len(values)} != {num_nodes}")
    return values


def cut_metric(edges: list[list[int]], partition: list[int]) -> int:
    return sum(
        1
        for pins in edges
        if any(partition[pin] != partition[pins[0]] for pin in pins[1:])
    )


def tob_metric(levels: list[int], partition: list[int], k: int) -> int:
    tau_max = max(levels) if levels else 0
    totals = [0] * (tau_max + 1)
    counts = [[0] * (tau_max + 1) for _ in range(k)]
    for hn, tau in enumerate(levels):
        totals[tau] += 1
        counts[partition[hn]][tau] += 1

    metric = 0.0
    for tau, total in enumerate(totals):
        if total <= 0:
            continue
        avg = total / k
        sq_sum = sum((counts[part][tau] - avg) ** 2 for part in range(k))
        metric += sq_sum / (k * total * total)
    return round(metric * 1e9)


def imbalance_metric(partition: list[int], k: int) -> float:
    counts = [0] * k
    for block in partition:
        counts[block] += 1
    perfect = math.ceil(len(partition) / k)
    return max(count / perfect for count in counts) - 1.0


def run_one_objective(
    binary: Path,
    graph: Path,
    objective: str,
    args: argparse.Namespace,
) -> dict:
    out_part = graph.parent / (
        f"{graph.name}.part{args.k}.epsilon{epsilon_str(args.epsilon)}.seed{args.seed}.KaHyPar"
    )
    scoped_part = graph.parent / (
        f"{graph.name}.part{args.k}.epsilon{epsilon_str(args.epsilon)}.seed{args.seed}.KaHyPar.{objective}"
    )

    if out_part.exists():
        out_part.unlink()
    if scoped_part.exists():
        scoped_part.unlink()

    cmd = [
        str(binary),
        "-h",
        str(graph),
        "-k",
        str(args.k),
        "-e",
        str(args.epsilon),
        "-o",
        objective,
        "-m",
        "direct",
        "-p",
        str(args.config),
        "--seed",
        str(args.seed),
        "--time-limit",
        str(args.time_limit),
        "--i-runs",
        "1",
        "--r-runs",
        "0",
        "--i-r-runs",
        "0",
        "--p-use-sparsifier=false",
        "--p-detect-communities=false",
        "--c-rating-use-communities=false",
        "--i-c-rating-use-communities=false",
        "--write-partition=true",
        "-q",
        "true",
    ]

    start = time.time()
    status = "ok"
    error = ""
    try:
        proc = subprocess.run(
            cmd,
            cwd=str(repo_root()),
            capture_output=True,
            text=True,
            timeout=args.timeout,
        )
        if proc.returncode != 0:
            status = f"failed_rc_{proc.returncode}"
            lines = (proc.stderr or proc.stdout or "").strip().splitlines()
            error = lines[-1] if lines else ""
    except subprocess.TimeoutExpired:
        status = "timeout"

    result = {
        "status": status,
        "runtime_sec": round(time.time() - start, 2),
        "error": error,
    }

    if status == "ok" and out_part.exists():
        shutil.copyfile(out_part, scoped_part)
        result["part_file"] = str(scoped_part)

    return result


def enrich_metrics(
    result: dict,
    part_file: Path,
    num_nodes: int,
    edges: list[list[int]],
    levels: list[int],
    k: int,
) -> None:
    partition = read_partition(part_file, num_nodes)
    result["tob_scaled"] = tob_metric(levels, partition, k)
    result["cut"] = cut_metric(edges, partition)
    result["imbalance"] = imbalance_metric(partition, k)


def main() -> int:
    args = parse_args()
    binary = ensure_kahypar_binary(args)
    args.output.parent.mkdir(parents=True, exist_ok=True)

    all_results = []
    for case in args.cases:
        graph = args.benchmark_dir / f"{case}.dah"
        if not graph.exists():
            raise SystemExit(f"Missing graph file: {graph}")

        num_nodes, edges, levels = parse_dah(graph)
        row = {
            "graph": graph.name,
            "k": args.k,
            "epsilon": args.epsilon,
            "seed": args.seed,
            "nodes": num_nodes,
            "edges": len(edges),
        }

        for objective in ("cut", "tob"):
            print(f"RUN {case} {objective}", flush=True)
            result = run_one_objective(binary, graph, objective, args)
            if result["status"] == "ok" and "part_file" in result:
                enrich_metrics(
                    result,
                    Path(result["part_file"]),
                    num_nodes,
                    edges,
                    levels,
                    args.k,
                )
            row[objective] = result
            print(
                f"DONE {case} {objective} status={result['status']} runtime={result['runtime_sec']}s",
                flush=True,
            )

        if "tob_scaled" in row["cut"] and "tob_scaled" in row["tob"]:
            row["delta_tob_cut_minus_tob"] = row["cut"]["tob_scaled"] - row["tob"]["tob_scaled"]
        if "cut" in row["cut"] and "cut" in row["tob"]:
            row["delta_cut_tob_minus_cut"] = row["tob"]["cut"] - row["cut"]["cut"]

        all_results.append(row)
        args.output.write_text(json.dumps(all_results, indent=2) + "\n")

    print(f"WROTE {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
