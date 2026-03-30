#!/usr/bin/env python3

import argparse
import json
from pathlib import Path


def collect_files(root: Path, patterns):
    matches = []
    if not root.exists():
        return matches
    for pattern in patterns:
        matches.extend(sorted(root.rglob(pattern)))
    return [str(path.relative_to(root)) for path in matches]


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Prepare manifests and output directories for open-source DAH benchmarks."
    )
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--output-root", required=True, type=Path)
    args = parser.parse_args()

    args.output_root.mkdir(parents=True, exist_ok=True)

    manifests = {
        "OpenABC": {
            "repo": "https://github.com/NYU-MLDA/OpenABC",
            "candidate_inputs": collect_files(args.source_root / "OpenABC", ["*.aig", "*.v", "*.blif"]),
        },
        "DEHNN": {
            "repo": "https://github.com/TILOS-AI-Institute/DEHNN",
            "candidate_inputs": collect_files(args.source_root / "DEHNN", ["*.csv", "*.json", "*.pkl", "*.pt"]),
        },
        "VTR": {
            "repo": "https://github.com/verilog-to-routing/vtr-verilog-to-routing",
            "candidate_inputs": collect_files(args.source_root / "VTR", ["*.blif", "*.eblif", "*.net", "*.xml"]),
        },
    }

    for name in manifests:
        (args.output_root / name.lower()).mkdir(exist_ok=True)

    manifest_path = args.output_root / "manifest.json"
    manifest_path.write_text(json.dumps(manifests, indent=2, sort_keys=True) + "\n")

    print(f"Wrote benchmark manifest to {manifest_path}")
    print("Use the listed candidate inputs as sources for repository-specific .dah conversion.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
