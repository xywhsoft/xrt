# Remote Worker Agent 范例

> 对应源码：`examples/remote_worker_agent.c`

这个范例展示 xwork remote worker/control plane 的最小闭环。

## 解决的问题

Agent 任务可能需要在隔离进程、远程机器或专用 worker 上执行。xwork 提供任务分配、租约、结果、artifact chunk 和恢复对象，但不绑定具体网络服务实现。

## 流程

```text
create runtime/local host/file persistence/control plane
register worker with process.exec capability
enqueue remote task
worker claims assignment
worker executes local process.exec
complete task with result summary
persist control plane snapshot
recover and orphan in-flight assignment
continue queued work
```

## 关键点

- control plane 管理 worker、heartbeat、lease 和 assignment。
- host 产品负责 socket/auth/retry/blob streaming 等网络层。
- artifact 和 output chunk 可独立查询，适合大输出或远程传输。
- 恢复时 in-flight assignment 会被标记为 orphaned，避免误认为仍在执行。

## 状态流

```text
worker register
  -> heartbeat online
task queued
  -> assignment claimed
  -> worker executes process.exec
  -> task completed / failed / cancelled
snapshot persisted
  -> recovery marks in-flight assignment orphaned
```

## 关键 API

| API | 作用 |
| --- | --- |
| `xwork_control_plane_create()` | 创建控制平面。 |
| `xwork_control_plane_start()` | 启动 plane。 |
| `xwork_control_plane_register_worker()` | 注册 worker 和 capability。 |
| `xwork_control_plane_enqueue_task()` | 提交 remote task。 |
| `xwork_control_plane_claim_task()` | worker claim assignment。 |
| `xwork_control_plane_execute_next_local()` | 本地 worker 快捷执行。 |
| `xwork_control_plane_complete_task()` | 完成任务。 |
| `xwork_control_plane_upload_artifact()` | 上传 artifact summary/blob chunk。 |
| `xwork_control_plane_upload_output_chunk()` | 上传 stdout/stderr chunk。 |
| `xwork_file_persistence_recover_control_plane()` | 从 snapshot 恢复 plane。 |

## Artifact / output chunk

- artifact upload 用于结构化产物、blob ref、content hash 和 chunk payload。
- output chunk 用于 stdout/stderr 或终端输出分片。
- snapshot 会保存 chunk summary，恢复后仍可查询。
- 网络层的 base64、重试和 streaming 由宿主 transport 负责。

## 适合扩展

- 接入真实 HTTP/WebSocket/gRPC transport。
- 增加 worker auth 和 project/tenant 隔离。
- 使用 blob chunk API 传输大文件产物。
