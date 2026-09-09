# AI IDE / xwork Agent Loop 集成

本案例展示 AI IDE 宿主怎样组合 session、memory、tool execution、权限和诊断，形成一个可控的 agent loop。

[返回范例解析](README.md) | [Tool Loop Agent](tool-loop-agent.md) | [Workspace RAG](workspace-rag.md)

## 问题

AI IDE 里的模型不能只“聊天”。它通常需要：

- 理解当前工作区。
- 检索相关代码和文档。
- 调用宿主工具检查状态、读取文件或运行测试。
- 遵守用户权限和审批规则。
- 在失败时输出可诊断的日志和 trace。

这个案例把前面几个能力组合起来，形成一个面向 AI IDE 的最小集成蓝图。

相关示例：

```text
examples/agent_loop/agent_loop.c
examples/ai_ide_memory/ai_ide_memory.c
```

## 架构

```text
AI IDE UI
  -> 当前用户 turn
  -> workspace memory search/apply
  -> conversation memory search/apply
  -> xllm_session_chat_ex
     -> model tool call
     -> permission gate
     -> xwork-like executor
     -> tool result
     -> final answer
  -> diagnostics/log/trace
  -> optional memory write-back
```

这不是“让模型控制 IDE”。真正控制 IDE 的是宿主程序。模型只提出 tool call，宿主负责校验、授权和执行。

## 组件分工

| 组件 | 职责 |
| --- | --- |
| `xllm_session` | 保存当前聊天短期历史 |
| `xllm_memory` | 保存工作区索引和长期对话记忆 |
| `xllm_tool_def` | 向模型声明 IDE 能力 |
| `xllm_tool_executor` | 宿主执行工具 |
| permission gate | 判断工具调用是否需要用户确认 |
| diagnostics | 记录错误、trace、tool loop 状态 |

## 步骤 1：索引工作区

AI IDE 启动或打开 workspace 后，先索引可公开给模型的文本文件：

```c
xllm_memory_ingest_workspace_options tWorkspace;

xllm_memory_ingest_workspace_options_init(&tWorkspace);
tWorkspace.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
tWorkspace.sPath = sWorkspaceRoot;
tWorkspace.bSkipHidden = true;
tWorkspace.sRecordIdPrefix = "workspace";
tWorkspace.sSourceUriPrefix = "workspace://";
tWorkspace.uMaxFileBytes = 4096u;
tWorkspace.uChunkChars = 512u;

xllm_memory_ingest_workspace(pMemory, &tWorkspace, &tWorkspaceResult, &tError);
```

示例 `examples/ai_ide_memory/ai_ide_memory.c` 会写入一个模拟 workspace，并验证 `.env` 没有进入上下文。

## 步骤 2：按用户问题注入工作区上下文

```c
xllm_memory_search_options tSearch;
xllm_memory_context_options tContext;

xllm_memory_search_options_init(&tSearch);
xllm_memory_context_options_init(&tContext);

tSearch.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
tSearch.sQuery = "How should the AI IDE inject workspace memory before provider execution?";
tSearch.uMaxHits = 2u;
tSearch.uMaxCharsPerHit = 512u;

tContext.sLabel = "AI IDE workspace context:";
tContext.eKindOverride = XLLM_CONTEXT_MEMORY;
tContext.uMaxHits = 2u;
tContext.uMaxCharsPerHit = 256u;

xllm_memory_search_and_apply_to_request(
    pMemory,
    &tSearch,
    &tRequest,
    &tContext,
    &tError
);
```

注入文本应包含 source URI、chunk ID 和 byte range。这样 UI 可以展示“回答参考了哪个文件片段”。

## 步骤 3：声明 IDE 工具

工具声明应尽量小而明确。例如：

```c
xllm_tool_def tTool;
memset(&tTool, 0, sizeof(tTool));

tTool.sToolId = "xwork.workspace.inspect";
tTool.sWireName = "inspect_workspace";
tTool.sDescription = "Inspect workspace state using the host xwork-like executor.";

xllm_turn_add_tool(&tTurn, &tTool);
```

常见 IDE 工具可以包括：

| 工具 | 风险 |
| --- | --- |
| `workspace.inspect` | 低，读取状态 |
| `file.read` | 中，可能读取敏感文件 |
| `file.patch` | 高，修改工作区 |
| `test.run` | 中到高，执行命令 |
| `git.diff` | 低到中，读取变更 |

高风险工具应默认需要用户确认。

## 步骤 4：安装 Executor

```c
xllm_tool_executor tExecutor;

memset(&tExecutor, 0, sizeof(tExecutor));
tExecutor.pCtx = &tState;
tExecutor.pfnExecute = xwork_like_execute;

xllm_session_set_tool_executor(pSession, &tExecutor);
```

Executor 里不要直接信任模型参数。推荐执行顺序：

1. 解析 `sArgumentsJson`。
2. 校验 tool ID 和参数 schema。
3. 检查权限和工作区边界。
4. 如有风险，请求用户确认。
5. 执行工具。
6. 返回短结果。
7. 记录审计日志。

## 步骤 5：执行 Session Agent Loop

```c
xllm_session_chat_ex(
    pSession,
    &tTurn,
    &tCallOptions,
    &pResponse,
    &tError
);
```

如果模型返回 tool call，xllm 会调用 executor，并把 tool result 放入后续请求。最终响应通过 `xllm_response_get_text` 读取。

AI IDE UI 通常还会展示：

- 模型准备调用哪个工具。
- 工具参数。
- 用户是否批准。
- 工具执行结果摘要。
- 最终回答。

## 步骤 6：写回有价值的长期记忆

一次 agent loop 结束后，可以把有长期价值的信息写回 memory：

```c
xllm_memory_ingest_turn_response_options tWrite;

xllm_memory_ingest_turn_response_options_init(&tWrite);
tWrite.eExtractionPolicy = XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY;
tWrite.pTurn = &tTurn;
tWrite.pResponse = pResponse;
tWrite.sConversationId = "agent-loop-demo";
tWrite.sTurnId = "turn-001";
tWrite.sRecordId = "agent-loop-summary";
tWrite.sSummaryText = "summary: Agent inspected workspace before editing.";
tWrite.bReplaceExisting = true;

xllm_memory_ingest_turn_response(pMemory, &tWrite, &tError);
```

不要把所有工具日志都写进长期 memory。长期 memory 应该是精炼、可追溯、对未来有用的信息。

## 诊断接入

AI IDE 场景建议同时接入 log 和 trace：

```c
xllm_runtime_set_log_callback(pRuntime, on_log, pLogCtx);
xllm_runtime_set_trace_callback(pRuntime, on_trace, pTraceCtx);
```

重点观察：

| Trace | 用途 |
| --- | --- |
| `XLLM_TRACE_REQUEST` | 查看最终注入给模型的上下文 |
| `XLLM_TRACE_TOOL_LOOP` | 查看工具循环每轮状态 |
| `XLLM_TRACE_COMPACT` | 查看 session 历史压缩 |
| `XLLM_TRACE_RESPONSE` | 查看模型响应解析 |

日志和 trace 要脱敏。不要把 API key、`.env`、证书、私有文件全文写入诊断包。

## 构建示例

Agent loop 示例：

```bat
cmd /c .\examples\agent_loop\build.bat
```

AI IDE memory 示例：

```bat
cmd /c .\examples\ai_ide_memory\build.bat
```

脚本会从仓库根目录使用 gcc 编译：

```bat
gcc -std=c11 -Wall -Wextra -I. -Ilib -Ilib\sqlite ^
    -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
    examples\agent_loop\agent_loop.c ^
    lib\sqlite\sqlite3.c ^
    -o build\agent_loop.exe ^
    -lws2_32 -liphlpapi -lshell32 -lcrypt32
```

## 关键 API

| API | 作用 |
| --- | --- |
| `xllm_memory_ingest_workspace` | 索引工作区 |
| `xllm_memory_search_and_apply_to_request` | 注入工作区上下文 |
| `xllm_session_create` | 创建短期对话 |
| `xllm_session_set_tool_executor` | 安装工具执行器 |
| `xllm_turn_add_tool` | 提供本轮工具 |
| `xllm_session_chat_ex` | 执行 agent loop |
| `xllm_memory_ingest_turn_response` | 写回长期记忆 |
| `xllm_runtime_set_trace_callback` | 接入诊断 |

## 扩展点

真实 AI IDE 可以继续扩展：

- 按语言服务结果增强 RAG。
- 对 file patch 工具增加 diff 预览和用户审批。
- 对 test.run 工具增加命令白名单。
- 对 memory 写入增加用户确认。
- 对 workspace sync 使用 watcher worker。
- 对上下文注入做 citation UI。

## 常见问题

不要让模型绕过宿主权限。所有工具执行都必须经过宿主校验。

不要把隐藏文件和密钥注入模型。工作区索引必须有敏感默认过滤。

不要把工具结果无限塞回上下文。工具结果应短、结构化、和当前任务相关。

不要只保存 final answer。调试 agent 时，tool call、参数、执行状态和 trace 同样重要。
