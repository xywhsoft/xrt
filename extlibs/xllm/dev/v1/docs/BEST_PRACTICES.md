# xllm 最佳实践

本文整理 xllm 集成时最容易影响稳定性、可维护性和安全性的做法。

[返回文档中心](README.md)

## Profile 管理

把 profile 当作“模型连接配置”，不要把它散落在业务代码里。

建议：

- 每个 profile 使用稳定 `sId`，例如 `fast-text`、`tool-agent`、`vision`。
- 明确填写 `sProvider`、`sAdapter`、`sBaseUrl` 和认证方式。
- 给每个模型准确声明 `tCaps.uFlags`。
- 不要把 API key 写死在源码或文档里，使用环境变量或安全配置来源。
- provider 能力变化后，重新跑 provider probe 或 smoke。

继续阅读：[Provider 与 Profile 入门](guide/provider-profile-intro.md)

## 请求构造

简单文本请求用 `xllm_turn`，复杂控制用 `xllm_request`。

建议：

- 每轮新输入创建新的 `xllm_turn`，发送后调用 `xllm_turn_reset`。
- 需要底层控制时再使用 `xllm_request`。
- 发送前对复杂请求调用 `xllm_validate_request`。
- JSON/schema 输出使用 response format，不要只依赖 prompt。
- 多模态请求要明确 MIME 类型。

继续阅读：[Request / Response 入门](guide/request-response-intro.md)

## 错误处理

优先使用带 `_ex` 的 API，传入 `xllm_error`。

建议：

- 失败时记录 `eCode`、`iStatus`、`iHttpStatus`、`sRequestId`。
- 能力错误时查看 `uRequiredCapability` 和 `sSelectedModel`。
- 循环复用错误对象时，每次处理后调用 `xllm_error_reset`。
- 最后调用 `xllm_error_free`。
- 不要把 provider 原始错误直接展示给最终用户，先做脱敏和整理。

继续阅读：[Diagnostics 入门](guide/diagnostics-intro.md)

## Session 使用

Session 保存短期历史，不保存长期记忆。

建议：

- 聊天窗口、IDE 面板和命令行助手使用 `xllm_session`。
- 单轮任务使用 `xllm_send_ex` 或 `xllm_chat_ex`。
- 设置 `bEnableAutoCompact` 和 `uKeepRecentTurns`。
- 长对话使用 compact，旧历史可摘要或截断。
- 要跨进程恢复时使用 session state 导出导入。

不要把 session 当作用户长期画像或知识库。

继续阅读：[Session 入门](guide/session-intro.md)

## Tool Loop

模型只提出 tool call，宿主负责真正执行。

建议：

- 每个工具有稳定 `sToolId`。
- `sWireName` 简短清晰，适合模型理解。
- 生产环境补充 `tInputSchema`。
- executor 必须校验 `sArgumentsJson`。
- 文件、命令、网络、数据库类工具应有权限控制。
- 高风险工具执行前请求用户确认。
- 工具结果要短而明确。
- 记录 tool call、参数、执行状态和结果摘要。

继续阅读：[Tool Loop 入门](guide/tool-loop-intro.md)

## Memory 写入

长期 memory 应少而准。

建议：

- 项目知识写入 `XLLM_MEMORY_SCOPE_KNOWLEDGE`。
- 用户偏好、事实、任务写入 `XLLM_MEMORY_SCOPE_MEMORY`。
- 给每条记录设置稳定 `sRecordId`。
- 设置 `sSourceUri`，便于追溯和删除。
- 更新同一条记录时使用 `bReplaceExisting = true`。
- 偏好和个人信息写入前请求用户确认。
- 不要把每一句对话都写成长期 memory。

继续阅读：[Conversation Memory 入门](guide/conversation-memory-intro.md)

## Workspace RAG

工作区索引必须先过滤，再写入。

建议：

- 开启隐藏文件跳过和 `.gitignore`。
- 排除 `.git`、`build`、依赖目录和缓存目录。
- 排除密钥、证书、`.env`、大型二进制文件。
- 设置 `uMaxFileBytes`。
- 使用稳定 `sRecordIdPrefix` 和 `sSourceUriPrefix`。
- 检索注入时限制 `uMaxHits`、`uMaxCharsPerHit`、`uMaxTotalChars`。
- UI 中显示 source URI、chunk ID 和 byte range。

继续阅读：[工作区索引入门](guide/workspace-index-intro.md)

## Context Packing

上下文不是越多越好。

建议：

- 当前用户输入优先级最高。
- 系统提示词保持短而稳定。
- Memory 检索结果设置预算和去重。
- 工具结果只保留当前任务需要的部分。
- 旧历史使用 session compact。
- 检索上下文要有标签，说明它是参考材料。

继续阅读：[Context Packing 入门](guide/context-packing-intro.md)

## 诊断与脱敏

诊断要能定位问题，也不能泄露数据。

建议：

- 开发环境打开 trace，生产环境默认关闭或严格脱敏。
- 普通日志记录错误码、request id、状态码，不记录完整正文。
- 诊断包不要包含 API key、Authorization header、Cookie、证书和 `.env`。
- Tool loop 调试时记录工具名、参数摘要、执行状态。
- Memory 调试时记录 record/chunk 数量和 source URI。

## 发布与消费

发布前要像下游使用者一样验证。

建议：

- 运行 `cmd /c .\build.bat verify-version`。
- 运行 `cmd /c .\build.bat singlehead`。
- 运行 `cmd /c .\build.bat release-bundle -VerifyCompile`。
- 运行 `cmd /c .\build.bat verify-artifact -RootDir build\release_bundle -VerifyCompile`。
- 运行 `cmd /c .\build.bat downstream-smoke`。
- 真正发布前运行 `cmd /c .\build.bat release-gate -RunDownstream`。

继续阅读：[Release Gate 入门](guide/release-gate-intro.md)

## 最小安全清单

- API key 不进源码。
- 工具执行前校验参数。
- 文件工具限制在 workspace 内。
- 高风险工具需要用户确认。
- Memory 写入有来源 URI。
- 工作区索引过滤敏感文件。
- 诊断日志脱敏。
- 发布包 checksum 通过后再消费。
