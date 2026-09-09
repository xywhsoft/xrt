# AI IDE Agent 范例

> 对应源码：`examples/ai_ide_agent.c`

这个范例展示 `xcode` profile 下的 AI IDE 集成基线。

## 解决的问题

AI IDE 需要让模型读取项目、提出修改、等待用户审批，再把结果沉淀为可查看的 artifact。这个范例跑通了最小闭环。

## 流程

```text
create runtime/workspace/local host
apply xcode profile
read README.md
emit file-content artifact
mock model requests filesystem.apply_patch
policy requires approval
submit approval
resume tool loop
emit patch artifact and final report
```

## 关键点

- `xcode` profile 默认偏半自动，适合 IDE 中的人类审阅工作流。
- patch 使用 dry-run 语义，适合 UI 展示和人工确认。
- 文件内容、patch 结果和最终报告都会进入 artifact 流。
- 审批暂停是可恢复状态，不是一次临时 callback。

## 关键 API

| API | 作用 |
| --- | --- |
| `xwork_profile_get_builtin(XWORK_PROFILE_XCODE)` | 获取 AI IDE 默认 profile。 |
| `xwork_local_host_configure_services()` | 启用本地 filesystem host service。 |
| `xwork_runtime_register_builtin_tool()` | 注册 `filesystem.read_text` 和 `filesystem.apply_patch`。 |
| `xwork_runtime_invoke_host_service()` | 读取 `README.md`。 |
| `xwork_run_emit_output_artifact()` | 记录文件内容 artifact。 |
| `xwork_run_emit_patch_artifact()` | 记录 dry-run patch artifact。 |
| `xwork_run_execute()` | 执行 mock model turn + tool loop。 |
| `xwork_run_get_last_approval_request()` | 读取审批请求。 |
| `xwork_run_submit_approval()` | 提交审批结果。 |
| `xwork_run_resume()` | 恢复已审批 run。 |

## Approval 生命周期

```text
model requests filesystem.apply_patch
policy requires approval
run enters WAITING_APPROVAL
UI reads xwork_approval_request
user approves
xwork_run_submit_approval
xwork_run_resume
orchestrator executes tool and completes
```

## Artifact 列表

| Artifact | 说明 |
| --- | --- |
| file-content output | `README.md` 的读取结果。 |
| patch artifact | dry-run patch，带 apply result 和 file summary JSON。 |
| final report | `xwork.report.v1` final report。 |

## 适合扩展

- 接入真实 editor buffer host service。
- 将 approval request 显示到 IDE UI。
- 把 artifact summary 展示为文件变更、诊断和报告面板。
