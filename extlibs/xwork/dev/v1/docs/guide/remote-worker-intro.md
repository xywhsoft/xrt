# 远程 Worker 与控制平面

> 状态：中文初稿，待人工审阅。

xwork 的 remote worker 能力提供控制平面对象和协议数据结构。它不强绑定某个网络 server；socket、认证、重试、部署和云控制面由宿主产品负责。

## 核心能力

| 能力 | 说明 |
| --- | --- |
| worker registry | 注册 worker、心跳、能力、租约和状态。 |
| assignment queue | 提交 remote task，按能力匹配 worker，claim/complete/fail/cancel。 |
| policy gate | 对 remote task 的工具、网络和审批策略做统一检查。 |
| artifact upload | 通过 blob ref、content hash、chunk metadata 和 payload bytes 上传结果。 |
| output chunks | 保存 stdout/stderr 或终端输出 chunk，支持查询和恢复。 |
| recovery | 恢复控制平面 snapshot，并将 in-flight assignment 标记为 orphaned。 |

## 网络边界

xwork 定义 decoded-message transport marker 和协议对象，但不内置完整 HTTP server/client。这样可以让 AI IDE、claw 或未来云服务按自己的安全模型接入：

- socket 生命周期。
- worker auth。
- tenant/project 隔离。
- retry/backoff。
- 大文件 blob streaming。

## Transport 边界图

```text
worker process
  |
  | HTTP/WebSocket/gRPC/etc.
  v
host-owned transport
  - socket lifecycle
  - auth / signing / mTLS
  - retry / replay protection
  - blob streaming
  |
  | decoded message
  v
xwork_control_plane API
  - register / heartbeat
  - enqueue / claim / complete
  - artifact/output chunk
  - snapshot / recovery
```

`XWORK_REMOTE_TRANSPORT_HTTP_BOUNDARY` 表示进入 xwork 前消息已经完成网络层解码和认证。xwork 保存 transport marker 用于审计和恢复，但不拥有 socket。

## In-process transport

`XWORK_REMOTE_TRANSPORT_IN_PROCESS` 适合本地 worker、测试和单进程部署。control plane 和 worker 共享内存，持有 `xwork_control_plane *` 即表示可信调用方。

生产远程部署必须在宿主层增加 worker auth、tenant/project 隔离、密钥管理和网络重试。

## 相关范例

- [Remote Worker 范例](../case/remote-worker-agent.md)
- [Remote Worker API](../api/api-remote-worker.md)
