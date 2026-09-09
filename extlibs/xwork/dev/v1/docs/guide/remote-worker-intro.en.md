# Remote Worker and control plane

>Status: First draft in Chinese, awaiting manual review.

xwork's remote worker capabilities provide control plane objects and protocol data structures. It does not force binding to a network server; the socket, authentication, retry, deployment and cloud control plane are taken care of by the host product.

## Core Competencies

| Capabilities | Description |
| --- | --- |
| worker registry | Register workers, heartbeats, capabilities, leases and status. |
| assignment queue | Submit remote tasks, match workers according to capabilities, claim/complete/fail/cancel. |
| policy gate | Unified inspection of remote task tools, networks, and approval policies. |
| artifact upload | Upload results via blob ref, content hash, chunk metadata and payload bytes. |
| output chunks | Save stdout/stderr or terminal output chunk, support query and recovery. |
| recovery | Recover the control plane snapshot and mark the in-flight assignment as orphaned. |

## Network Boundary

xwork defines decoded-message transport marker and protocol objects, but does not have a full HTTP server/client built in. This allows AI IDE, claw or future cloud services to access according to their own security model:

- socket life cycle.
-worker auth.
- tenant/project isolation.
- retry/backoff.
- Large file blob streaming.

## Transport boundary map

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

`XWORK_REMOTE_TRANSPORT_HTTP_BOUNDARY` indicates that the message has completed network layer decoding and authentication before entering xwork. xwork saves the transport marker for auditing and recovery, but does not own the socket.

## In-process transport

`XWORK_REMOTE_TRANSPORT_IN_PROCESS` is suitable for local worker, testing and single-process deployments. The control plane and worker share memory, and holding `xwork_control_plane *` represents a trusted caller.

Production remote deployment must add worker auth, tenant/project isolation, key management and network retries at the hosting layer.

## Related examples

- [Remote Worker Example](../case/remote-worker-agent.md)
- [Remote Worker API](../api/api-remote-worker.md)
