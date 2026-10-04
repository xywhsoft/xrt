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

声明长度最多为 INT64_MAX。即使服务器未声明 APPENDLIMIT，Begin 也会在分配命令字段
或发送线路字节前拒绝大于 63 位的尺寸，保留已认证会话，随后仍可提交合法消息。

对已经驻留内存的小消息，`xrtImapClientAppend` 组合 Begin、Write 与 End，保持同一状态和
错误契约。

## 所有权与失败

Mailbox、Flags、InternalDate 和正文缓冲只在对应调用期间借用。结果中的 UID 数值由值语义
返回，不借用线路缓冲。服务器在正文发送前或完整上传后以 tagged `NO`/`BAD` 拒绝时，
客户端保留最近响应及可恢复会话；`NO` 返回 `XERR_PERMISSION`，`BAD` 返回
`XERR_PROTOCOL`。同步 literal 尚未上传就收到 tagged `OK` 是协议错序，会话进入失败终态。

写入超过声明长度返回 `XERR_RANGE`；尚未写满就调用 `AppendEnd` 返回 `XERR_STATE`，
活动上传仍可继续写入。实际传输失败、取消、超时或中途断流会使会话进入失败终态，
因为无法重新建立可靠的 IMAP 命令边界。

启用 COMPRESS 后，`AppendWrite` 成功表示编码器已接受该块正文，压缩数据可能仍在
编码器中等待刷新；`AppendEnd` 建立最后的同步刷新边界，再等待服务器 completion。
压缩处理期间也检查取消和截止时间，包括尚未产生网络输出的块。失败的调用不扣减
剩余 literal 长度，但已提交的网络字节不能回滚，调用方应销毁失败会话。
