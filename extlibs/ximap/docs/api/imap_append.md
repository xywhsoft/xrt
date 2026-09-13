# IMAP APPEND API

`imap_append` 提供不聚合邮件正文的流式 APPEND。命令头、literal 正文和结束 CRLF 分别
发送，调用方可以把文件、MIME 生成器或其他流直接写入网络。

## Literal 模式

`XIMAP_LITERAL_AUTO` 根据 CAPABILITY、APPENDLIMIT、LITERAL+ 和 LITERAL- 选择协议允许的
模式；`SYNC` 等待服务器 continuation，`NONSYNC` 在协议能力允许时省去一次往返。显式
模式用于调用方按延迟、吞吐和服务器兼容性作出选择。

`xrtImapClientAppendBegin` 声明精确正文长度。`AppendWrite` 直接发送调用方提供的块，不创建
整封邮件副本，并拒绝超过声明长度的写入。只有剩余长度为零时 `AppendEnd` 才会发送结束
CRLF、消费 completion，并解析 UIDPLUS 的可选 APPENDUID。

对已经驻留内存的小消息，`xrtImapClientAppend` 组合 Begin、Write 与 End，保持同一状态和
错误契约。

## 所有权与失败

Mailbox、Flags、InternalDate 和正文缓冲只在对应调用期间借用。结果中的 UID 数值由值语义
返回，不借用线路缓冲。服务器在正文发送前拒绝同步 literal 时会保留可恢复会话；正文开始
后发生短写、取消、超时、长度错配或线路错误时，客户端进入失败终态，因为无法重新建立
可靠的 IMAP 命令边界。
