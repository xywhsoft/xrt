# xllm Diagnostics API

> Headers: `xllm.h`, `xllm-memory.h`

This page brings xllm diagnostics into one place. You will learn how to inspect version, attach logs, attach traces, read error objects, and get memory diagnostic snapshots.

## Where Diagnostics Come From

| Source | Purpose |
| --- | --- |
| `xllm_version` | Confirm runtime version. |
| log callback | Record local runtime logs. |
| trace callback | Observe requests, responses, streaming events, session compacting, and tool loops. |
| `xllm_error` | Get concrete failure reason, HTTP status, and provider error for one failed call. |
| `xllm_memory_get_diagnostics` | Get memory scheme, profile, SQLite, embedder, record/chunk status. |

Diagnostics are local and opt-in by default. xllm does not automatically send this information to external services.

## API: Version

### xllm_version

**Purpose:** returns the xllm version string.

**Prototype:**

```c
XLLM_API const char *xllm_version(void);
```

**Return Value:** version string. The caller must not free it.

**Related Constants:**

```c
#define XLLM_VERSION_MAJOR 0
#define XLLM_VERSION_MINOR 1
#define XLLM_VERSION_PATCH 0
```

**Example Code:**

```c
printf("xllm version: %s\n", xllm_version());
```

## API: Log and Trace Names

### xllm_log_event_name

**Purpose:** converts a log event enum to a stable name.

```c
XLLM_API const char *xllm_log_event_name(xllm_log_event eEvent);
```

The returned string is borrowed and must not be freed.

### xllm_log_level_name

**Purpose:** converts a log level to a stable name.

```c
XLLM_API const char *xllm_log_level_name(xllm_log_level eLevel);
```

### xllm_trace_kind_name

**Purpose:** converts a trace kind to a stable name.

```c
XLLM_API const char *xllm_trace_kind_name(xllm_trace_kind eKind);
```

## API: Setting Callbacks

### xllm_runtime_set_log_callback

**Purpose:** sets the runtime log callback.

```c
XLLM_API int xllm_runtime_set_log_callback(
    xllm_runtime *pRuntime,
    xllm_log_callback pfnLog,
    void *pLogCtx
);
```

| Parameter | Description |
| --- | --- |
| `pRuntime` | Runtime object. |
| `pfnLog` | Log callback. Pass `NULL` to disable. |
| `pLogCtx` | User context passed to the callback. |

Returns `XRT_NET_OK` on success and `XRT_NET_ERROR` on failure.

Callback prototype:

```c
typedef void (*xllm_log_callback)(
    void *pCtx,
    xllm_log_level eLevel,
    const char *sComponent,
    const char *sMessage
);
```

### xllm_runtime_set_trace_callback

**Purpose:** sets the runtime trace callback.

```c
XLLM_API int xllm_runtime_set_trace_callback(
    xllm_runtime *pRuntime,
    xllm_trace_callback pfnTrace,
    void *pTraceCtx
);
```

Callback prototype:

```c
typedef void (*xllm_trace_callback)(
    void *pCtx,
    xllm_trace_kind eKind,
    const xvalue *pPayload
);
```

Trace kinds:

```c
typedef enum {
    XLLM_TRACE_EVENT = 1,
    XLLM_TRACE_REQUEST,
    XLLM_TRACE_RESPONSE,
    XLLM_TRACE_STREAM,
    XLLM_TRACE_COMPACT,
    XLLM_TRACE_TOOL_LOOP
} xllm_trace_kind;
```

**Notes:** trace payload is structured `xvalue`. The host can convert it into log lines, IDE panels, debug records, or test reports.

## API: Error Object

### xllm_error

**Purpose:** stores diagnostic information for one failed API call.

```c
typedef struct {
    xllm_error_code eCode;
    int32 iStatus;
    int32 iHttpStatus;
    const char *sMessage;
    const char *sProviderCode;
    const char *sProviderMessage;
    const char *sRequestId;
    int32 iMessageIndex;
    int32 iPartIndex;
    xllm_capability_flags uRequiredCapability;
    const char *sSelectedModel;
    const char *sMimeType;
    xvalue tVendorExtra;
} xllm_error;
```

| Field | Meaning |
| --- | --- |
| `eCode` | Normalized xllm error code. |
| `iStatus` | Lower-level status, usually related to `XRT_NET_OK/ERROR`. |
| `iHttpStatus` | Upstream HTTP status. May be `0` if HTTP was not reached. |
| `sMessage` | Error message suitable for display or logs. |
| `sRequestId` | Upstream request ID, if provided by provider. |
| `sProviderCode` | Raw provider error code. |
| `sProviderMessage` | Raw provider error message. |
| `iMessageIndex` | Input message index where the error occurred. |
| `iPartIndex` | Content part index where the error occurred, useful for multimodal input. |
| `uRequiredCapability` | Capability required but unsupported by the current model. |
| `sSelectedModel` | Model actually selected or checked. |
| `sMimeType` | MIME type of the problematic content. |
| `tVendorExtra` | Extension field. |

### xllm_error_init / reset / free

**Purpose:** initialize, reset, and free an error object.

```c
XLLM_API void xllm_error_init(xllm_error *pError);
XLLM_API void xllm_error_reset(xllm_error *pError);
XLLM_API void xllm_error_free(xllm_error *pError);
```

**Notes:** both `reset` and `free` release internal strings. The difference is mainly naming semantics. You can use `init` + `reset` on stack objects, and `free` before destroying heap objects.

## API: Memory Diagnostics

### xllm_memory_diagnostics

**Purpose:** stores a memory diagnostic snapshot.

```c
typedef struct {
    xllm_memory_scheme eScheme;
    const char *sMemoryProfileId;
    const char *sEmbedProfileId;
    const char *sNamespace;
    const char *sSqlitePath;
    const char *sSqliteVectorExtensionPath;
    xllm_memory_builtin_embedder_kind eBuiltinEmbedderKind;
    const char *sEmbedderKind;
    const char *sEmbedModelId;
    const char *sEmbedRepoId;
    const char *sEmbedRuntimeDllPath;
    const char *sEmbedModelPath;
    const char *sEmbedTokenizerPath;
    const char *sEmbedQueryPrefix;
    const char *sEmbedDocumentPrefix;
    const char *sEmbedPoolingMode;
    bool bSqliteOpen;
    bool bSqliteWalEnabled;
    bool bSqliteWalRequested;
    bool bSqliteVectorExtensionRequested;
    bool bSqliteVectorExtensionLoaded;
    bool bVectorTableReady;
    bool bHybridSearchEnabled;
    bool bEmbedderConfigured;
    bool bOwnsEmbedderCtx;
    bool bEmbedNormalize;
    bool bStorageProfileMatch;
    uint32 uVectorDim;
    uint32 uEmbedDimensions;
    uint32 uEmbedMaxInputTokens;
    uint32 uSqliteSchemaVersion;
    uint32 uSqliteBusyTimeoutMs;
    uint32 uDefaultChunkChars;
    uint32 uDefaultChunkOverlapChars;
    uint32 uDefaultMaxHits;
    double fLexicalWeight;
    double fVectorWeight;
    const char *sStoredMemoryProfileId;
    size_t iRecordCount;
    size_t iMemoryRecordCount;
    size_t iKnowledgeRecordCount;
    size_t iChunkCount;
    size_t iMemoryChunkCount;
    size_t iKnowledgeChunkCount;
} xllm_memory_diagnostics;
```

### xllm_memory_diagnostics_init

Initializes a memory diagnostics result.

```c
XLLM_API void xllm_memory_diagnostics_init(
    xllm_memory_diagnostics *pDiagnostics
);
```

### xllm_memory_get_diagnostics

Gets the current memory diagnostic snapshot.

```c
XLLM_API int xllm_memory_get_diagnostics(
    const xllm_memory *pMemory,
    xllm_memory_diagnostics *pDiagnostics,
    xllm_error *pError
);
```

Returns `XRT_NET_OK` on success and `XRT_NET_ERROR` on failure.

**Example Code:**

```c
xllm_memory_diagnostics diag;
xllm_memory_diagnostics_init(&diag);

if (xllm_memory_get_diagnostics(memory, &diag, NULL) == XRT_NET_OK) {
    printf("scheme=%d records=%zu chunks=%zu\n",
        (int)diag.eScheme,
        diag.iRecordCount,
        diag.iChunkCount);
}
```

## Example: Attach Trace Callback

```c
static void on_trace(void *ctx, xllm_trace_kind kind, const xvalue *payload)
{
    (void)ctx;
    printf("trace kind=%s payload=%p\n", xllm_trace_kind_name(kind), (const void *)payload);
}

xllm_runtime_set_trace_callback(runtime, on_trace, NULL);
```

## Troubleshooting Advice

- Call failed: inspect `xllm_error.eCode`, `sMessage`, `iHttpStatus`, and `sProviderCode` first.
- Search has no hits: inspect `xllm_memory_search_debug`, then `xllm_memory_get_diagnostics`.
- Provider behavior looks wrong: enable trace and inspect `REQUEST` and `RESPONSE` payloads.
- Session auto compact looks wrong: inspect `XLLM_TRACE_COMPACT`.
- Tool loop looks wrong: inspect `XLLM_TRACE_TOOL_LOOP`.

## Related Documentation

- [Core API](api-core.en.md)
- [Request/Response API](api-request-response.en.md)
- [Memory Search API](api-memory-search.en.md)
- [Back to API Index](README.en.md)
