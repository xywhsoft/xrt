# xllm 迁移说明

本文说明怎样从直接 provider 调用迁移到 xllm，从无 session 迁移到 session，从临时 RAG 迁移到 xllm memory。

[返回文档中心](README.md)

## 迁移原则

不要一次迁移所有能力。建议按层推进：

1. 先迁移最小聊天调用。
2. 再把 provider 配置整理成 profile。
3. 再引入 session。
4. 再引入 tool loop。
5. 再引入 memory/RAG。
6. 最后接入 diagnostics 和 release gate。

每一层都应该有一个能运行的最小验证程序。

## 从直接 Provider 调用迁移

原来你可能直接构造 HTTP 请求：

```text
业务代码 -> provider SDK/HTTP -> provider response
```

迁移后：

```text
业务代码 -> xllm request/turn -> adapter/profile -> provider response -> xllm_response
```

第一步只迁移单轮文本：

- 创建 `xllm_runtime`。
- 注册对应 adapter。
- 创建 `xllm_profile`。
- 用 `xllm_turn_add_user_text` 构造输入。
- 调用 `xllm_send_ex`。
- 用 `xllm_response_get_text` 读取输出。

参考：[最小聊天调用](case/minimal-chat.md)

## 从硬编码模型迁移到 Profile

如果你的代码里到处都是 base URL、API key、model ID，先集中成 profile。

建议把配置抽象为：

| 原配置 | xllm 字段 |
| --- | --- |
| provider 名称 | `xllm_profile.sProvider` |
| adapter 类型 | `xllm_profile.sAdapter` |
| base URL | `xllm_profile.sBaseUrl` |
| API key | `xllm_profile.tAuth` |
| model | `tModels.tText.sModelId` 或 `tModels.tMultimodal.sModelId` |
| 能力 | `tCaps.uFlags` |

迁移时不要忽略能力标记。很多运行时错误都来自 profile 没有声明真实能力。

## 从无历史调用迁移到 Session

如果你原来每次都把历史手工拼进 prompt，建议迁移到 `xllm_session`。

迁移步骤：

1. 创建 `xllm_session_options`。
2. 设置 `sProfileId` 和 `sSystemPrompt`。
3. 用 `xllm_session_create` 创建 session。
4. 每轮只把新增用户输入放入 `xllm_turn`。
5. 调用 `xllm_session_chat_ex`。
6. 设置 compact 策略。

参考：[Session Chat 案例](case/session-chat.md)

## 从手写工具协议迁移到 Tool Loop

如果你原来在 prompt 里要求模型输出某种 JSON 来表示工具调用，可以迁移到 xllm tool API。

迁移步骤：

1. 把每个工具声明为 `xllm_tool_def`。
2. 给工具补充 `sToolId`、`sWireName`、`sDescription` 和 schema。
3. profile 声明 `XLLM_CAP_TOOL_CALL_OUT` 和 `XLLM_CAP_TOOL_RESULT_IN`。
4. 实现 `xllm_tool_executor`。
5. 安装 executor。
6. 用 trace 观察 tool loop。

参考：[Tool Loop Agent](case/tool-loop-agent.md)

## 从临时 RAG 迁移到 xllm Memory

如果你原来用自己的数组、临时字符串或外部向量库拼 RAG 上下文，可以逐步迁移到 `xllm_memory`。

迁移步骤：

1. 先用 `XLLM_MEMORY_SCHEME_BUILTIN_SPARSE` 跑通最小 ingest/search。
2. 把文档写入 `XLLM_MEMORY_SCOPE_KNOWLEDGE`。
3. 给记录设置稳定 `sRecordId` 和 `sSourceUri`。
4. 用 `xllm_memory_search` 检索。
5. 用 `xllm_memory_apply_search_to_request` 注入上下文。
6. 再引入 workspace ingest/sync。
7. 最后考虑 embedding、混合检索和 watcher。

参考：[Workspace RAG](case/workspace-rag.md)

## 从隐式记忆迁移到显式 Conversation Memory

如果你原来让模型“自己记住”用户偏好，建议改为显式写入 memory。

迁移步骤：

1. 定义什么信息可以长期保存。
2. 对用户偏好和个人信息增加确认流程。
3. 用 `xllm_memory_ingest_text` 写入摘要。
4. 后续用 typed APIs 写入 task/fact/preference。
5. 每轮请求前检索并注入。
6. 提供删除入口。

参考：[Conversation Memory 案例](case/conversation-memory.md)

## 从无诊断迁移到可观测调用

迁移到 xllm 后，应尽早接入：

- `xllm_error`
- log callback
- trace callback
- memory diagnostics

这样 provider、session、tool loop、memory 出问题时可以定位。

参考：[Diagnostics 入门](guide/diagnostics-intro.md)

## 迁移风险

| 风险 | 处理 |
| --- | --- |
| provider 行为变化 | 先用 mock/smoke 验证，再跑真实 provider probe |
| 能力声明不准确 | 建立 profile 能力矩阵 |
| 上下文变长 | 引入 context packing 和 compact |
| 工具执行风险 | 增加权限、审批和审计 |
| memory 噪声 | 限制写入、设置 scope、使用稳定来源 |
| 发布包缺文件 | 使用 downstream smoke 验证 |

## 推荐迁移检查表

1. 最小聊天跑通。
2. Profile 从配置加载，不硬编码 key。
3. 所有失败路径都能打印 `xllm_error`。
4. Session 只用于短期历史。
5. Tool executor 校验参数和权限。
6. Memory 记录有 `sRecordId` 和 `sSourceUri`。
7. RAG 注入有预算。
8. 诊断日志脱敏。
9. 发布包通过 artifact 和 downstream 验证。
