# Language-Aware Chunking Strategy

This document evaluates language-aware chunking for AI IDE / claw use cases and defines the gate before it becomes a default memory capability.

## Current Baseline

xllm currently uses a deterministic generic text chunker:

- fixed character budget and overlap.
- preferred breakpoints around blank lines, line breaks, sentence punctuation, and whitespace.
- stable chunk metadata persisted with byte ranges, content hash, previous/next chunk ids, and concrete `chunk_profile_id`.
- chunking changes are treated as memory profile changes and must not be mixed silently with old chunks.

This baseline is production-safe because it is deterministic, dependency-free, and already covered by memory ingest/search/list smoke tests.

## Evaluation Result

Language-aware chunking is useful, but should remain profile-gated rather than silently replacing the default chunker.

Recommended modes:

- `generic`: current behavior; default for compatibility.
- `markdown`: prefer ATX headings, setext headings, fenced code blocks, thematic breaks, and list section boundaries.
- `c_family`: prefer function/type/macro boundaries, blank lines between declarations, and block boundaries for C/C++/Objective-C style files.
- `json`: prefer object/array member boundaries and avoid splitting inside string literals where possible.
- `auto_by_extension`: host-selected mode based on file extension and content sniffing, with fallback to `generic`.

The first implementation should be heuristic and deterministic. Do not add a full compiler/parser dependency to the default build. Parser-backed chunkers can be introduced later as optional host/xwork extensions if eval proves the need.

## Profile And Migration Rules

Language-aware chunking changes stored chunk identity and retrieval behavior. Treat it as a chunk profile change:

- assign a new `chunk_profile_id` and memory profile id when enabled for an existing namespace.
- re-chunk and reindex affected records instead of mixing old and new chunks.
- run retrieval eval before switching a host namespace to the new profile.
- keep copy-forward rollback available as described in `MEMORY_PROFILE_MIGRATION.md`.

Do not enable language-aware chunking as a silent minor-version behavior change.

## API Boundary

The public API should stay additive:

- existing ingest options keep using `uChunkChars` and `uChunkOverlapChars`.
- a future chunker option can be added as a profile-selected enum or string profile field.
- host policy decides whether file extension, MIME type, or explicit user/project config selects a chunker.
- xllm owns deterministic chunk production and persisted profile identity.
- xwork or host owns project-level chunking policy and migration orchestration.

## Acceptance Gate

Before implementation is promoted beyond experimental:

- add focused smoke coverage for markdown, C/C++, and JSON boundary selection.
- add eval cases comparing generic vs language-aware retrieval for code, docs, and config files.
- verify `xllm_memory_list_chunks` exposes stable byte ranges and adjacency after language-aware splitting.
- verify profile mismatch rejects old chunks when a namespace is opened with a new chunk profile.
- document rollback and release notes for hosts rebuilding local indexes.

Until those gates pass, the production recommendation is to keep `generic` as the default and let hosts experiment in separate namespaces.
