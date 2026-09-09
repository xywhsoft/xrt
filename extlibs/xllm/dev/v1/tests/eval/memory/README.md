# xllm Memory Retrieval Eval

This directory contains the fixed local retrieval eval used to track memory search quality across code, documentation, and conversation memory.

The current `memory_eval_v1` dataset is intentionally small and deterministic. It uses the built-in sparse scheme so it can run without model assets or provider credentials.

Run:

```bat
build.bat memory-eval
```

Run the deterministic performance benchmark:

```bat
build.bat memory-bench
```

Compare against a previous report:

```bat
build.bat memory-eval -OutputDir build\memory_eval_current -BaselineReport build\memory_eval_baseline\memory_eval_report.json
```

Outputs:

- `build/memory_eval/memory_eval_report.json`
- `build/memory_eval/memory_eval_report.txt`
- `build/memory_eval_current/memory_eval_compare.json` when `-BaselineReport` is provided
- `build/memory_eval_current/memory_eval_compare.txt` when `-BaselineReport` is provided
- `build/memory_benchmark/memory_benchmark_report.json`
- `build/memory_benchmark/memory_benchmark_report.txt`

Metrics:

- `recall_at_1`
- `recall_at_5`
- `mrr`
- `precision_at_1`
- `precision_at_5`
- `avg_latency_ms`
- `avg_context_chars`

Benchmark metrics:

- `ingest_total_ms`
- `ingest_avg_ms`
- `search_avg_ms`
- `search_p50_ms`
- `search_p95_ms`
- `reload_ms`
- `sqlite_bytes`
- `indexed_records`
- `indexed_chunks`
