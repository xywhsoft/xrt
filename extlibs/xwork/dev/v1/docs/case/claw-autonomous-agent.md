# claw 自主 Agent 范例

> 对应源码：`examples/claw_autonomous_agent.c`

这个范例展示 `xclaw` profile 下的自主 Agent 基线。

## 解决的问题

claw 类产品需要更高自主性，可以自动执行低风险或已授权动作，同时保留 artifact、snapshot 和恢复能力。

## 流程

```text
create runtime/workspace/local host/file persistence
apply xclaw profile
execute process.exec
emit command artifact
emit final report
persist run snapshot
recover run from file persistence
```

## 关键点

- `process.exec` 基于 xrt subprocess 路径，支持 stdout/stderr、timeout、stdin、env 和事件输出。
- 命令结果会生成 command artifact 和必要的 diagnostics report。
- run snapshot 可恢复生命周期状态，但不会恢复 live process handle。
- `xclaw` profile 可以启用更高自动化策略，但网络默认仍应 deny-by-default。

## process.exec 请求/响应

请求示例：

```json
{"command":"cmd /c echo xwork-claw-example","timeout_ms":10000,"include_events":true}
```

成功响应包含：

```json
{"ok":true,"stdout":"xwork-claw-example\r\n","stderr":"","exit_code":0,"timed_out":false,"cancelled":false}
```

实际输出文本因平台 shell 和换行而不同，调用方应依赖结构化字段而不是硬编码完整 stdout。

## 关键 API

| API | 作用 |
| --- | --- |
| `xwork_profile_get_builtin(XWORK_PROFILE_XCLAW)` | 获取自主 Agent 默认 profile。 |
| `xwork_file_persistence_configure_backend()` | 配置 `.xwork_claw_store`。 |
| `xwork_local_host_configure_services()` | 启用 local process host service。 |
| `xwork_runtime_register_builtin_tool()` | 注册 `process.exec`。 |
| `xwork_runtime_invoke_host_service()` | 执行命令。 |
| `xwork_run_emit_command_artifact()` | 保存命令 artifact。 |
| `xwork_run_emit_report_artifact()` | 保存 final report。 |
| `xwork_run_get_snapshot()` | 获取 run snapshot。 |
| `xwork_runtime_recover_run_from_persistence()` | 从 file persistence 恢复 run。 |

## 恢复边界

可以恢复 run 状态、summary、event、artifact 和 latest snapshot。不能恢复已经结束或仍在外部运行的 live process handle。产品如果需要恢复后继续命令执行，应重新调度一个新的 host tool 调用。

## 适合扩展

- 接入真实模型 provider。
- 加入更严格的命令 allow/deny policy。
- 将 command artifact 转换为任务报告或诊断列表。
