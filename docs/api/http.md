# HTTP 协议底座

XRT 的 HTTP 模块是可直接组合 TCP、TLS 和自定义传输的 HTTP/1 线协议底座，
不提供客户端对象、服务器对象、路由、中间件或请求/响应拥有型模型。

核心目标是：严格解析不可信输入、完整表达 HTTP/1.0 和 HTTP/1.1 分帧、允许
调用方直接发送原始报文，并保证常用路径不分配正文缓冲。

完整函数、常量和类型索引见 [HTTP 公共符号参考](http-reference.md)。精确参数、所有权和
失败契约以对应公共头的中文注释为准。

## 模块边界

| 模块 | 裁剪宏 | 作用 |
| --- | --- | --- |
| `http` | `XRT_MODULE_HTTP` | token、字段、方法、状态和 Content-Length |
| `http_param` | `XRT_MODULE_HTTP_PARAM` | 参数与 quoted-string 语法 |
| `http_param_host` | `XRT_MODULE_HTTP_PARAM_HOST` | 参数解码后的无分配 Host 验证 |
| `http_expect` | `XRT_MODULE_HTTP_EXPECT` | Expect 字段 |
| `http_upgrade` | `XRT_MODULE_HTTP_UPGRADE` | Upgrade 字段与写出 |
| `http_te` | `XRT_MODULE_HTTP_TE` | TE 与 transfer-coding 参数 |
| `http_connection` | `XRT_MODULE_HTTP_CONNECTION` | Connection 选项与持久连接 |
| `http_trailer` | `XRT_MODULE_HTTP_TRAILER` | Trailer 声明和尾字段约束 |
| `http_encoding` | `XRT_MODULE_HTTP_ENCODING` | Accept-Encoding 与 Content-Encoding |
| `http_decode` | `XRT_MODULE_HTTP_DECODE` | 流式 gzip/deflate 自动解码 |
| `http_host` | `XRT_MODULE_HTTP_HOST` | Host authority |
| `http_target` | `XRT_MODULE_HTTP_TARGET` | 四种 request-target |
| `http1_head` | `XRT_MODULE_HTTP1_HEAD` | 起始行、Header 解析与写入 |
| `http1_net` | `XRT_MODULE_HTTP1_NET` | TCP 块链 Header 解析与余量保留 |
| `http1_tls` | `XRT_MODULE_HTTP1_TLS` | TLS 明文块链增量 Header 解析 |
| `http1_body` | `XRT_MODULE_HTTP1_BODY` | 正文计划、chunked 和 trailer |
| `http1_message` | `XRT_MODULE_HTTP1_MESSAGE` | 连续内存完整消息便利层 |

`http_decode` 才依赖 Inflate，`http1_net` 才依赖网络块链，`http1_tls` 才依赖 TLS Stream。
只启用连续内存 HTTP/1 解析不会带入压缩、网络、TLS 或 WebSocket。

## 借用模型

`xhttpfield`、`xhttp1head`、`xhttp1message` 和所有语法游标都借用调用方输入。
解析器不复制字段名称和值，输入缓冲和字段描述符数组必须覆盖借用视图的使用期。

字段描述符允许位于合法的未对齐存储。实现使用安全加载，不要求调用方为了协议
解析调整缓冲布局。

XRT 不再提供动态 Header 容器。需要拥有、修改或索引 Header 的框架应在 `xhttp`
中建立对象模型；快速路径可以直接使用栈数组或应用自己的存储。

## Host 语法

`xrtHttpHostParse` 保留 Host、可选端口和 IP-literal 分类，任意长度十进制端口在
协议层仍然合法，只有可放入 `uint16` 的值才带 `XHTTP_AUTHORITY_PORT_VALUE`。
`xrtHttpIpv4Valid` 与 `xrtHttpIpv6Valid` 是无错误副作用的纯谓词，公开同一套严格
IP 文本规则，扩展协议无需复制 Host 内部解析器。

`xrtHttpParamHostValid` 直接消费 `xhttpparam` 的语义值。quoted-pair 会在验证时
流式解码，任意长度 reg-name 与 IPvFuture 不进入固定缓冲，也不申请堆内存。

## Header 解析

`xrtHttp1RequestParse` 和 `xrtHttp1ResponseParse` 接受从消息首字节开始的累计输入：

- `XHTTP1_MORE`：输入尚未包含完整 Header；
- `XHTTP1_FIELDS`：Header 完整，但字段描述符容量不足，`FieldCount` 给出需求；
- `XHTTP1_READY`：解析完成；
- `XHTTP1_ERROR`：协议、限额或参数错误。

解析器只接受 CRLF，拒绝 obs-fold、裸 LF、非法字段名、控制字符、冲突的
Content-Length、歧义 Transfer-Encoding 和非法 Upgrade。`xhttp1errorinfo`
提供稳定错误码、消息相对偏移和行号。

`xhttp1limits` 分别限制 Header 总长度、起始行、单字段行和字段数量。默认值面向
公网输入，服务端应按路由策略进一步收紧，而不是无上限增长接收缓冲。

网络热路径使用 `xrtHttp1RequestParseBuffer`、`xrtHttp1ResponseParseBuffer` 直接扫描
`xnetbuf`。只有完整 Header 跨块时才连续化实际 Header 前缀，函数不消费输入，Upgrade 后的余量
保持原位。TLS 对应入口是 `xrtHttp1RequestParseTls`、`xrtHttp1ResponseParseTls`；输入不足时它们
通过 `xrtTlsStreamReadMore` 请求下一段受限明文，Header 完成后仍由调用方消费 `Head.Bytes`。

## 正文计划

Header 完成后必须调用：

- `xrtHttp1RequestBodyPlan` 处理请求；
- `xrtHttp1ResponseBodyPlan` 处理响应，并传入原请求方法。

`xhttp1bodyplan` 的模式是唯一分帧结论：

- `XHTTP1_BODY_NONE`：没有正文；
- `XHTTP1_BODY_FIXED`：Content-Length 定长；
- `XHTTP1_BODY_CHUNKED`：分块编码；
- `XHTTP1_BODY_CLOSE`：由可靠 EOF 定界；
- `XHTTP1_BODY_TUNNEL`：CONNECT 或 101 后的字节不再属于 HTTP。

响应计划覆盖 HEAD、成功 CONNECT、1xx、204 和 304。调用方不应仅凭
Content-Length 判断响应正文。

## 流式接收

`xrtHttp1BodyInit` 创建无分配 reader。`xrtHttp1BodyRead` 每次返回一个借用正文
片段、容量请求或终态，并通过 `Consumed` 精确说明已消费线路字节。

对于 chunked，reader 会验证 chunk-size、扩展语法、CRLF、last-chunk 和 trailer，
并严格拒绝 chunk-size 与分号或 CRLF 之间的空白，避免端点对报文边界产生分歧。
正文视图不包含分块元数据。`Received` 是应用正文长度，`WireBytes` 是正文区实际
线路长度。

当返回 `XHTTP1_BODY_FIELDS` 时，`TrailerCount` 给出所需描述符数量。调用
`xrtHttp1BodyTrailers` 绑定足够存储后，用同一输入继续解析，不会丢失进度。

close-delimited 模式只有在传输层给出可靠 EOF 时才传入 `bEnd = true`。超时、取消、
RST 和普通 EOF 不能混为同一种上层状态。

## gzip 自动解码

`xrtHttpDecodeCreate` 直接读取解析后的字段数组。把 `xrtHttp1BodyRead` 发布的每个
正文片段交给 `xrtHttpDecodeWrite`，并只在 body reader 完成时设置 `bFinal`。

identity 和显式原样回退路径不复制输入。gzip/deflate 路径按 Content-Encoding
逆序流式解码，验证 gzip Header、CRC、ISIZE 和压缩流终态。每个中间层与最终
输出都受同一明文硬限额约束。

详见 [HTTP 正文解码](http_decode.md)。

## 原始写出

`xrtHttp1RequestWrite` 和 `xrtHttp1ResponseWrite` 不添加 Host、Date、Server 或其他
策略字段。空输出用于精确测量，容量不足不会产生半个 Header。

高性能发送建议：

1. 在连接或协程栈上的小缓冲中写 Header；
2. 用 `xrtNetStreamSendVec` 一次提交 Header 与小正文；
3. 大正文用 `xrtNetStreamSendRef`、`SendRefs`、`SendTake` 或 `SendBuffer`；
4. chunked 正文只用 `xrtHttp1ChunkLineWrite` 生成短前缀，数据本身保持引用发送；
5. `XRT_NET_AGAIN` 时等待 writable/drain，不绕过网络队列硬上限。

这条路径只构造必需的线路字节，不创建请求、响应、字典或正文对象。

## 报文长度

- `xhttp1head.Bytes`：起始行、Header 和最终空行的线路长度；
- `xhttp1bodyplan.Length`：定长正文长度，仅在 `FIXED` 模式有效；
- `xhttp1body.Received`：已经发布的应用正文长度；
- `xhttp1body.WireBytes`：已经消费的正文线路长度；
- `xhttp1message.Wire.Size`：连续输入中第一条完整消息的线路总长度；
- `xhttp1message.BodyBytes`：移除 chunked 元数据后的正文长度。

chunked 和 close-delimited 消息在完成前不存在可提前得知的总线路长度。

## TLS 与 Upgrade

HTTP 不拥有 TLS。`http1_tls` 只是块链适配器，不创建连接或安全策略；证书验证、SNI、会话恢复和
ALPN 仍属于通用 TLS 模块。它按需连续化实际 Header，不增加连接级固定缓冲。

`xrtHttpUpgrade*`、`xrtHttpConnection*` 和 WebSocket 握手函数共同完成 Upgrade
验证。101 后的剩余字节必须原样交给新协议，不能继续送入 HTTP body reader。

## 完整消息便利层

`xrtHttp1RequestMessageParse` 和 `xrtHttp1ResponseMessageParse` 适合已经连续驻留在
内存中的报文、测试和协议网关。它们仍只借用输入，不持有堆对象。

固定长度或 close-delimited 正文可由 `xrtHttp1MessageBodyView` 直接查看；chunked
正文使用 `xrtHttp1MessageBodyCopy` 去除分帧。真正的网络热路径应优先使用流式
body reader，避免等待和复制整个消息。

## 扩展库边界

客户端池、重定向、重试、缓存、认证、Cookie、MIME、Multipart、FormData、SSE、
服务器路由和中间件已经迁入 `extlibs/xhttp/archive`。备份不参与 XRT 构建，下一
阶段会在不破坏上述快速路径的前提下重建 `xhttp`。
