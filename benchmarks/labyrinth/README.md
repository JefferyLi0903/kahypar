# Labyrinth IBM Benchmarks (DAH Conversion)

Raw source page:
- https://cseweb.ucsd.edu/~kastner/labyrinth_vault/benchmarks/

## Conversion rule used in this repo

The original `ibmXX.modified.txt` files are undirected routing-net descriptions over grid pins.
To derive a DAG-style `.dah` input for TOB experiments, we apply:

1. Deduplicate repeated pins inside each net.
2. Drop nets with fewer than 2 unique pins.
3. Assign one global node ID per unique `(x, y)` coordinate.
4. Sort each net's pins by `(y, x)` and orient as:
   - source: first pin in sorted order
   - targets: all remaining pins

This gives acyclic orientation under the induced coordinate order, suitable for KaHyPar TOB experiments.

## Re-run conversion

```bash
python3 scripts/convert_labyrinth_benchmarks_to_dah.py \
  --input-dir benchmarks/labyrinth/raw \
  --output-dir benchmarks/labyrinth/dah
```

## Result file

Benchmark run summaries are written to:
- `benchmarks/labyrinth/results/labyrinth_tob_results.json`

## Full ablation script

To run `cut` vs `tob` ablation over `ibm01-10` on your own host, use:

```bash
python3 scripts/run_labyrinth_ablation.py
```

What this script does:
- optionally configures and builds KaHyPar with CMake
- runs the Labyrinth `ibm01-10` `.dah` cases serially
- for each case, runs both `-o cut` and `-o tob`
- copies each successful partition into objective-specific files
- recomputes `TOB / cut / imbalance` with a unified offline evaluator
- writes aggregated JSON results

Default output:
- `benchmarks/labyrinth/results/labyrinth_tob_vs_cut_full.json`

### Recommended usage on a fresh host

If the raw benchmarks have already been downloaded and converted:

```bash
python3 scripts/run_labyrinth_ablation.py
```

If you want to skip the CMake build and use an existing binary:

```bash
python3 scripts/run_labyrinth_ablation.py --skip-build
```

If you only want a subset of cases:

```bash
python3 scripts/run_labyrinth_ablation.py --cases ibm01 ibm02 ibm03
```

### Important behavior

- The script removes the default KaHyPar partition output before each run, so it does not silently reuse stale partition files.
- Metrics are only recorded for successful runs.
- `cut` runs on larger Labyrinth cases can be much slower than `tob`, so the script uses:
  - KaHyPar internal `--time-limit 180`
  - external per-run timeout `420s`

You can override those defaults, for example:

```bash
python3 scripts/run_labyrinth_ablation.py --time-limit 300 --timeout 900
```
