# Memory Vector Index Evolution

This document defines the boundary for future HNSW and external vector database integrations.

## Current Baseline

xllm currently supports:

- builtin sparse retrieval.
- vector embedding storage in SQLite-backed chunks.
- flat in-memory cosine fallback.
- optional sqlite-vec acceleration when `sqlite-vec.dll` is present and explicitly loaded.
- hybrid RRF fusion with lexical/vector score and rank diagnostics.

This baseline keeps local memory usable without a mandatory vector database dependency.

## Decision

HNSW and external vector databases are future optional adapters, not default dependencies.

Rationale:

- AI IDE / claw needs a reliable local default that works from a release bundle without services.
- External vector DBs introduce deployment, auth, tenancy, network, versioning, and deletion semantics that belong to host/product policy.
- HNSW libraries add native dependency and index persistence compatibility risk.
- xllm already has a vindex boundary where optional acceleration can be added without changing memory/search public semantics.

## Adapter Boundary

Future vector index adapters should live behind the internal vindex layer.

They may own:

- vector upsert/delete/query operations.
- approximate nearest-neighbor index lifecycle.
- adapter-specific index metadata.
- adapter-specific diagnostics.

They must not own:

- memory namespace/profile authority.
- record/chunk source-of-truth metadata.
- text chunking.
- embedding model execution.
- final hybrid fusion.
- product tenancy, auth, or network retry policy.

xllm store remains the source of truth for records, chunks, metadata, and delete/export mapping.

## Required Adapter Contract

A future adapter must provide:

- create/open/close.
- upsert vector for `(namespace, scope, record_id, chunk_index)`.
- delete by record and delete by namespace/scope.
- query top-k by embedding vector.
- health/diagnostics with index profile id, dimension, indexed vector count, and last error.
- deterministic fallback behavior when unavailable.

External adapters must also define:

- tenant/namespace mapping.
- auth ownership.
- timeout/retry ownership.
- delete consistency guarantees.
- offline/degraded behavior.

These host/product concerns should not become required xllm core configuration.

## Promotion Gates

Before adding HNSW or an external vector adapter to bundled xllm:

- keep builtin sparse and flat fallback fully functional without the adapter.
- add focused smoke coverage for adapter unavailable, adapter available, upsert/delete/query, and reopen/reload.
- add health-check coverage for vector count/profile/dimension mismatch.
- run `memory-eval` and compare against current hybrid baseline.
- document migration and rollback if the adapter persists its own index files.
- prove delete/export governance remains correct through SQLite record/chunk metadata.

## Recommended Roadmap

1. Keep current sqlite-vec optional acceleration as the only bundled vector accelerator.
2. Add an internal vindex adapter interface only when a second implementation is ready.
3. Prototype HNSW as experimental build-time optional code, disabled by default.
4. Let host-owned external vector DB integrations mirror xllm records from SQLite rather than replacing SQLite metadata.
5. Promote only after eval and production hardening show material benefit over sqlite-vec/flat fallback.

## Non-Goals

- No mandatory vector DB server.
- No network vector DB calls in default memory search.
- No replacement of SQLite as the metadata source of truth.
- No product credential or tenant policy inside xllm memory core.
