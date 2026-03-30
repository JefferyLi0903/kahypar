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
