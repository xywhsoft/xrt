# xllm-memory v2 MVP

`xllm-memory` is the explicit long-term record and retrieval layer. It does not call a model, inspect a session, execute tools, or decide what should be remembered. `xwork` or the product host owns candidate extraction, user approval, conflict policy, context-injection timing, and any product audit log.

## Guarantees in this milestone

- Pure C API and a single-writer local JSON store.
- Atomic full-store persistence for every successful create, replace, or remove operation.
- Namespace validation prevents accidentally opening one project's store as another project.
- Every record carries a stable record ID, source URI, actor, reason, scope, kind, trust level, sensitivity level, timestamps, record revision, and content fingerprint.
- Every mutation returns a receipt with its action, store revision, record revision, and previous/new fingerprints.
- Replacement is opt-in. Writing the same content is a no-op and does not advance revisions.
- Deterministic local lexical retrieval with exact UTF-8 phrase matching; no provider or embedding dependency.
- Search defaults exclude expired records and records above `internal` sensitivity.
- Rendered context is bounded, source-labelled, and explicitly marked as untrusted reference material.
- Persisted content fingerprints are checked when the store is reopened.

The fingerprint is a deterministic non-cryptographic change detector. It is not a signature, authentication mechanism, or encryption feature. Hosts that need a durable governance audit should persist mutation receipts in their own append-only audit system.

## Minimal flow

```c
#include "xllm-memory.h"

xllm_memory_config config;
xllm_memory_record_input record;
xllm_memory_receipt receipt;
xllm_error error;

xllmMemoryConfigInit(&config);
config.sPath = ".xcode/memory.json";
config.sNamespace = "project-id";

xllm_memory* memory = xllmMemoryOpen(&config, &error);

xllmMemoryRecordInputInit(&record);
record.eKind = XLLM_MEMORY_KIND_FACT;
record.eTrust = XLLM_MEMORY_TRUST_USER_APPROVED;
record.sRecordId = "fact:language-stack";
record.sSourceUri = "user://conversation/42";
record.sText = "This project uses a pure C technology stack.";
record.sActor = "user";
record.sReason = "explicit remember request";

if (!xllmMemoryPut(memory, &record, &receipt, &error)) {
    /* surface error; do not silently downgrade approval or provenance */
}

xllmMemoryClose(memory);
```

## Retrieval boundary

`xllmMemorySearch()` returns owned hits with provenance and record revisions. Call `xllmMemorySearchResultUnit()` after use. An empty query provides a bounded audit listing using the same scope, expiry, and sensitivity filters.

`xllmMemoryRenderContext()` only formats selected hits. It does not inject them into a request. The host decides whether the current task needs memory and where the resulting untrusted reference block belongs in the context budget.

## Deferred capabilities

- SQLite/WAL multi-process storage.
- Chunked workspace/file indexing.
- Sparse inverted indexes and semantic vector retrieval.
- Embedding runtimes and optional vector accelerators.
- Automatic memory candidates, approval UI, conflict resolution, retention policy, export, and deletion governance.

Those features can extend the store/search boundary without making `xllm-session` depend on memory or moving product policy into the library.
