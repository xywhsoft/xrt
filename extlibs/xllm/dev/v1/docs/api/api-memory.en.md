# xllm Memory API

> Header: `xllm-memory.h`

This page explains the basic objects of the memory subsystem: how to create a memory store, choose a retrieval scheme, configure embedding, and inspect the basic state of the current memory store.  
If you only want to write text or files into memory, read the creation section here first, then continue with [Memory Ingest API](api-memory-ingest.en.md).

## What You Will Learn

- What `xllm_memory` is and how it relates to `xllm_runtime`.
- The difference between the `memory` and `knowledge` scopes.
- How to choose among sparse, ONNX E5, and custom embedder schemes.
- How to initialize options, create memory, and destroy memory.
- How to query memory scheme, profile, record count, and chunk count.
- How to use built-in embedder probe and creation APIs.

## Core Concepts

### memory store

`xllm_memory` represents a searchable memory store. It stores records, chunks, metadata, and optional vector indexes. You can think of it as xllm's built-in lightweight RAG storage layer.

Typical call order:

1. Create `xllm_runtime`.
2. Initialize and fill `xllm_memory_options`.
3. Call `xllm_memory_create` to get `xllm_memory *`.
4. Use ingest APIs to write content.
5. Use search APIs to retrieve content, or inject search results into a request/turn.
6. Call `xllm_memory_destroy` to release memory.
7. Call `xllm_runtime_destroy` to release the runtime.

### scope

`xllm_memory_scope` distinguishes the purpose of memory.

| Enum Value | Meaning | Suitable Content |
| --- | --- | --- |
| `XLLM_MEMORY_SCOPE_ANY` | No scope restriction during query | Wildcard scope for search, statistics, and deletion |
| `XLLM_MEMORY_SCOPE_MEMORY` | Conversation memory | User preferences, tasks, facts, conversation summaries |
| `XLLM_MEMORY_SCOPE_KNOWLEDGE` | Knowledge material | Docs, source code, workspace files, knowledge-base fragments |

A simple learner rule: put user/conversation-related information into `MEMORY`, and external materials/files into `KNOWLEDGE`.

### record and chunk

A record is one logical ingested object, such as a note, file, or preference. A chunk is a text fragment split from a record for retrieval.

Search returns chunk hits, but each hit carries its record ID, title, source URI, and metadata. You can see both the matched text and where it came from.

## Constants

### Compile-Time Scheme Switches

**Purpose:** control which memory retrieval schemes are included in the current build.

```c
#define XLLM_MEMORY_SCHEME_MODE_ONNX_E5 1
#define XLLM_MEMORY_SCHEME_MODE_BUILTIN_SPARSE 2
#define XLLM_MEMORY_SCHEME_MODE_CUSTOM 3
#define XLLM_MEMORY_SCHEME_MODE_ALL 4

#ifndef XLLM_MEMORY_SCHEME_MODE
#define XLLM_MEMORY_SCHEME_MODE XLLM_MEMORY_SCHEME_MODE_ALL
#endif
```

**Notes:**

- `XLLM_MEMORY_SCHEME_MODE_ALL` is the default and means compile as many available schemes as possible.
- `BUILTIN_SPARSE` does not depend on external embedding models and is suitable for getting started and testing.
- `ONNX_E5` depends on built-in ONNX E5 embedder assets and is suitable for semantic retrieval.
- `CUSTOM` is for providing your own embedding callbacks.

## Types

### xllm_memory

**Purpose:** opaque handle for a memory store.

```c
typedef struct xllm_memory xllm_memory;
```

You cannot access its fields directly. Use APIs to operate on it. This design lets xllm change internal storage structures without breaking your code.

### xllm_memory_scheme

**Purpose:** selects the retrieval and embedding scheme for memory.

```c
typedef enum {
    XLLM_MEMORY_SCHEME_AUTO = 0,
    XLLM_MEMORY_SCHEME_BUILTIN_SPARSE,
    XLLM_MEMORY_SCHEME_ONNX_E5,
    XLLM_MEMORY_SCHEME_CUSTOM
} xllm_memory_scheme;
```

| Value | Meaning |
| --- | --- |
| `XLLM_MEMORY_SCHEME_AUTO` | Let xllm choose an available scheme automatically. |
| `XLLM_MEMORY_SCHEME_BUILTIN_SPARSE` | Use built-in sparse retrieval; no vector model required. |
| `XLLM_MEMORY_SCHEME_ONNX_E5` | Use built-in multilingual E5 small ONNX embedder. |
| `XLLM_MEMORY_SCHEME_CUSTOM` | Use a custom embedder supplied by you. |

**Learning advice:** choose `AUTO` or leave it unset the first time. If you want to eliminate model asset issues, explicitly set `BUILTIN_SPARSE`.

### xllm_memory_embed_task

**Purpose:** tells the embedder whether it is encoding a query or a document.

```c
typedef enum {
    XLLM_MEMORY_EMBED_QUERY = 1,
    XLLM_MEMORY_EMBED_DOCUMENT
} xllm_memory_embed_task;
```

Some embedding models use different prefixes for queries and documents, such as the common E5 `query:` and `passage:` prefixes. A custom embedder can use this value to choose the right encoding mode.

### xllm_memory_embedding

**Purpose:** stores one embedding result.

```c
typedef struct {
    float *pfValues;
    uint32 uValueCount;
} xllm_memory_embedding;
```

| Field | Meaning |
| --- | --- |
| `pfValues` | Floating-point vector array. |
| `uValueCount` | Vector dimension. |

**Notes:** If you provide a custom embedder, `pfnResetEmbedding` must know how to release `pfValues`. If vector memory is allocated by your embedder, it should also be released by your reset callback.

### xllm_memory_embedder

**Purpose:** describes an embedding provider.

```c
typedef struct {
    xllm_memory_embed_text_fn pfnEmbedText;
    xllm_memory_embed_reset_fn pfnResetEmbedding;
    xllm_memory_embed_clone_fn pfnCloneCtx;
    xllm_memory_embed_dispose_fn pfnDisposeCtx;
    void *pCtx;
    xvalue tVendorExtra;
} xllm_memory_embedder;
```

| Field | Meaning |
| --- | --- |
| `pfnEmbedText` | Callback that converts text to vectors. |
| `pfnResetEmbedding` | Callback that releases or resets `xllm_memory_embedding`. |
| `pfnCloneCtx` | Callback that clones embedder context; may be `NULL`. |
| `pfnDisposeCtx` | Callback that destroys embedder context; may be `NULL`. |
| `pCtx` | Your embedder context, such as a model handle. |
| `tVendorExtra` | Extension field reserved for integrators. |

**Notes:** If you choose `XLLM_MEMORY_SCHEME_CUSTOM`, you usually need at least `pfnEmbedText` and `pfnResetEmbedding`.

### xllm_memory_builtin_embedder_kind

**Purpose:** selects a built-in embedder provided by xllm.

```c
typedef enum {
    XLLM_MEMORY_BUILTIN_EMBEDDER_NONE = 0,
    XLLM_MEMORY_BUILTIN_EMBEDDER_HASH,
    XLLM_MEMORY_BUILTIN_EMBEDDER_MULTILINGUAL_E5_SMALL_ONNX
} xllm_memory_builtin_embedder_kind;
```

| Value | Meaning |
| --- | --- |
| `NONE` | Do not use a built-in embedder. |
| `HASH` | Use the hash built-in embedder, suitable for testing and no-model environments. |
| `MULTILINGUAL_E5_SMALL_ONNX` | Use multilingual E5 small ONNX embedder. |

### xllm_memory_builtin_embedder_options

**Purpose:** configures model, tokenizer, and runtime assets for a built-in embedder.

```c
typedef struct {
    xllm_memory_builtin_embedder_kind eKind;
    const char *sRuntimeDllPath;
    const char *sModelPath;
    const char *sTokenizerPath;
    bool bAutoDiscoverAssets;
    uint32 uHashDimensions;
    xvalue tVendorExtra;
} xllm_memory_builtin_embedder_options;
```

| Field | Meaning |
| --- | --- |
| `eKind` | Built-in embedder kind. |
| `sRuntimeDllPath` | ONNX Runtime dynamic library path; may be `NULL` to let xllm auto-discover. |
| `sModelPath` | Embedding model path. |
| `sTokenizerPath` | Tokenizer path. |
| `bAutoDiscoverAssets` | Whether to auto-discover model-related assets. |
| `uHashDimensions` | Vector dimension for the `HASH` embedder. |
| `tVendorExtra` | Extension field. |

### xllm_memory_builtin_embedder_probe

**Purpose:** stores probe results for a built-in embedder.

```c
typedef struct {
    xllm_memory_builtin_embedder_kind eKind;
    bool bRuntimeFound;
    bool bModelFound;
    bool bTokenizerFound;
    bool bImplemented;
    bool bReady;
    char *sResolvedRuntimeDllPath;
    char *sResolvedModelPath;
    char *sResolvedTokenizerPath;
} xllm_memory_builtin_embedder_probe;
```

**Cleanup rule:** call `xllm_memory_builtin_embedder_probe_reset` to release strings allocated by xllm in the probe result.

### xllm_memory_options

**Purpose:** configuration used when creating a memory store.

```c
typedef struct {
    const char *sNamespace;
    const char *sSqlitePath;
    const char *sSqliteVectorExtensionPath;
    bool bLoadSqliteVectorExtension;
    xllm_memory_embedder tEmbedder;
    xllm_memory_scheme eScheme;
    const char *sMemoryProfileId;
    bool bEnableHybridSearch;
    xllm_opt_float tLexicalWeight;
    xllm_opt_float tVectorWeight;
    uint32 uDefaultChunkChars;
    uint32 uDefaultChunkOverlapChars;
    uint32 uDefaultMaxHits;
    uint32 uSqliteBusyTimeoutMs;
    bool bDisableSqliteWal;
    xvalue tVendorExtra;
} xllm_memory_options;
```

| Field | Meaning |
| --- | --- |
| `sNamespace` | Namespace used to separate applications or tenants. |
| `sSqlitePath` | SQLite file path. If `NULL`, memory or default storage strategy is used. |
| `sSqliteVectorExtensionPath` | sqlite-vector extension path. |
| `bLoadSqliteVectorExtension` | Whether to load the SQLite vector extension. |
| `tEmbedder` | Custom embedder or embedder created from built-in factory. |
| `eScheme` | Retrieval scheme. |
| `sMemoryProfileId` | Memory profile identifier used to detect storage configuration match. |
| `bEnableHybridSearch` | Whether lexical + vector hybrid search is enabled. |
| `tLexicalWeight` | Lexical search weight. |
| `tVectorWeight` | Vector search weight. |
| `uDefaultChunkChars` | Default chunk character count. |
| `uDefaultChunkOverlapChars` | Default chunk overlap character count. |
| `uDefaultMaxHits` | Default search hit count. |
| `uSqliteBusyTimeoutMs` | SQLite busy timeout. |
| `bDisableSqliteWal` | Whether to disable WAL. |
| `tVendorExtra` | Extension field. |

**Defaults:** `xllm_memory_options_init` sets scheme to `AUTO`, enables hybrid search, sets lexical/vector weights to `1.0`, chunk size to `800`, overlap to `120`, default hits to `5`, and SQLite busy timeout to `5000ms`.

## API

### xllm_memory_embedder_init

**Purpose:** initializes `xllm_memory_embedder`.

**Prototype:**

```c
XLLM_API void xllm_memory_embedder_init(xllm_memory_embedder *pEmbedder);
```

**Parameters:**

| Parameter | Description |
| --- | --- |
| `pEmbedder` | Embedder structure to initialize. May be `NULL`. |

**Return Value:** none.

**Notes:** This function zeroes the structure. After declaring `xllm_memory_embedder` on the stack, call it before filling callback fields.

**Example Code:**

```c
xllm_memory_embedder embedder;
xllm_memory_embedder_init(&embedder);
embedder.pfnEmbedText = my_embed_text;
embedder.pfnResetEmbedding = my_reset_embedding;
embedder.pCtx = my_model;
```

### xllm_memory_embedder_reset

**Purpose:** releases context held by the embedder and clears the structure.

**Prototype:**

```c
XLLM_API void xllm_memory_embedder_reset(xllm_memory_embedder *pEmbedder);
```

**Parameters:**

| Parameter | Description |
| --- | --- |
| `pEmbedder` | Embedder to reset. |

**Return Value:** none.

**Notes:** If `pEmbedder->pfnDisposeCtx` is set, reset releases `pCtx` through that callback. This is especially important for embedders created by `xllm_memory_make_builtin_embedder`.

### xllm_memory_builtin_embedder_options_init

**Purpose:** initializes built-in embedder options.

**Prototype:**

```c
XLLM_API void xllm_memory_builtin_embedder_options_init(
    xllm_memory_builtin_embedder_options *pOptions
);
```

**Parameters:**

| Parameter | Description |
| --- | --- |
| `pOptions` | Options to initialize. |

**Return Value:** none.

**Notes:** Initialize first, then set `eKind`, model path, or auto-discovery options. Do not manually zero the structure and skip init, because defaults may evolve by version.

### xllm_memory_builtin_embedder_probe_reset

**Purpose:** releases a built-in embedder probe result.

**Prototype:**

```c
XLLM_API void xllm_memory_builtin_embedder_probe_reset(
    xllm_memory_builtin_embedder_probe *pProbe
);
```

**Parameters:**

| Parameter | Description |
| --- | --- |
| `pProbe` | Probe result to release. |

**Return Value:** none.

**Notes:** `xllm_memory_probe_builtin_embedder` may write resolved path strings into the probe. Reset it after use.

### xllm_memory_probe_builtin_embedder

**Purpose:** checks whether required assets for a built-in embedder are available.

**Prototype:**

```c
XLLM_API int xllm_memory_probe_builtin_embedder(
    const xllm_memory_builtin_embedder_options *pOptions,
    xllm_memory_builtin_embedder_probe *pProbe,
    xllm_error *pError
);
```

**Parameters:**

| Parameter | Description |
| --- | --- |
| `pOptions` | Built-in embedder configuration. |
| `pProbe` | Output probe result. |
| `pError` | Error object, may be `NULL`. |

**Return Value:**

| Return Value | Meaning |
| --- | --- |
| `XRT_NET_OK` | Probe completed. Check `pProbe->bReady` to know whether it is usable. |
| `XRT_NET_ERROR` | Invalid parameter or probe failure. |

**Notes:** A successful probe does not mean the embedder is ready. For example, ONNX runtime may be found while the model file is missing; the function can still finish with `bReady=false`.

**Example Code:**

```c
xllm_memory_builtin_embedder_options options;
xllm_memory_builtin_embedder_probe probe;
xllm_error error;

xllm_error_init(&error);
xllm_memory_builtin_embedder_options_init(&options);
memset(&probe, 0, sizeof(probe));

options.eKind = XLLM_MEMORY_BUILTIN_EMBEDDER_MULTILINGUAL_E5_SMALL_ONNX;
options.bAutoDiscoverAssets = true;

if (xllm_memory_probe_builtin_embedder(&options, &probe, &error) == XRT_NET_OK) {
    if (probe.bReady) {
        /* You can create the ONNX E5 embedder. */
    }
}

xllm_memory_builtin_embedder_probe_reset(&probe);
xllm_error_reset(&error);
```

### xllm_memory_make_builtin_embedder

**Purpose:** creates a built-in embedder from options.

**Prototype:**

```c
XLLM_API int xllm_memory_make_builtin_embedder(
    const xllm_memory_builtin_embedder_options *pOptions,
    xllm_memory_embedder *pEmbedder,
    xllm_error *pError
);
```

**Parameters:**

| Parameter | Description |
| --- | --- |
| `pOptions` | Built-in embedder configuration. |
| `pEmbedder` | Output embedder. |
| `pError` | Error object, may be `NULL`. |

**Return Value:**

| Return Value | Meaning |
| --- | --- |
| `XRT_NET_OK` | Created successfully. |
| `XRT_NET_ERROR` | Incomplete configuration, unavailable assets, or internal initialization failure. |

**Notes:** After success, `pEmbedder` owns resources. Eventually call `xllm_memory_embedder_reset`, or pass it to `xllm_memory_create` and let memory manage its lifecycle.

### xllm_memory_options_init

**Purpose:** initializes `xllm_memory_options` with recommended defaults.

**Prototype:**

```c
XLLM_API void xllm_memory_options_init(xllm_memory_options *pOptions);
```

**Parameters:**

| Parameter | Description |
| --- | --- |
| `pOptions` | Options to initialize. May be `NULL`. |

**Return Value:** none.

**Defaults:**

| Field | Default |
| --- | --- |
| `eScheme` | `XLLM_MEMORY_SCHEME_AUTO` |
| `bEnableHybridSearch` | `true` |
| `tLexicalWeight` | set, `1.0` |
| `tVectorWeight` | set, `1.0` |
| `uDefaultChunkChars` | `800` |
| `uDefaultChunkOverlapChars` | `120` |
| `uDefaultMaxHits` | `5` |
| `uSqliteBusyTimeoutMs` | `5000` |

**Example Code:**

```c
xllm_memory_options options;
xllm_memory_options_init(&options);
options.sNamespace = "demo";
options.sSqlitePath = "demo-memory.sqlite3";
options.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
```

### xllm_memory_create

**Purpose:** creates a memory store.

**Prototype:**

```c
XLLM_API int xllm_memory_create(
    xllm_runtime *pRuntime,
    const xllm_memory_options *pOptions,
    xllm_memory **ppMemory
);
```

**Parameters:**

| Parameter | Description |
| --- | --- |
| `pRuntime` | xllm runtime. |
| `pOptions` | Memory configuration. May be `NULL` to use default options. |
| `ppMemory` | Output memory pointer. |

**Return Value:**

| Return Value | Meaning |
| --- | --- |
| `XRT_NET_OK` | Created successfully. |
| `XRT_NET_ERROR` | Invalid runtime, unavailable scheme, database initialization failure, or embedder initialization failure. |

**Notes:**

- `pRuntime` must outlive `xllm_memory`.
- If `sSqlitePath` is set, memory content is stored in a SQLite file.
- If you choose `BUILTIN_SPARSE`, ingest and search work without an embedding model.
- If you choose `CUSTOM`, provide valid callbacks in `pOptions->tEmbedder`.

**Example Code:**

```c
xllm_runtime *runtime = NULL;
xllm_memory *memory = NULL;
xllm_runtime_options rt_options;
xllm_memory_options mem_options;

xllm_runtime_options_init(&rt_options);
xllm_runtime_create(&rt_options, &runtime);

xllm_memory_options_init(&mem_options);
mem_options.sNamespace = "tutorial";
mem_options.sSqlitePath = "tutorial-memory.sqlite3";
mem_options.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;

if (xllm_memory_create(runtime, &mem_options, &memory) != XRT_NET_OK) {
    /* Handle error. */
}

xllm_memory_destroy(memory);
xllm_runtime_destroy(runtime);
```

### xllm_memory_destroy

**Purpose:** destroys a memory store and releases internal resources.

**Prototype:**

```c
XLLM_API void xllm_memory_destroy(xllm_memory *pMemory);
```

**Parameters:**

| Parameter | Description |
| --- | --- |
| `pMemory` | Memory to destroy. May be `NULL`. |

**Return Value:** none.

**Notes:** Do not use result objects previously obtained from this memory after destroying it. Strings independently copied into your own memory are not affected.

### xllm_memory_get_scheme

**Purpose:** reads the actual retrieval scheme used by current memory.

**Prototype:**

```c
XLLM_API xllm_memory_scheme xllm_memory_get_scheme(const xllm_memory *pMemory);
```

**Parameters:**

| Parameter | Description |
| --- | --- |
| `pMemory` | Memory object. |

**Return Value:** current actual scheme.

**Notes:** If memory was created with `AUTO`, this function tells you whether it resolved to sparse, ONNX E5, or custom.

### xllm_memory_get_profile_id

**Purpose:** reads the current memory profile ID.

**Prototype:**

```c
XLLM_API const char *xllm_memory_get_profile_id(const xllm_memory *pMemory);
```

**Parameters:**

| Parameter | Description |
| --- | --- |
| `pMemory` | Memory object. |

**Return Value:** profile ID string. The pointer is owned by memory; do not free it.

**Notes:** Profile ID helps determine whether an existing SQLite memory file matches the current embedding dimension, retrieval scheme, and similar configuration.

### xllm_memory_record_count

**Purpose:** counts records.

**Prototype:**

```c
XLLM_API size_t xllm_memory_record_count(
    const xllm_memory *pMemory,
    xllm_memory_scope eScope
);
```

**Parameters:**

| Parameter | Description |
| --- | --- |
| `pMemory` | Memory object. |
| `eScope` | Scope to count, commonly `ANY`, `MEMORY`, or `KNOWLEDGE`. |

**Return Value:** record count.

**Notes:** This is a lightweight status query, useful after ingesting to confirm writes happened.

### xllm_memory_chunk_count

**Purpose:** counts chunks.

**Prototype:**

```c
XLLM_API size_t xllm_memory_chunk_count(
    const xllm_memory *pMemory,
    xllm_memory_scope eScope
);
```

**Parameters:**

| Parameter | Description |
| --- | --- |
| `pMemory` | Memory object. |
| `eScope` | Scope to count. |

**Return Value:** chunk count.

**Notes:** One record may produce multiple chunks, so chunk count is usually greater than or equal to record count.

## Complete Example: Create a Minimal Memory Store

```c
#include "xllm.h"
#include "xllm-memory.h"

int main(void)
{
    xllm_runtime *runtime = NULL;
    xllm_memory *memory = NULL;
    xllm_runtime_options rt_options;
    xllm_memory_options mem_options;

    xllm_runtime_options_init(&rt_options);
    if (xllm_runtime_create(&rt_options, &runtime) != XRT_NET_OK) {
        return 1;
    }

    xllm_memory_options_init(&mem_options);
    mem_options.sNamespace = "learn-xllm";
    mem_options.sSqlitePath = "learn-xllm.sqlite3";
    mem_options.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;

    if (xllm_memory_create(runtime, &mem_options, &memory) != XRT_NET_OK) {
        xllm_runtime_destroy(runtime);
        return 1;
    }

    printf("records=%zu chunks=%zu\n",
        xllm_memory_record_count(memory, XLLM_MEMORY_SCOPE_ANY),
        xllm_memory_chunk_count(memory, XLLM_MEMORY_SCOPE_ANY));

    xllm_memory_destroy(memory);
    xllm_runtime_destroy(runtime);
    return 0;
}
```

## Common Mistakes

### Writing conversation memory into KNOWLEDGE

If you save preferences such as "the user likes concise answers", use `XLLM_MEMORY_SCOPE_MEMORY`. `KNOWLEDGE` is better for docs, files, and reference material.

### Creating custom scheme without an embedder

`XLLM_MEMORY_SCHEME_CUSTOM` needs a valid `pfnEmbedText`. If you only want to try memory quickly, start with `BUILTIN_SPARSE`.

### Forgetting to release probe or result objects

Release `xllm_memory_builtin_embedder_probe` results with `xllm_memory_builtin_embedder_probe_reset`. Search, list, debug, and other result objects also have matching reset APIs.

## Related Documentation

- [Memory Ingest API](api-memory-ingest.en.md)
- [Memory Search API](api-memory-search.en.md)
- [Memory Workspace API](api-memory-workspace.en.md)
- [Diagnostics API](api-diagnostics.en.md)
- [Back to API Index](README.en.md)
