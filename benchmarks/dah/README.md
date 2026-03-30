# DAH Benchmarks

This directory contains small Directed Acyclic Hypergraph (`.dah`) samples used to
exercise KaHyPar's DAH input pipeline and TOB objective.

Files included in the repository:

- `toy_pipeline.dah`: a tiny layered pipeline with one merge.
- `fork_join.dah`: a small fork/join dataflow example.
- `c17_excerpt.dah`: a tiny DAG-style excerpt inspired by public logic benchmark structure.

For larger public benchmarks, use [`scripts/download_open_dah_benchmarks.sh`](/Users/limuhan/myProjects/GitHub/kahypar/scripts/download_open_dah_benchmarks.sh).
That script stages source datasets from public repositories and prepares an output
directory for generated `.dah` instances and optional `.hgr + .lvl` companions.
