# WebSocket 协议底座

XRT 的 WebSocket 模块保留可组合、高性能的协议核心：帧、消息、握手、扩展协商和
permessage-deflate。已建立连接、Future、TLS 和连接组当前仍可用，但会在下一阶段按
与 HTTP 相同的原则复审；只有不强迫高级对象且不妨碍直接收发的路径才会继续留在核心。

HTTP 客户端/服务器适配、路由和框架型便利对象已经退出核心，备份位于
`extlibs/xws/archive`。

## 裁剪层次

| 层次 | 主要模块 | 依赖与用途 |
| --- | --- | --- |
| 帧 | `websocket_frame` | 无网络依赖的 parser、writer 和掩码 |
| 关闭 | `websocket_close` | Close code、UTF-8 reason |
| 消息 | `websocket_message` | 分片消息、控制帧和角色规则 |
| 握手 | `websocket_handshake`、`websocket_keygen` | HTTP 字段、SHA-1、Base64、随机数 |
| 扩展 | `websocket_extension`、`websocket_deflate` | 扩展字段和 permessage-deflate 协商 |
| 压缩 | `websocket_inflater`、`websocket_deflater` | 无上下文或复用上下文的流式实现 |
| 连接（待复审） | `websocket_connection` | 已连接 `xnetstream` 的事件与背压 |
| 所有权（待复审） | `websocket_connection_ref`、`websocket_writer` | 引用发送与流式写入 |
| 异步（待复审） | `websocket_connection_future` | Future 与协程等待桥接 |
| TLS（待复审） | `websocket_connection_tls` | 已建立 `xtlsstream` 的传输接管 |
| 分组（待复审） | `websocket_group`、`websocket_group_future` | 连接集合和有界广播 |

只使用帧 parser 不会带入网络、TLS、Future 或压缩。permessage-deflate 的握手协商
与压缩运行时也可以分别裁剪。

## 帧

`xwsframe` 描述 FIN、RSV、opcode、mask 和 64 位 payload 长度。解析器区分输入不足
与协议错误，并拒绝：

- 非最短长度编码；
- 大于 63 位的 payload 长度；
- 非法或保留 opcode；
- 控制帧分片或长度超过 125；
- 不符合端点角色的 mask；
- 未经扩展允许的 RSV 位。

`xrtWsFrameWrite` 只写 Header，不复制 payload。发送方可以把 Header、payload 和
后续片段一次交给网络向量发送。发起端掩码可由 `xrtWsMask` 原地或分片执行，offset
允许跨多个输入片段保持掩码相位。

## 消息状态机

`xwsmessagestate` 在帧层之上验证 continuation、文本 UTF-8、控制帧穿插、Close 负载和
消息长度限额。它不保存完整消息，正文片段借用输入并立即交给调用方。

服务端角色要求客户端帧有 mask，客户端角色要求服务端帧无 mask。角色在连接建立
时固定，不能根据单帧自动猜测。

控制帧不会中断正在进行的分片消息。Ping 应尽快产生相同 payload 的 Pong；收到
Close 后只能完成关闭握手，不再开始新的数据消息。

## 握手

握手工具公开底层可复用步骤：

- 生成和验证 `Sec-WebSocket-Key`；
- 计算 `Sec-WebSocket-Accept`；
- 解析、选择和写出子协议列表；
- 验证 Upgrade、Connection、版本和 101 响应字段；
- 解析扩展列表与参数。

函数只处理字段和借用视图，不创建 HTTP 客户端或服务器。调用方先用 HTTP/1 parser
得到 Header，再执行 WebSocket 握手验证；成功后把 parser 未消费的字节直接交给
WebSocket 消息层。

服务端必须使用不可预测随机数生成客户端可接受的握手材料和 masking key，不能把
固定示例值用于生产连接。

## permessage-deflate

`xwsdeflateconfig` 表达双方角色、context takeover 和 window bits。协商器严格
区分 offer 与 response，不接受重复参数、非法范围或响应凭空增加能力。

压缩规则：

- RSV1 只出现在压缩消息的第一个数据帧；
- continuation 不重复 RSV1；
- 控制帧永不压缩；
- 解码消息尾部按 RFC 7692 补回同步刷新尾；
- no-context-takeover 在消息边界复位字典；
- 解压总量受消息和 Inflate 双重硬限额约束。

`xwsinflater` 与 `xwsdeflater` 是流式对象，不需要聚合完整消息。连接 writer 可在压缩
启用时保持同一套背压与所有权语义。

## 已建立连接

`xwsconn` 接管已经完成握手的 TCP 或 TLS 流。它不负责 URL、DNS、HTTP 请求、证书
策略或重定向。

连接事件包含消息开始、数据片段、消息结束、Ping、Pong、Close、错误和最终关闭。
回调中的数据视图只在回调期间有效；需要跨回调保存时必须复制或接管拥有型缓冲。

接收流控使用 pause/resume，不允许靠无限增长应用队列解决慢消费者。发送路径继承
网络流的高/低水位、最大排队字节和 `XRT_NET_AGAIN` 契约。

## 发送路径

连接层提供三类所有权：

- copy：返回前完成复制，适合短控制帧和短消息；
- ref：队列释放时调用一次 release，适合静态或共享大块；
- take/buffer：把已分配内存或 XRT 网络缓冲链交给连接。

`xwswriter` 负责长消息分片。writer 的 Header 和压缩状态尺寸稳定，不因 hover、事件
或动态内容改变网络对象布局。发送失败遵守原子所有权规则：未受理时所有权仍归调用
方，受理后只由连接释放。

## Future 与协程

Future 只包装已建立连接上的发送、关闭、drain 和终态等待，不创建隐藏 HTTP 客户端。
取消等待不等于无条件销毁连接；调用方必须根据具体操作契约决定继续使用、正常关闭
或 abort。

协程通过通用 Future/Wait 体系等待，不在 WebSocket 内维护第二套调度器。

## TLS

TLS 连接路径接管已握手的 `xtlsstream`。SNI、证书验证、ALPN、会话恢复和超时属于
通用 TLS/网络层。WebSocket 只保证加密传输上的帧、消息、背压和关闭语义与普通 TCP
一致。

## 连接组

`xwsgroup` 以引用持有连接并支持并发加入、移除和快照广播。广播不会绕过单连接发送
上限；慢连接按调用方策略跳过、等待或关闭，不能拖出无界共享队列。

Future 广播返回可等待的聚合结果，取消聚合等待不会破坏已经被各连接受理的数据所有权。

## 错误与关闭

协议错误使用稳定 WebSocket 域错误码，并尽可能映射到 RFC Close code。网络错误、
TLS 错误和应用回调错误保留各自错误域，不被压成一个布尔原因。

正常关闭顺序是：发送或响应 Close、停止新数据发送、等待对端 Close 或截止时间、
关闭写方向并等待传输结束。协议破坏、超限或底层失败可以直接 abort，但仍必须释放
所有已受理引用一次。

## 高级适配边界

原 WebSocket 客户端、服务器、HTTP Future、路由和代理/TLS 便利入口已迁移到
`extlibs/xws/archive`。下一阶段会先继续收敛 XRT 内剩余连接层，再由 `xws` 基于公开
HTTP/1、TCP/TLS 和 WebSocket 原语重建高级能力，不能让对象模型反向污染帧与直接
收发路径。
