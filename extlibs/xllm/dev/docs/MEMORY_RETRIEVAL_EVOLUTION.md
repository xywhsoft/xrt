# Memory Retrieval Evolution

This note fixes the near-term boundary for memory retrieval improvements after the v3 baseline.

## Query Rewrite / Expansion Boundary

Decision: query rewrite and expansion should be host-controlled by default, with xllm providing optional primitives later.

Rationale:

- Rewriting a user query can change intent. In an agent stack, the active file, task, conversation, and tool state live above xllm, usually in xwork or the IDE host.
- Provider-backed rewrite adds latency, cost, privacy questions, and provider dependency. It should not become a default memory search prerequisite.
- Deterministic local expansion can be useful, but it must be explainable in diagnostics and bounded by retrieval eval.

Current xllm responsibility:

- Preserve the raw query in search options and diagnostics.
- Support caller-provided rewritten or expanded query text by normal `xllm_memory_search`.
- Keep retrieval deterministic when no host rewrite is supplied.

Future optional xllm primitives:

- A `query_terms` debug dump from the sparse tokenizer.
- A local, deterministic expansion hook limited to aliases, path/title terms, and language identifiers.
- A search option that accepts separate `sOriginalQuery` and `sRetrievalQuery` once diagnostics need both.

Non-goals for core xllm:

- No default LLM rewrite before memory search.
- No hidden semantic expansion that cannot be traced in eval output.
- No product-level approval flow for rewriting; that belongs to host UX.

## Hybrid Weighted Fusion

Decision: keep scaled RRF as the default v3 fusion strategy. Weighted fusion is a v3-later experiment, not a default dependency.

Rationale:

- RRF is stable across sparse-only, vector-only, and hybrid profiles without requiring score normalization guarantees.
- Weighted score fusion needs calibrated lexical/vector scores and a stable eval target. Current sparse BM25-like scores and vector cosine scores are not directly comparable across providers/profiles.
- AI IDE usage will likely need profile-specific weights, for example code-path/title heavy retrieval versus conversation semantic retrieval.

Experiment gate:

- Add weighted fusion only behind an explicit experimental option.
- Report lexical score, vector score, normalized score, chosen weight, and final score in retrieval debug output.
- Require `memory_eval_v1` and future larger eval sets to show no regression in recall@5 and MRR before promoting.
- Keep RRF as the fallback when vector embeddings are unavailable or score normalization cannot be computed.

Suggested experimental configuration:

```text
final_score = (lexical_weight * normalized_lexical_score) + (vector_weight * normalized_vector_score) + metadata_boost
```

Initial candidate weights:

- Code/workspace search: lexical 0.65, vector 0.25, metadata/title/path 0.10.
- Documentation search: lexical 0.45, vector 0.45, metadata/title/path 0.10.
- Conversation memory search: lexical 0.30, vector 0.60, metadata/recency/priority 0.10.

## Eval Requirements

Before enabling either feature by default:

- Compare against a baseline report using `build.bat memory-eval -BaselineReport <json>`.
- Track per-group metrics for code, docs, and conversation cases.
- Inspect per-query rank deltas, not only aggregate metrics.
- Preserve deterministic behavior for builtin sparse eval.

Future HNSW / external vector DB adapter policy is documented in `docs\MEMORY_VECTOR_INDEX_EVOLUTION.md`.
