# SSH Auth Guard API

`ssh_auth_guard` 是认证客户端和服务端共享的纯资源预算层。它不解析消息、不保存用户名或凭据，
也不读取系统时钟；同步、future 和协程驱动都把同一个单调毫秒时间传入，因此行为完全一致且
可确定性测试。

默认策略采用 RFC 4252 建议的 10 分钟超时和 20 次认证尝试，并额外限制 32 个方法交互轮次、
256 条认证消息和 16 MiB 总报文字节。每个单项置零可显式禁用，达到上限本身仍被允许，下一项
才返回 `XSSH_AUTH_GUARD_DISCONNECT`。

每条认证消息只调用一次 `xrtSshAuthGuardReserve`：USERAUTH_REQUEST 使用
`XSSH_AUTH_EVENT_ATTEMPT`；keyboard challenge、密码更改提示等新交互使用
`XSSH_AUTH_EVENT_ROUND`；其余 failure、success、banner、response 使用
`XSSH_AUTH_EVENT_MESSAGE`。超限时首个原因冻结在 `Exhaustion`，不能通过切换用户名、服务名或
认证方法重置会话总预算。

成功消息计入预算后调用 `xrtSshAuthGuardComplete`。此后认证消息返回
`XSSH_AUTH_GUARD_IGNORE`，符合成功后忽略认证消息的协议要求。示例见
`examples/auth_guard/main.c`。
