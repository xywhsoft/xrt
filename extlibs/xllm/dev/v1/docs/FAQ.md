# xllm 常见问题

本文回答安装、构建、provider 配置、session、memory、工具调用、诊断和发布包消费中的常见问题。

[返回文档中心](README.md)

## 我应该先读哪些文档？

第一次学习建议按这个顺序：

1. [第一个 xllm 程序](guide/first-xllm-program.md)
2. [Provider 与 Profile 入门](guide/provider-profile-intro.md)
3. [Request / Response 入门](guide/request-response-intro.md)
4. [最小聊天调用](case/minimal-chat.md)

要做 agent 或 AI IDE，再继续读 session、tool loop、memory 和案例。

## xllm 和直接调用 provider SDK 有什么区别？

直接调用 provider SDK 时，你通常要为每个 provider 单独处理请求格式、工具协议、错误、流式事件和模型能力。

xllm 提供统一结构：

- `xllm_profile` 管 provider/model 配置。
- `xllm_request` / `xllm_turn` 管输入。
- `xllm_response` 管输出。
- `xllm_session` 管短期历史。
- `xllm_memory` 管长期记忆和 RAG。
- `xllm_error` 管诊断。

## 我只想问一次模型，需要 session 吗？

不需要。单轮请求使用 `xllm_create` + `xllm_send_ex`，或底层 `xllm_chat_ex`。

只有当你希望模型记住当前聊天里的前几轮内容时，才使用 `xllm_session`。

## Session 能不能当长期记忆？

不建议。Session 是短期历史，会受到 compact、导入导出和上下文窗口限制。

长期偏好、任务、事实、项目知识应写入 `xllm_memory`。

## Memory 和 RAG 是必须的吗？

不是。普通聊天不需要 memory。

当你需要模型引用项目文档、代码工作区、用户长期偏好或对话摘要时，再使用 memory。

## 为什么我的请求提示“能力不支持”？

通常是 profile 的 `tCaps.uFlags` 没有声明请求所需能力，或模型本身确实不支持。

排查：

1. 查看 `xllm_error.uRequiredCapability`。
2. 查看 `xllm_error.sSelectedModel`。
3. 检查 profile 的 text/multimodal model caps。
4. 如果是 provider 能力变化，重新跑 provider probe。

## 为什么没有流式输出？

检查：

- profile 是否包含 `XLLM_CAP_STREAM`。
- `xllm_call_options.eStreamMode` 是否为 `XLLM_STREAM_PREFER` 或 `XLLM_STREAM_REQUIRE`。
- 是否设置了 `pfnOnEvent`。
- 回调是否返回 `true`。
- provider 当前模型是否支持 stream。

## Tool call 返回了，但工具没有执行？

如果你没有设置 executor，普通发送会停在 `XLLM_STATUS_TOOL_CALL_REQUIRED`，这是正常行为。

要自动执行工具，需要：

- 在 turn 中添加 `xllm_tool_def`。
- profile 声明 `XLLM_CAP_TOOL_CALL_OUT` 和 `XLLM_CAP_TOOL_RESULT_IN`。
- 调用 `xllm_set_tool_executor` 或 `xllm_session_set_tool_executor`。

## 工具参数可以直接执行吗？

不可以。`sArgumentsJson` 来自模型输出，必须当作不可信输入处理。

你应该解析 JSON、校验 schema、检查权限和路径边界，再执行工具。

## Memory 搜不到刚写入的内容怎么办？

检查：

- 写入 scope 和搜索 scope 是否一致。
- `sQuery` 是否和内容相关。
- 是否设置了过高的 `tMinScore`。
- 是否设置了 metadata 过滤但写入时没有对应 metadata。
- 是否调用了 `xllm_memory_search_result_reset` 后还在读旧指针。
- record/chunk 数量是否正常。

可使用 `xllm_memory_get_diagnostics` 和 `xllm_memory_search_debug` 排查。

## 工作区索引为什么跳过了文件？

可能原因：

- 文件是隐藏文件。
- 扩展名不在 `sAllowedExtensions`。
- 命中了 ignored directories/extensions/patterns。
- 文件超过 `uMaxFileBytes`。
- `.gitignore` 或 ignore files 生效。
- 被敏感默认规则跳过。

查看 ingest result 的 skipped details 可以定位原因。

## xllm 会自动上传我的工作区吗？

xllm memory 负责本地 ingest/search/apply。是否把检索片段发给 provider，取决于你是否把它注入 request/turn 并调用模型。

你应先过滤敏感文件，再控制 RAG 注入预算。

## 什么时候用 `xllm_request`，什么时候用 `xllm_turn`？

`xllm_turn` 适合多数学习和聊天场景。它更方便。

`xllm_request` 适合高级场景，例如你需要完整控制 messages、context blocks、tools、response format 和 profile。

## 出错时应该记录哪些信息？

至少记录：

- `xllm_version()`
- API 返回值
- `xllm_error.eCode`
- `xllm_error.iHttpStatus`
- `xllm_error.sMessage`
- `xllm_error.sProviderCode`
- `xllm_error.sRequestId`
- 当前 profile ID 和模型

不要记录 API key 或完整敏感正文。

## 发布包验证失败怎么办？

先区分失败类型：

- `verify-version` 失败：同步版本文件、头文件宏和 release notes。
- checksum 失败：重新生成或重新下载包。
- `verify-artifact` 失败：检查 bundle 结构和 metadata。
- downstream smoke 失败：检查发布包是否漏头文件、源码或依赖。

继续读：[发布包消费案例](case/release-bundle-consumption.md)

## 是否应该现在生成英文文档？

当前文档策略是先完成中文版本，人工审阅通过后再生成 `.en.md` 翻译版本。在审阅前不生成英文文件。

## 去哪里查函数参数和资源释放？

读 [API 索引](api/README.md)。API 页按 xrt 风格写，包含函数原型、参数、返回值、资源归属、补充说明和范例代码。
