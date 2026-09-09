# Diagnostics Introduction

Diagnostics help you answer three questions: why a request failed, what the model actually received, and what happened inside memory or a tool loop.

[Back to Tutorials](README.en.md) | [Diagnostics API](../api/api-diagnostics.en.md) | [Release Gate Introduction](release-gate-intro.en.md)

## What You Will Learn

This guide helps you build a minimal troubleshooting flow:

- Print the xllm version.
- Attach a log callback to the runtime.
- Attach a trace callback to the runtime.
- Read `xllm_error` correctly.
- Inspect memory diagnostics.
- Use release gate reports to locate release bundle problems.

## Print the Version First

When troubleshooting, record the version first. It helps you confirm which release package the user is running.

```c
printf("xllm version: %s\n", xllm_version());
```

For release package troubleshooting, also record the `VERSION` file, release bundle name, and checksums.

## Attaching Logs

The log callback is suitable for runtime events, such as request start, provider response, tool loop, and memory ingest/search.

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

Logs can stay enabled for long periods, but sanitize them. API keys, user file contents, and raw provider request bodies should not enter ordinary logs.

## Attaching Trace

Trace is more structured than logs and is useful for debugging requests, responses, streaming events, compacting, and tool loops.

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

Common trace kinds:

| Kind | Use |
| --- | --- |
| `XLLM_TRACE_REQUEST` | Inspect request construction and provider-adapter structures |
| `XLLM_TRACE_RESPONSE` | Inspect parsed provider responses |
| `XLLM_TRACE_STREAM` | Inspect streaming events |
| `XLLM_TRACE_COMPACT` | Inspect session compact triggers and results |
| `XLLM_TRACE_TOOL_LOOP` | Inspect each tool loop round |

You can enable detailed trace during development. In production, avoid recording sensitive bodies, or use a strict redaction strategy.

## Reading Error Objects

APIs with an `_ex` suffix usually let you pass `xllm_error`. On failure, inspect these fields first:

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

How to read the fields:

| Field | Look Here For |
| --- | --- |
| `eCode` | Normalized xllm error, such as auth, timeout, unsupported capability |
| `iHttpStatus` | Provider HTTP errors |
| `sProviderCode` / `sProviderMessage` | Raw provider error |
| `sRequestId` | Provider support or server-side log lookup |
| `iMessageIndex` / `iPartIndex` | Which input message or multimodal part failed |
| `uRequiredCapability` | Which model capability is missing |
| `sSelectedModel` | Confirming the final selected model |

If you reuse the same `xllm_error` in a loop, call `xllm_error_reset` after each handled failure, then call `xllm_error_free` at the end.

## Common Troubleshooting Paths

### Request Failed

1. Check whether the API return value is `XRT_NET_OK`.
2. Print `xllm_error.eCode` and `sMessage`.
3. If the request reached the provider, inspect `iHttpStatus`, `sProviderCode`, and `sRequestId`.
4. If it is a capability issue, check `tCaps.uFlags` in the profile.
5. Enable request/response trace.

### Streaming Has No Delta

1. Confirm that the profile declares `XLLM_CAP_STREAM`.
2. Confirm that `xllm_call_options.eStreamMode` is not `XLLM_STREAM_OFF`.
3. Confirm that `pfnOnEvent` is set and the callback returns `true`.
4. Check whether the provider really supports streaming for the selected model.

### Tool Was Not Executed

1. Confirm that the turn contains `xllm_tool_def`.
2. Confirm that the profile declares `XLLM_CAP_TOOL_CALL_OUT` and `XLLM_CAP_TOOL_RESULT_IN`.
3. If you use the automatic loop, confirm that `xllm_set_tool_executor` or `xllm_session_set_tool_executor` was called.
4. Inspect `XLLM_TRACE_TOOL_LOOP`.
5. Check the tool executor return value and tool result content.

### Memory Search Finds Nothing

1. Confirm that ingest and search use the same scope.
2. Print record/chunk counts.
3. Use `xllm_memory_search_debug` to inspect candidates.
4. Inspect `xllm_memory_get_diagnostics`.
5. Check chunk size, source URI, metadata filters, and min score.

## Memory Diagnostics

Memory diagnostics show the current memory backend state:

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

Common fields:

| Field | Use |
| --- | --- |
| `eScheme` | Current retrieval scheme |
| `sNamespace` | Current namespace |
| `sSqlitePath` | SQLite storage path |
| `bSqliteOpen` | Whether SQLite is open |
| `bHybridSearchEnabled` | Whether hybrid search is enabled |
| `bEmbedderConfigured` | Whether embedding is configured |
| `iRecordCount` / `iChunkCount` | Record and chunk counts |

## Release Gate Reports

If a problem appears in a release package or downstream integration, run these first:

```bat
cmd /c .\build.bat verify-version
cmd /c .\build.bat release-bundle -VerifyCompile
cmd /c .\build.bat verify-artifact -RootDir build\release_bundle -VerifyCompile
cmd /c .\build.bat downstream-smoke
```

Final pre-release check:

```bat
cmd /c .\build.bat release-gate -RunDownstream
```

If examples compile inside the repository, but downstream smoke fails inside the release package, the usual causes are missing bundle files, incomplete include paths, missing dependencies, or checksum mismatch.

## Redaction Advice

Diagnostic information may contain user input, file contents, provider errors, and request IDs. Recommended practice:

- Record error codes, status codes, and request IDs by default; do not record full bodies.
- Record request/response trace only in debug mode.
- Redact API keys, Authorization headers, cookies, key file paths, and similar secrets.
- Do not include secret file contents in diagnostic packages that users can export.

## Next Steps

- For field-level details, read [Diagnostics API](../api/api-diagnostics.en.md).
- To troubleshoot release packages, read [Release Gate Introduction](release-gate-intro.en.md).
- To troubleshoot memory, read [Memory RAG Introduction](memory-rag-intro.en.md) and [Workspace Index Introduction](workspace-index-intro.en.md).
