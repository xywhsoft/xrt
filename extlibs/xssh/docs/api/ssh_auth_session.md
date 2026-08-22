# SSH Auth Session API

`ssh_auth_session` 在 `ssh_transport_core` 和具体认证方法之间提供 client/server 共用的认证编排。
它负责 `ssh-userauth` service、通用 USERAUTH 请求与结果、banner、方法扩展包、认证资源预算和成功
方向一致性，但不保存凭据、不选择认证方法、不访问网络，也不拥有 payload 缓冲。

## 分层

- `ssh_auth_message` 和 password/publickey/keyboard/hostbased 模块构建或解析方法 payload。
- `ssh_auth_session` 校验当前角色和阶段是否允许发送或接收该 payload。
- `ssh_transport_core` 完成 framing、加密、顺序、rekey 和 `USERAUTH_SUCCESS` 方向提交。
- 网络驱动只负责把最终 wire bytes 可靠入队或提供一个完整 packet。

未知的 60..79 方法消息统一返回 `XSSH_AUTH_SESSION_PACKET_METHOD`，完整 payload 由
`xrtSshAuthSessionMethod` 借出。由此，新认证方法可以直接组合现有会话，而不需要修改通用状态机。
server 发出的这类消息计入交互轮次，client 响应计入普通消息；USERAUTH_REQUEST 单独计入尝试次数。

## 写事务

1. 方法模块直接向调用方 payload 缓冲写报文。
2. 调用 `xrtSshAuthSessionWritePrepare` 验证阶段并预留预算。
3. 调用 `xrtSshTransportCoreWritePrepareWithPadding` 构建最终线路包。
4. 网络可靠接管整包后提交 transport core。
5. 最后调用 `xrtSshAuthSessionWriteCommit`。

背压或取消发生在可靠接管前时，先放弃 transport 写事务，再调用
`xrtSshAuthSessionWriteAbort`。认证阶段、尝试数、轮次和字节预算均不会被消费。

## 读事务

transport core 验证完整 packet 后，把 `Packet.Payload` 传给
`xrtSshAuthSessionReadPrepare`。Request、Failure、Banner 和 Method getter 返回的视图都借用该
packet，只能在读提交前使用；异步认证后端必须复制它真正需要跨等待保存的字段。

上层接受消息后先提交 transport core，再提交 auth session。已认证输入不能回滚；拒绝该输入时
调用 `xrtSshAuthSessionReadAbort` 并关闭 transport，避免线路序号与认证状态出现分叉。

## 生命周期

`xrtSshAuthSessionBegin` 只接受已完成首轮 KEX 的同角色 transport。client 从发送 service request
开始，server 从接收 service request 开始。banner 不改变当前主事件；failure 允许新请求；server
方法包允许 client 发送后续 request 或方法响应；success 必须由 server 方向经 transport 提交。

认证会话不处理 KEXINIT、NEWKEYS、DISCONNECT 等 transport 控制消息。驱动在 rekey 期间先把这些
消息路由给 KEX/transport 状态机，完成后继续同一个 auth session。完整示例见
`examples/auth_session/main.c`，双端事务和预算边界见 `tests/auth/test_ssh_auth_session.c`。
