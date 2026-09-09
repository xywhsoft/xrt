# xllm 性能与发布说明

本文解释如何看待 xllm 的性能、上下文预算、memory 检索、工具循环开销和 release gate 报告。

[返回文档中心](README.md)

## 先明确性能目标

LLM 应用的性能不只有“请求多快”。你需要同时关注：

- 首 token 延迟。
- 总响应时间。
- 输入 token 数量。
- 输出 token 数量。
- 工具调用轮数。
- RAG 检索耗时。
- Memory ingest/sync 耗时。
- 发布包编译和下游验证时间。

不同场景目标不同：聊天 UI 更看重首 token；批处理更看重吞吐；AI IDE 更看重工具和 RAG 的整体闭环时间。

## 流式输出

流式输出不能减少模型总计算量，但能改善用户感知延迟。

建议：

- UI 聊天使用 `XLLM_STREAM_PREFER`。
- 必须实时输出时使用 `XLLM_STREAM_REQUIRE`。
- 不支持 stream 的模型降级到非流式。
- 回调中不要做耗时操作。
- 回调返回 `false` 会中止请求。

继续阅读：[Request / Response 入门](guide/request-response-intro.md)

## Context Packing

上下文越大，通常成本越高、延迟越高。RAG 和 session 都应有预算。

建议：

- Memory 注入设置 `uMaxHits`。
- 设置 `uMaxCharsPerHit` 和 `uMaxTotalChars`。
- 对同一 record 的多个命中做去重。
- Session 设置 `uKeepRecentTurns`。
- 长对话启用 compact。
- 工具结果只保留回答所需内容。

继续阅读：[Context Packing 入门](guide/context-packing-intro.md)

## Memory Ingest 性能

Memory ingest 的耗时来自：

- 文件扫描。
- 文件读取。
- chunking。
- embedding 或稀疏索引。
- SQLite 写入。

建议：

- 设置 `uMaxFileBytes`。
- 过滤构建目录、依赖目录和二进制文件。
- 工作区长期运行时使用 sync 或 watcher，而不是每次全量 ingest。
- 对大工作区分批处理。
- 使用 `bSkipUnchanged` 避免重复写入。

继续阅读：[工作区索引入门](guide/workspace-index-intro.md)

## Memory Search 性能

Search 的成本来自候选检索、排序、过滤和上下文渲染。

建议：

- 设置 `uMaxHits`。
- 设置 scope，避免不必要地搜索 `ANY`。
- 使用 metadata/source URI 过滤缩小范围。
- 对 UI 中的重复问题做应用层缓存。
- 使用 `xllm_memory_search_debug` 分析低质量检索，而不是盲目加大命中数。

## Tool Loop 性能

工具循环会增加模型轮次。一个 tool call 往往意味着至少多一次模型请求。

建议：

- 工具结果尽量短。
- 对慢工具设置超时。
- 可并行的宿主操作在 executor 内并行，但返回给模型时保持结果清晰。
- 限制最大工具轮数。
- 把不需要模型决策的固定检索放在模型调用前完成。

继续阅读：[Tool Loop Agent](case/tool-loop-agent.md)

## Session Compact 成本

Compact 可以降低后续上下文成本，但 compact 本身也可能有成本。

| 策略 | 成本 | 适用 |
| --- | --- | --- |
| truncate | 低 | 学习、简单聊天、可丢弃旧历史 |
| summarize | 中到高 | 长对话，需要保留旧历史含义 |
| custom | 取决于实现 | 有自己的摘要或压缩策略 |

建议先用 truncate 跑通，再为长对话引入 summarize。

## Diagnostics 开销

日志和 trace 有运行时和存储开销。

建议：

- 开发时打开详细 trace。
- 生产默认记录错误码、状态码和 request id。
- 大请求/响应正文默认不记录。
- 用户导出的诊断包必须脱敏。

## Release Gate 报告

Release gate 不是性能 benchmark，但它能证明发布包质量。

关注：

- `verify-version` 是否通过。
- `singlehead` 是否通过。
- `release-bundle -VerifyCompile` 是否通过。
- `verify-artifact` 是否通过 checksum 和 metadata 检查。
- `downstream-smoke` 是否在干净环境消费发布包。

继续阅读：[Release Gate 入门](guide/release-gate-intro.md)

## 何时需要 Benchmark

当你准备做以下事情时，再建立 benchmark：

- 比较不同 provider 或模型。
- 比较不同 memory scheme。
- 调整 chunk 大小和检索数量。
- 优化 AI IDE 首次索引时间。
- 评估 tool loop 最大轮数。
- 发布前验证性能没有明显退化。

Benchmark 应记录输入规模、模型、profile、网络条件、memory scheme、SQLite 路径、是否流式、命中数量和工具轮数。

## 常见性能问题

### 响应慢

先看：

- 输入上下文是否过大。
- 是否触发了工具循环。
- 是否启用了 summarize compact。
- provider 网络是否慢。
- 是否记录了大量 trace 正文。

### RAG 回答差

先看：

- 是否索引了太多无关文件。
- 查询是否过于宽泛。
- `uMaxHits` 是否太低或太高。
- 是否缺少 source URI 和 context label。
- 是否需要 metadata 过滤。

### 工作区索引慢

先看：

- 是否过滤了 `build/`、依赖目录和隐藏文件。
- `uMaxFileBytes` 是否过大。
- 是否每次全量 ingest。
- 是否应该使用 `sync_workspace` 或 watcher。

## 推荐默认值

这些值适合起步，实际应用应根据模型和数据调优：

| 场景 | 起步建议 |
| --- | --- |
| 普通聊天 | `XLLM_STREAM_PREFER` |
| Session | 保留最近 4 到 8 轮 |
| Workspace RAG | 3 到 6 条命中，总计 4000 到 8000 字符 |
| Conversation memory | 1 到 3 条命中，每条 512 到 1200 字符 |
| Tool loop | 最大 3 到 5 轮 |
| 工作区文件 | 设置明确 `uMaxFileBytes`，过滤构建和依赖目录 |

## 下一步

- 学上下文预算：[Context Packing 入门](guide/context-packing-intro.md)
- 学工作区索引：[工作区索引入门](guide/workspace-index-intro.md)
- 学发布验证：[发布包消费案例](case/release-bundle-consumption.md)
