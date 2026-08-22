# SSH Connection Message API

`ssh_connection_message` 实现 RFC 4254 与 channel 无关的全局请求 envelope。模块只依赖 wire，
不拥有请求队列、网络或端口转发策略。

`xrtSshGlobalRequestWrite/Read` 严格校验请求名，并把未知请求的 type-specific fields 原样公开。
`xrtSshGlobalSuccessWrite/Read` 同样保留任意响应专用数据；
`xrtSshGlobalFailureWrite/Read` 处理严格无字段失败消息。因此 keepalive、端口转发和未来扩展都能
直接复用公开底层，不需要进入私有 parser。

协议要求需要回复的全局请求按请求顺序匹配响应，因为响应本身没有 request id。后续 connection
状态层负责该 FIFO；本模块不暗中分配队列。示例见 `examples/connection_message/main.c`。
