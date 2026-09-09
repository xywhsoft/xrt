# xllm Diagnostics API

> 状态：中文初稿已生成，待审阅。  
> 头文件：`xllm.h`、`xllm-memory.h`

本页把 xllm 的诊断能力集中讲清楚。你会学到如何查看版本、如何接日志、如何接 trace、如何读取错误对象，以及如何获取 memory 的诊断快照。

## 诊断从哪里来

| 来源 | 用途 |
| --- | --- |
| `xllm_version` | 确认运行时版本。 |
| log callback | 记录本地运行日志。 |
| trace callback | 观察请求、响应、流式事件、session compact、tool loop。 |
| `xllm_error` | 获取一次失败的具体原因、HTTP 状态和 provider 错误。 |
| `xllm_memory_get_diagnostics` | 获取 memory 的 scheme、profile、SQLite、embedder、record/chunk 状态。 |

诊断默认是本地、按需启用的。xllm 不会自动把这些信息发送到外部服务。

## API：版本

### xllm_version

**功能**：返回 xllm 版本字符串。

**原型**：

```c
XLLM_API const char *xllm_version(void);
```

**返回值**：版本字符串，调用方不要释放。

**相关常量**：

```c
#define XLLM_VERSION_MAJOR 0
#define XLLM_VERSION_MINOR 1
#define XLLM_VERSION_PATCH 0
```

**范例代码**：

```c
printf("xllm version: %s\n", xllm_version());
```

## API：日志与 trace 名称

### xllm_log_event_name

**功能**：把日志事件枚举转成稳定名称。

**原型**：

```c
XLLM_API const char *xllm_log_event_name(xllm_log_event eEvent);
```

**返回值**：事件名称字符串，调用方不要释放。

### xllm_log_level_name

**功能**：把日志级别转成稳定名称。

**原型**：

```c
XLLM_API const char *xllm_log_level_name(xllm_log_level eLevel);
```

### xllm_trace_kind_name

**功能**：把 trace 类型转成稳定名称。

**原型**：

```c
XLLM_API const char *xllm_trace_kind_name(xllm_trace_kind eKind);
```

## API：设置回调

### xllm_runtime_set_log_callback

**功能**：设置 runtime 的日志回调。

**原型**：

```c
XLLM_API int xllm_runtime_set_log_callback(
    xllm_runtime *pRuntime,
    xllm_log_callback pfnLog,
    void *pLogCtx
);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pRuntime` | runtime 对象。 |
| `pfnLog` | 日志回调。传 `NULL` 可关闭。 |
| `pLogCtx` | 传给回调的用户上下文。 |

**返回值**：成功返回 `XRT_NET_OK`，失败返回 `XRT_NET_ERROR`。

**回调原型**：

```c
typedef void (*xllm_log_callback)(
    void *pCtx,
    xllm_log_level eLevel,
    const char *sComponent,
    const char *sMessage
);
```

### xllm_runtime_set_trace_callback

**功能**：设置 runtime 的 trace 回调。

**原型**：

```c
XLLM_API int xllm_runtime_set_trace_callback(
    xllm_runtime *pRuntime,
    xllm_trace_callback pfnTrace,
    void *pTraceCtx
);
```

**回调原型**：

```c
typedef void (*xllm_trace_callback)(
    void *pCtx,
    xllm_trace_kind eKind,
    const xvalue *pPayload
);
```

**trace 类型**：

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

**补充说明**：trace payload 是结构化 `xvalue`。宿主可以把它转成日志行、IDE 面板、调试记录或测试报告。

## API：错误对象

### xllm_error

**功能**：保存一次 API 调用失败的诊断信息。

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

| 字段 | 含义 |
| --- | --- |
| `eCode` | xllm 归一化错误码。 |
| `iStatus` | 底层状态码，通常和 `XRT_NET_OK/ERROR` 相关。 |
| `iHttpStatus` | 上游 HTTP 状态码。未到达 HTTP 层时可能为 `0`。 |
| `sMessage` | 适合展示或日志记录的错误消息。 |
| `sRequestId` | 上游 request id，如果 provider 提供。 |
| `sProviderCode` | provider 原始错误码。 |
| `sProviderMessage` | provider 原始错误消息。 |
| `iMessageIndex` | 出错消息下标，适合定位输入内容问题。 |
| `iPartIndex` | 出错 content part 下标，适合定位多模态输入问题。 |
| `uRequiredCapability` | 失败时需要但当前模型不支持的能力。 |
| `sSelectedModel` | 实际选择或检查到的模型名称。 |
| `sMimeType` | 出错内容的 MIME 类型。 |
| `tVendorExtra` | 扩展字段。 |

### xllm_error_init / reset / free

**功能**：初始化、重置、释放错误对象。

**原型**：

```c
XLLM_API void xllm_error_init(xllm_error *pError);
XLLM_API void xllm_error_reset(xllm_error *pError);
XLLM_API void xllm_error_free(xllm_error *pError);
```

**补充说明**：`reset` 和 `free` 都释放内部字符串；区别主要是命名语义。你可以在栈上使用 `init` + `reset`，在堆上对象销毁前使用 `free`。

## API：memory diagnostics

### xllm_memory_diagnostics

**功能**：保存 memory 的诊断快照。

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

**功能**：初始化 memory diagnostics 结果。

```c
XLLM_API void xllm_memory_diagnostics_init(
    xllm_memory_diagnostics *pDiagnostics
);
```

### xllm_memory_get_diagnostics

**功能**：获取 memory 当前诊断快照。

```c
XLLM_API int xllm_memory_get_diagnostics(
    const xllm_memory *pMemory,
    xllm_memory_diagnostics *pDiagnostics,
    xllm_error *pError
);
```

**返回值**：成功返回 `XRT_NET_OK`，失败返回 `XRT_NET_ERROR`。

**范例代码**：

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

## 范例：接入 trace 回调

```c
static void on_trace(void *ctx, xllm_trace_kind kind, const xvalue *payload)
{
    (void)ctx;
    printf("trace kind=%s payload=%p\n", xllm_trace_kind_name(kind), (const void *)payload);
}

xllm_runtime_set_trace_callback(runtime, on_trace, NULL);
```

## 排查建议

- 调用失败：先看 `xllm_error.eCode`、`sMessage`、`iHttpStatus`、`sProviderCode`。
- 检索没有命中：先看 `xllm_memory_search_debug`，再看 `xllm_memory_get_diagnostics`。
- provider 行为异常：打开 trace，看 `REQUEST` 和 `RESPONSE` payload。
- session 自动压缩异常：看 `XLLM_TRACE_COMPACT`。
- 工具循环异常：看 `XLLM_TRACE_TOOL_LOOP`。

## 相关文档

- [Core API](api-core.md)
- [Request/Response API](api-request-response.md)
- [Memory Search API](api-memory-search.md)
- [返回 API 索引](README.md)
