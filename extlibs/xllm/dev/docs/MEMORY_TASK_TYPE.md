# xllm Task Memory Type

`task.v1` 是 xllm 提供给 AI IDE / claw 的 typed long-memory 记录格式，用于让 LLM 检索任务上下文。真实任务状态机、调度、权限和执行仍由宿主或 xwork 负责。

## Public API

- `xllm_memory_ingest_task_options_init`
- `xllm_memory_ingest_task`
- `xllm_memory_task_status`

## Metadata Contract

Task memory 写入 `XLLM_MEMORY_SCOPE_MEMORY`，默认 `record_id` 为 `task:<task_id>`，默认 `source_uri` 为 `task://<task_id>`。

稳定 metadata:

- `memory_type=task.v1`
- `extraction_policy=task`
- `task_id`
- `task_status=open|done|canceled`
- `task_owner`
- `task_deadline_unix`
- `source_conversation_id`
- `source_turn_id`
- `priority`
- `expires_at_unix`
- `created_at_unix`
- `updated_at_unix`

## Boundary

xllm 只负责把任务作为可检索记忆保存、过滤和注入上下文。xwork 或宿主负责：

- open/done/canceled 的真实状态流转。
- owner/deadline 的业务校验。
- crash recovery、task queue、tool execution、checkpoint。
- 决定何时调用 `xllm_memory_ingest_task` 更新 typed memory。
