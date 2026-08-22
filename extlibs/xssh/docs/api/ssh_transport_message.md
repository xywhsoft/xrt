# SSH Transport 公共消息

`ssh_transport_message` 只依赖 `ssh_wire`，负责 transport 状态机两端都需要的公共 payload，
不拥有网络、密钥、压缩器或会话内存。

## RFC 4253

模块覆盖 `DISCONNECT`、`IGNORE`、`UNIMPLEMENTED`、`DEBUG`、`SERVICE_REQUEST`、
`SERVICE_ACCEPT` 和 `NEWKEYS`。每类消息都提供对称的 `Write` / `Read`；解析严格拒绝错误消息号、
截断和尾随字段，失败不发布输出或推进 writer。

`xrtSshMessageType` 可在自定义状态机中先读取消息号，再把完整 payload 分派给具体解析器。诊断文本
保持借用字节，不在 wire 层强制 Unicode 策略。

## RFC 8308

`xrtSshExtInfoWrite` 接受扩展数组，`xrtSshExtInfoRead` 先线性验证完整消息，
`xrtSshExtInfoNext` 再以借用视图迭代。扩展值保持任意二进制，包括任意位置的零字节；模块不设置
固定扩展数量上限，也不为解析分配数组。调用方应忽略未知扩展，并按具体扩展定义解释值。

`xrtSshNewCompressWrite` / `xrtSshNewCompressRead` 只处理 delayed compression 的触发消息；何时
允许发送、压缩上下文何时重置，属于 transport 状态层而不是消息编解码层。
