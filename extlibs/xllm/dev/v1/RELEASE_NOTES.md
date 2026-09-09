# Release Notes

## 0.1.0 - 2026-05-05

### core

- Initial public runtime, provider/profile, request/response, streaming, diagnostics, and tool-call API surface.

### session

- Initial session history, summary/compact, state import/export, and tool-loop helpers.

### memory

- Initial local memory/RAG APIs covering ingest, search, workspace sync, SQLite persistence, diagnostics, and builtin embedders.

### provider

- Initial OpenAI-compatible, Anthropic, Ollama, GLM, Kimi, Minimax, Qwen, Doubao, Gemini, and Vertex Gemini adapter examples and smoke probes.

### release

- Windows-first release tooling baseline with version verification, artifact checksums, compile verification, and downstream smoke.

### security

- Default release artifacts are checksum-verified but not publisher-signed. API keys are provided by downstream hosts and are not bundled.

### known gaps

- Linux/macOS build gates and public artifact signing remain future release tracks.
