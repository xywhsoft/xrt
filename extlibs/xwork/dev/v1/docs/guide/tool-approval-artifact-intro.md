# 工具、审批与 artifact

> 状态：中文初稿，待人工审阅。

真实 Agent 开发的关键风险不是“模型是否能返回 tool call”，而是工具执行会产生真实副作用。xwork 将工具、副作用、审批和产物统一建模，避免这些逻辑散落在产品层。

## 工具分层

| 层次 | 说明 |
| --- | --- |
| Tool definition | 暴露给模型的工具名、描述、参数 schema 和风险标记。 |
| Tool executor | xwork 内部或调用方提供的执行函数。 |
| Host service | 产品侧实际执行文件、进程、终端、VCS、编辑器动作的桥接层。 |
| Policy gate | 在副作用发生前判断是否允许、拒绝或需要审批。 |
| Artifact | 执行后留下可查询、可持久化、可审计的结构化结果。 |

## 内置 host tool 类别

- `filesystem.*`：读取、写入、列目录、stat、glob、mkdir、move、delete、apply_patch。
- `process.*`：命令执行和交互式终端 session。
- `vcs.*`：status、diff、log、branch 等只读版本控制查询。
- `editor.*`：编辑器 buffer 相关桥接边界。

## artifact 类型

xwork 会为常见工具结果生成 typed artifact：

- 文件内容 output artifact。
- patch artifact，包含 `xwork.patch_apply_result.v1` 和 `xwork.patch_file_summary.v1`。
- command artifact，包含 stdout/stderr、exit code、截断标记和 I/O 统计。
- terminal state / inventory artifact。
- diagnostics report artifact。
- 通用 `xwork.report.v1` 报告 artifact。

## 常见 JSON 请求与响应

读取文件请求：

```json
{"path":"README.md","offset_bytes":0,"max_bytes":4096}
```

成功响应包含：

```json
{"ok":true,"path":"README.md","text":"...","file_size_bytes":1234,"bytes_read":1234,"eof":true}
```

执行命令请求：

```json
{"command":"git status --short","cwd":"D:/git/project","timeout_ms":10000,"include_events":true}
```

成功响应包含：

```json
{"ok":true,"command":"git status --short","stdout":"","stderr":"","exit_code":0,"timed_out":false,"cancelled":false}
```

读取终端状态请求：

```json
{"session_id":"terminal-1","after_seq":0,"max_events":100}
```

成功响应使用 `xwork.terminal_state.v1`，包含 session id、输出窗口、事件游标和是否还有更多事件。

失败响应应包含 `ok:false`、`error_kind` 和可选 `error`，便于 UI 和 replay 做结构化判断。

## 设计约束

- 文件、进程、网络和终端操作必须经过 policy。
- 可恢复 run 不恢复 live OS process handle 或本地 terminal session。
- 持久化 artifact 是审计和恢复线索，不等同于活跃资源句柄。
- 产品层可以自定义 host service，但不应绕过 xwork 的策略和事件模型。

## 相关文档

- [API 文档索引](../api/README.md)
- [Host Tools API](../api/api-host-tools.md)
- [AI IDE Agent 范例](../case/ai-ide-agent.md)
- [claw 自主 Agent 范例](../case/claw-autonomous-agent.md)
