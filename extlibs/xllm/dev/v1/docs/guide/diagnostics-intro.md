# Diagnostics 入门

Diagnostics 帮你回答三个问题：请求为什么失败、模型实际收到了什么、memory 或工具循环内部发生了什么。

[返回教程入口](README.md) | [Diagnostics API](../api/api-diagnostics.md) | [Release Gate 入门](release-gate-intro.md)

## 你会学到什么

本文会带你建立一套最小排查流程：

- 打印 xllm 版本。
- 给 runtime 接入 log callback。
- 给 runtime 接入 trace callback。
- 正确读取 `xllm_error`。
- 查看 memory diagnostics。
- 用 release gate 报告定位发布包问题。

## 先打印版本

排查问题时，先记录版本。它能帮你确认用户运行的是哪个发布包。

```c
printf("xllm version: %s\n", xllm_version());
```

发布包排查还应同时记录 `VERSION` 文件、release bundle 名称和校验和。

## 接入日志

Log callback 适合记录运行时事件，例如请求开始、provider 响应、tool loop、memory ingest/search。

```c
static void on_log(
    void *pCtx,
    xllm_log_level eLevel,
    const char *sComponent,
    const char *sMessage
)
{
    (void)pCtx;
    fprintf(stderr, "[%s] %s: %s\n",
            xllm_log_level_name(eLevel),
            sComponent ? sComponent : "xllm",
            sMessage ? sMessage : "");
}

xllm_runtime_set_log_callback(pRuntime, on_log, NULL);
```

日志适合长期打开，但要注意脱敏。API key、用户文件内容和 provider 原始请求体不应进入普通日志。

## 接入 Trace

Trace 比日志更结构化，适合调试请求、响应、流式事件、compact 和 tool loop。

```c
static void on_trace(void *pCtx, xllm_trace_kind eKind, const xvalue *pPayload)
{
    (void)pCtx;
    printf("trace kind=%s payload=%p\n",
           xllm_trace_kind_name(eKind),
           (const void *)pPayload);
}

xllm_runtime_set_trace_callback(pRuntime, on_trace, NULL);
```

常见 trace 类型：

| 类型 | 用途 |
| --- | --- |
| `XLLM_TRACE_REQUEST` | 查看请求构造和 provider 适配前后的结构 |
| `XLLM_TRACE_RESPONSE` | 查看 provider 响应解析结果 |
| `XLLM_TRACE_STREAM` | 查看流式事件 |
| `XLLM_TRACE_COMPACT` | 查看 session compact 触发和结果 |
| `XLLM_TRACE_TOOL_LOOP` | 查看工具循环的每轮状态 |

开发调试时可以打开更详细 trace；生产环境应避免记录敏感正文，或使用严格脱敏策略。

## 读取错误对象

带 `_ex` 后缀的 API 通常允许你传入 `xllm_error`。失败时先看这几个字段：

```c
xllm_error tError;
xllm_error_init(&tError);

if ( xllm_send_ex(pLlm, &tTurn, &tOptions, &pResponse, &tError) != XRT_NET_OK ) {
    fprintf(stderr, "code=%d status=%d http=%d message=%s\n",
            (int)tError.eCode,
            (int)tError.iStatus,
            (int)tError.iHttpStatus,
            tError.sMessage ? tError.sMessage : "(null)");
}

xllm_error_free(&tError);
```

字段解读：

| 字段 | 先看场景 |
| --- | --- |
| `eCode` | xllm 归一化错误，例如鉴权、超时、能力不支持 |
| `iHttpStatus` | provider 返回 HTTP 错误时最有用 |
| `sProviderCode` / `sProviderMessage` | provider 原始错误 |
| `sRequestId` | 找 provider 支持或查服务端日志时使用 |
| `iMessageIndex` / `iPartIndex` | 定位哪条输入或哪个多模态 part 出错 |
| `uRequiredCapability` | 模型缺少某项能力时使用 |
| `sSelectedModel` | 确认最终选择的模型 |

如果你在循环里复用同一个 `xllm_error`，每次处理完失败后调用 `xllm_error_reset`，最后调用 `xllm_error_free`。

## 常见排查路径

### 请求失败

1. 查看 API 返回值是否为 `XRT_NET_OK`。
2. 打印 `xllm_error.eCode` 和 `sMessage`。
3. 如果到达 provider，查看 `iHttpStatus`、`sProviderCode`、`sRequestId`。
4. 如果是能力问题，检查 profile 的 `tCaps.uFlags`。
5. 打开 request/response trace。

### 流式输出没有增量

1. 确认 profile 声明了 `XLLM_CAP_STREAM`。
2. 确认 `xllm_call_options.eStreamMode` 不是 `XLLM_STREAM_OFF`。
3. 确认 `pfnOnEvent` 已设置，并且回调返回 `true`。
4. 查看 provider 是否真的支持当前模型流式输出。

### 工具没有被执行

1. 确认 turn 里添加了 `xllm_tool_def`。
2. 确认 profile 声明了 `XLLM_CAP_TOOL_CALL_OUT` 和 `XLLM_CAP_TOOL_RESULT_IN`。
3. 如果使用自动 loop，确认设置了 `xllm_set_tool_executor` 或 `xllm_session_set_tool_executor`。
4. 查看 `XLLM_TRACE_TOOL_LOOP`。
5. 检查 tool executor 返回值和 tool result 内容。

### Memory 搜不到

1. 确认写入时使用的 scope 和搜索时一致。
2. 打印 record/chunk 计数。
3. 使用 `xllm_memory_search_debug` 查看候选。
4. 查看 `xllm_memory_get_diagnostics`。
5. 检查 chunk 大小、source URI、metadata 过滤条件和 min score。

## Memory Diagnostics

Memory diagnostics 能告诉你当前 memory backend 的状态：

```c
xllm_memory_diagnostics tDiag;
xllm_memory_diagnostics_init(&tDiag);

if ( xllm_memory_get_diagnostics(pMemory, &tDiag, &tError) == XRT_NET_OK ) {
    printf("scheme=%d records=%zu chunks=%zu sqlite_open=%d\n",
           (int)tDiag.eScheme,
           tDiag.iRecordCount,
           tDiag.iChunkCount,
           tDiag.bSqliteOpen ? 1 : 0);
}
```

常看字段：

| 字段 | 用途 |
| --- | --- |
| `eScheme` | 当前检索方案 |
| `sNamespace` | 当前命名空间 |
| `sSqlitePath` | SQLite 存储位置 |
| `bSqliteOpen` | SQLite 是否打开 |
| `bHybridSearchEnabled` | 是否启用混合检索 |
| `bEmbedderConfigured` | 是否配置 embedding |
| `iRecordCount` / `iChunkCount` | 记录和 chunk 数量 |

## Release Gate 报告

如果问题发生在发布包或下游集成阶段，优先运行：

```bat
cmd /c .\build.bat verify-version
cmd /c .\build.bat release-bundle -VerifyCompile
cmd /c .\build.bat verify-artifact -RootDir build\release_bundle -VerifyCompile
cmd /c .\build.bat downstream-smoke
```

发布前总检查：

```bat
cmd /c .\build.bat release-gate -RunDownstream
```

如果仓库内示例能编译，但发布包中的下游 smoke 失败，通常说明 bundle 缺文件、include 路径不完整、依赖没有打包或校验和不一致。

## 脱敏建议

诊断信息可能包含用户输入、文件内容、provider 错误和请求 ID。建议：

- 默认记录错误码、状态码、request id，不记录完整正文。
- 调试模式下才记录 request/response trace。
- 对 API key、Authorization header、Cookie、密钥文件路径做脱敏。
- 用户可导出的诊断包中不包含机密文件内容。

## 下一步

- 查看字段级说明：读 [Diagnostics API](../api/api-diagnostics.md)。
- 想排查发布包：读 [Release Gate 入门](release-gate-intro.md)。
- 想排查 memory：读 [Memory RAG 入门](memory-rag-intro.md) 和 [工作区索引入门](workspace-index-intro.md)。
