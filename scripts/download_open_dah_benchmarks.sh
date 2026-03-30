#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${1:-${ROOT_DIR}/external_dah_benchmarks}"

mkdir -p "${OUT_DIR}/sources" "${OUT_DIR}/generated"

clone_or_update() {
  local repo_url="$1"
  local name="$2"
  if [[ -d "${OUT_DIR}/sources/${name}/.git" ]]; then
    git -C "${OUT_DIR}/sources/${name}" fetch --depth 1 origin
    git -C "${OUT_DIR}/sources/${name}" reset --hard FETCH_HEAD
  else
    git clone --depth 1 "${repo_url}" "${OUT_DIR}/sources/${name}"
  fi
}

echo "Staging public benchmark sources into ${OUT_DIR}"
clone_or_update "https://github.com/NYU-MLDA/OpenABC.git" "OpenABC"
clone_or_update "https://github.com/TILOS-AI-Institute/DEHNN.git" "DEHNN"
clone_or_update "https://github.com/verilog-to-routing/vtr-verilog-to-routing.git" "VTR"

cat <<EOF

Sources downloaded.

Next step:
  python3 scripts/prepare_dah_benchmarks.py --source-root "${OUT_DIR}/sources" --output-root "${OUT_DIR}/generated"

The preparation script scans the staged repositories, emits manifest files, and
creates target directories for generated .dah and optional .hgr/.lvl outputs.
Repository-specific netlist extraction still depends on the source format you pick.
EOF
