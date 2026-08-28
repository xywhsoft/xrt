# WebSocket 协议底座

XRT 的 WebSocket 模块提供 RFC 6455、RFC 7692 协议原语，以及可选的轻量 TCP/TLS
事件流。核心链路覆盖 HTTP/1.1 Upgrade、`ws`、`wss`、严格帧与消息解析、背压、关闭握手和
permessage-deflate，但不包含 Future、协程、重连、心跳调度、连接组、路由或应用会话策略。

完整公共符号清单见 [WebSocket API 参考](websocket-reference.md)。

## 模块边界

| 层次 | 模块 | 能力 |
| --- | --- | --- |
| 帧 | `websocket_frame` | 帧头解析、帧头写出、分片 mask |
| 关闭 | `websocket_close` | Close code、UTF-8 reason、payload 解析与写出 |
| 消息 | `websocket_message` | 分片、控制帧穿插、角色规则、UTF-8 和消息上限 |
| 握手 | `websocket_handshake`、`websocket_keygen` | Key、Accept、版本、子协议和 Upgrade 字段 |
| 扩展 | `websocket_extension`、`websocket_deflate` | 扩展字段与 permessage-deflate 协商 |
| 压缩 | `websocket_inflater`、`websocket_deflater` | 有界、流式、可复用的消息压缩与解压 |
| Upgrade | `websocket_upgrade`、`websocket_upgrade_deflate` | 完整 HTTP/1.1 Upgrade 校验、字段写出和压缩协商 |
| 通信 | `websocket_stream`、`websocket_stream_tls` | TCP/TLS 接管、消息事件、背压和关闭握手 |
| 组合 | `websocket_upgrade_stream` | 把基础协商结果直接映射为 Stream 配置，不强制带入压缩 |
| 压缩组合 | `websocket_upgrade_stream` + `websocket_upgrade_deflate` + `websocket_stream_deflate` | Upgrade、Stream 与 permessage-deflate 完整链路 |

每层都可以独立裁剪。仅选择帧层不会带入 HTTP、网络、TLS 或压缩；Upgrade 层才依赖 HTTP/1；
TCP Stream、TLS 适配和压缩发送分别独立启用。`websocket_upgrade_stream` 本身不依赖压缩，只有同时
选择 `websocket_upgrade_deflate` 与 `websocket_stream_deflate` 才启用协商后的压缩链路。任何核心组合
都不会带入 Future、协程或任务系统。

## 高性能收发

发送端使用 `xrtWsFrameWrite` 在调用方栈内存或固定小缓冲中生成最多 14 字节的帧头，随后将帧头和
payload 作为两个片段直接交给 `xrtNetStreamSendRefs`、TLS 写入器或调用方自己的向量发送接口。
模块不会聚合、复制或暂存完整报文。客户端 payload 必须使用 `xrtWsMask` 处理；该函数接受 offset，
因此可以跨任意数量的输入分片保持 mask 相位。

接收端先向 `xrtWsFrameParse` 提供当前可见字节。返回 `XWS_FRAME_MORE` 时继续累积最多 14 字节帧头；
返回 `XWS_FRAME_READY` 后，调用方按 `PayloadSize` 直接消费网络缓冲中的 payload。每个分片依次经过
`xrtWsMessageFrameBegin`、`xrtWsMessagePayload` 和 `xrtWsMessageFrameEnd`，正文仍借用原网络缓冲，
无需构造完整消息副本。

这种边界允许应用自行选择纯协议原语，或使用 `xwsstream` 完成标准事件驱动通信。更高层仍可在
同一底座上组合同步等待、Future 或协程，核心不会隐藏背压。

## 轻量 Stream

`xrtWsStreamAttach` 与 `xrtWsStreamAttachTls` 接管已经完成 Upgrade 的 TCP/TLS 调用方引用。
`iPrefix` 是已经验证但尚未消费的 HTTP Header 长度：接管时先复制协商出的子协议，再精确消费
Header，最后异步处理同一缓冲中的帧余量。失败不会部分接管传输。

接收事件按 `MessageBegin`、零个或多个 `MessageData`、`MessageEnd` 发布。正文视图只在当前同步
回调有效，Stream 不聚合完整消息，也不为每条连接预分配固定 8 KiB 缓冲；TCP/TLS 现有块链直接
进入帧和消息状态机。TLS 帧头跨 record 时，Stream 通过受 `PlainLimit` 限制的 `ReadMore` 路径
累积最多 14 字节的完整帧头，不会因保留一个不完整前缀而停住。

发送 API 提供 copy 和可选 ref 路径，压缩发送按需创建 Deflater。`SendLimit` 是 WebSocket 与
底层传输待发量的统一硬边界，达到上限返回 `XNET_RESULT_AGAIN`；`Backpressure`、`Writable` 和
`Drain` 给出恢复边沿。Ping 可自动回复，Close 保证唯一发送、超时和对端终态快照。

## 帧与消息契约

`xwsframe` 描述 FIN、RSV、opcode、mask 和 63 位 payload 长度。严格解析拒绝：

- 非最短长度编码和超过 63 位的长度；
- 保留或未允许的 opcode、RSV 位；
- 分片控制帧或超过 125 字节的控制帧；
- 不符合客户端/服务端角色的 mask；
- 输出范围重叠、地址回绕和无效配置。

`xrtWsFrameConfigInit` 的 `Mask` 默认为 `XWS_MASK_ANY`，只用于角色未知的协议工具和
中间层。端点直接使用帧解析器时，服务端必须改为 `XWS_MASK_REQUIRED`，客户端必须改为
`XWS_MASK_FORBIDDEN`；`xwsstream` 已按连接角色自动设置该策略。

`xwsmessagestate` 不保存完整消息，只记录协议状态。它验证 continuation 顺序、Text UTF-8、
控制帧穿插、Close payload、扩展位和解码后消息上限。Ping/Pong/Close 由返回的消息标志交给调用方处理；
核心不自动回复、不自动关闭网络流。消息状态配置默认 `MaxSize = SIZE_MAX`，因为这一层不聚合正文；
任何收集完整消息或持有正文的上层都必须设置符合自身内存预算的有限上限。面向不可信对端时，
`xrtWsMessageConfigInitSafe` 以 `XWS_MESSAGE_SIZE_SAFE_DEFAULT`（16 MiB）作为默认
消息上限初始化配置，防止恶意超长消息耗尽内存；需要无上限流时必须显式恢复
`MaxSize = SIZE_MAX`。XRT stream 层已提供自己的连接级默认限制。

## 握手与扩展

握手函数只处理 HTTP 字段值：

- 验证或生成 `Sec-WebSocket-Key`；
- 计算并验证 `Sec-WebSocket-Accept`；
- 解析、检查、选择和写出子协议列表；
- 检查方法、状态、版本、Upgrade、Connection、Host 和正文约束；
- 遍历、计数和写出扩展及参数。

`xrtWsUpgradeRequestCheck` 和 `xrtWsUpgradeResponseCheck` 对完整 `xhttp1head` 执行角色相关校验，
包括唯一 Host、GET/101、HTTP/1.1、Key/Accept 绑定、版本 13、Connection、Upgrade、正文禁止、
子协议和扩展协商。重复字段按线路顺序处理，重复子协议和歧义响应会被拒绝。

`xrtWsUpgradeRequestFields`、`xrtWsUpgradeResponseFields` 只填充调用方字段描述符，可继续与
`xrtHttp1RequestWrite`、`xrtHttp1ResponseWrite` 组合。TCP 使用 `xrtHttp1*ParseBuffer`，TLS 使用
`xrtHttp1*ParseTls`；后者在 Header 跨 TLS record 时自动请求有界增量明文。成功后把 `Head.Bytes`
直接传给 Attach，Upgrade 后已经到达的帧不会丢失。

生产客户端必须通过 `xrtWsKeyGenerate` 使用安全随机数生成 Key；示例固定值只能用于协议测试。

## permessage-deflate

协商层严格区分 offer 与 response，拒绝重复参数、非法 window bits、未知响应能力和不一致的
context takeover。`xrtWsDeflateDirection` 将协商结果转换为单方向运行参数。

`xwsinflater` 与 `xwsdeflater` 都是流式对象：

- 输入和输出按分片推进，不聚合整条消息；
- 输出通过回调直接进入调用方缓冲或网络发送队列；
- 解压总量受消息上限和 Inflate 上限双重约束；
- no-context-takeover 在消息边界重置字典；
- `xrtWsDeflaterBound` 可在写入前完成容量规划；
- 回调失败、OOM 或数据错误后具有明确的 reset、abort 或 destroy 路径。

RSV1 只允许出现在压缩消息的首个数据帧，continuation 不重复 RSV1，控制帧永不压缩。

## 所有权与错误

帧、消息、Close、握手和扩展解析器只借用输入视图；成功后也不会接管输入。所有写出函数使用
调用方缓冲，并在容量不足时返回所需长度，不留下半个结构。Upgrade 结果拥有 Accept 与扩展响应，
子协议暂时借用 HTTP Header；Attach 会在消费 Header 前复制它。Inflater、Deflater 和 Stream
持有资源，必须由对应 `Destroy` 释放。

协议错误通过稳定的模块错误码和 `xrtGetError`/结构化错误链表达。输入不足不是协议错误；
只有确定违反协议、参数范围、资源上限、输出回调或压缩状态时才进入失败路径。

## 扩展能力

连接对象、Writer、Future/协程桥接、重连与心跳策略、连接组和广播属于独立发布的 `xws`
扩展。基础 TCP/TLS 通信、关闭、背压和连接级压缩留在 XRT；`xws` 在这条稳定路径上提供高级抽象。

## 测试与示例

`tests/websocket/` 覆盖固定向量、分块矩阵、属性测试、no-allocation、OOM、协议模糊测试，以及
真实 Select 回环上的 `ws`/`wss` Upgrade。集成测试会拆分 TLS Header 和一字节帧头，并验证压缩
发送、解压接收和完整 Close。`examples/websocket/` 不依赖高级连接对象。
