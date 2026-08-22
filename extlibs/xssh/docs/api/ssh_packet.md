# SSH Packet API

`ssh_packet` 在 `ssh_wire` 之上提供 RFC 4253 的 binary packet framing。它不绑定密码算法、
随机实现或网络连接，packet、KEX 和自定义 transport 可以复用同一份公开 framing。

## 长度与视图

`xrtSshPacketMeasure` 计算 `padding_length` 和 `packet_length`。零块长使用 RFC 要求的最小
八字节；显式块长必须在 8 到 255 之间。返回的 `packet_length` 不包含它自身的四字节字段，
完整线路长度为 `4 + packet_length`。padding 至少四字节并保证完整线路长度按块对齐。

`xrtSshPacketRead` 返回借用 reader 输入的 `xsshpacketview`。零 `iMaxPacketSize` 使用
`XSSH_PACKET_MAX_DEFAULT`，调用方可以显式使用更严格或更宽的预算。短包返回
`XSSH_NEED_MORE`；畸形长度、块对齐和 padding 返回协议错误；超出预算返回
`XSSH_ERROR_OVERFLOW`。

## Padding 与原子性

`xrtSshPacketWrite` 强制调用方提供 `xsshpaddingproc`，不会静默使用零 padding。核心模块
因此不强制携带系统随机数，也允许 transport 使用已经播种的会话 PRNG。回调先写入最多
255 字节的栈临时区，成功后才提交目标 writer；回调失败、容量不足或参数错误均保持
writer 与序列号不变。
callback 主动失败返回 `XSSH_ERROR_CALLBACK`，与畸形线路的协议错误保持区分。

Reader 同样在副本中完成全部校验，只有完整成功后才推进输入、发布 packet view 并递增序列
号。序列使用 uint32 自然回绕；传入 NULL 可以在不需要 MAC 的独立 framing 工具中忽略序列。

示例见 `examples/packet/main.c`。示例的确定性 padding 不能直接用于真实 SSH transport。
