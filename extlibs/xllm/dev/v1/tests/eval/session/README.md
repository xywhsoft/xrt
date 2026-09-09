# xllm Session Summary Benchmark

This directory contains the deterministic local benchmark for session summary quality and token-budget behavior.

Run:

```bat
build.bat session-summary-bench
```

Outputs:

- `build/session_summary_benchmark/session_summary_benchmark_report.json`
- `build/session_summary_benchmark/session_summary_benchmark_report.txt`

Metrics:

- `avg_keyword_retention`: expected durable keywords retained in the generated session summary.
- `avg_token_compression`: relative reduction from compact input tokens before to after.
- `avg_latency_ms`: local compact latency for the deterministic mock summarizer.
- `avg_summary_chars`: generated summary size.
