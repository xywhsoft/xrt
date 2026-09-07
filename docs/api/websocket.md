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

### `xrtWsFrameConfigInit`

初始化帧解析的默认配置与上限。

```c
void xrtWsFrameConfigInit(xwsframeconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[frame](../../examples/websocket/frame/main.c) · 帧配置

```c
	xrtWsFrameConfigInit(&Config);
```

### `xrtWsFrameInit`

把帧结构初始化为可复用状态。

```c
void xrtWsFrameInit(xwsframe* pFrame)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFrame` | 输出 | 非空 | 接收帧 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[frame](../../examples/websocket/frame/main.c) · 初始化帧

```c
	xrtWsFrameInit(&Frame);
```

### `xrtWsFrameParse`

从输入头部解析一个帧描述。

```c
xwsframestatus xrtWsFrameParse(xbytesview Input, xwsframe* pFrame, const xwsframeconfig* pConfig, xwsframeerrorinfo* pError)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Input` | 输入 | 借用 | 输入数据 |
| `pFrame` | 输出 | 非空 | 接收帧 |
| `pConfig` | 输入 | 允许空 | 帧配置 |
| `pError` | 输出 | 允许空 | 接收错误信息 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWS_FRAME_OK` | 已解析完整帧头 | — |
| `XWS_FRAME_MORE` | 输入不足 | 不设错误 |
| `XWS_FRAME_ERROR` | 帧非法或超限 | `XERR_PROTOCOL` / `XERR_RANGE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（`xrt.ws` 语义） — 帧或扩展语法非法
- `XERR_RANGE` — 帧长超过配置上限

#### 范例

[frame](../../examples/websocket/frame/main.c) · 解析帧

```c
	if ( xrtWsFrameParse(
		Input, &Parsed, &Config, NULL
	) != XWS_FRAME_READY ) {
```

### `xrtWsFrameWrite`

把帧头按配置写入输出缓冲。

```c
bool xrtWsFrameWrite(const xwsframe* pFrame, const xwsframeconfig* pConfig, void* pOutput, size_t iCapacity, size_t* pSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFrame` | 输入 | 非空 | 帧描述 |
| `pConfig` | 输入 | 允许空 | 帧配置 |
| `pOutput` | 输出 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 非空 | 接收写出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量不足，不写半个结果
- `XERR_PROTOCOL`（`xrt.ws` 语义） — 帧或扩展语法非法

#### 范例

[frame](../../examples/websocket/frame/main.c) · 写出帧

```c
	if ( !xrtWsFrameWrite(
		&Frame, &Config, Head, sizeof(Head), &iHeadSize
	) ) {
```

### `xrtWsMask`

按偏移连续应用四字节掩码。

```c
bool xrtWsMask(void* pData, size_t iSize, const uint8 pMask[XWS_MASK_SIZE], uint64 iOffset)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入/输出 | 非空 | 数据 |
| `iSize` | 输入 | — | 字节数 |
| `XWS_MASK_SIZE` | 输入 | — | 四字节掩码 |
| `iOffset` | 输入 | — | 流内偏移 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[frame](../../examples/websocket/frame/main.c) · 掩码异或

```c
	if ( !xrtWsMask(Payload, sizeof(Payload), Mask, 0) ) {
```

### `xrtWsMessageConfigInit`

初始化消息重组的默认配置。

```c
void xrtWsMessageConfigInit(xwsmessageconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 消息配置

```c
	xrtWsMessageConfigInit(&MsgConfig);
```

### `xrtWsMessageConfigInitSafe`

初始化更严格上限的安全消息配置。

```c
void xrtWsMessageConfigInitSafe(xwsmessageconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 安全消息配置

```c
	xrtWsMessageConfigInitSafe(&MsgSafe);
```

### `xrtWsMessageInit`

初始化消息重组状态。

```c
bool xrtWsMessageInit(xwsmessagestate* pState, const xwsmessageconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pState` | 输出 | 非空 | 接收状态 |
| `pConfig` | 输入 | 允许空 | 消息配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 初始化消息

```c
	(void)xrtWsMessageInit(&MsgState, &MsgConfig);
```

### `xrtWsMessageFrameBegin`

把帧计入消息重组状态并产出消息信息。

```c
bool xrtWsMessageFrameBegin(xwsmessagestate* pState, const xwsframe* pFrame, xwsmessageinfo* pInfo, xwsmessageerrorinfo* pError)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pState` | 输入/输出 | 非空 | 消息状态 |
| `pFrame` | 输入 | 非空 | 帧描述 |
| `pInfo` | 输出 | 允许空 | 接收消息信息 |
| `pError` | 输出 | 允许空 | 接收错误信息 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL` — 帧序列违反消息语义（如文本帧后接续帧）
- `XERR_RANGE` — 消息超过配置上限

#### 范例

[message](../../examples/websocket/message/main.c) · 开始帧

```c
	if ( !xrtWsMessageFrameBegin(&State, &Frame, &Info, NULL) ||
		!xrtWsMessagePayload(
			&State,
			(xbytesview) { First, sizeof(First) },
			NULL
		) ||
		!xrtWsMessageFrameEnd(&State, NULL) ) {
```

### `xrtWsMessagePayload`

把帧载荷字节数计入消息状态。

```c
bool xrtWsMessagePayload(xwsmessagestate* pState, xbytesview Payload, xwsmessageerrorinfo* pError)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pState` | 输入/输出 | 非空 | 消息状态 |
| `Payload` | 输入 | 借用 | 帧载荷 |
| `pError` | 输出 | 允许空 | 接收错误信息 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 消息超过配置上限

#### 范例

[message](../../examples/websocket/message/main.c) · 计入载荷

```c
		!xrtWsMessagePayload(
			&State,
			(xbytesview) { First, sizeof(First) },
			NULL
		) ||
```

### `xrtWsMessageFrameEnd`

结束当前帧并推进重组状态。

```c
bool xrtWsMessageFrameEnd(xwsmessagestate* pState, xwsmessageerrorinfo* pError)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pState` | 输入/输出 | 非空 | 消息状态 |
| `pError` | 输出 | 允许空 | 接收错误信息 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL` — 分片未按 FIN 结束

#### 范例

[message](../../examples/websocket/message/main.c) · 结束帧

```c
		!xrtWsMessageFrameEnd(&State, NULL) ) {
```

### `xrtWsMessageReset`

重置消息重组状态以便复用。

```c
void xrtWsMessageReset(xwsmessagestate* pState)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pState` | 输入/输出 | 非空 | 消息状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已重置 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 重置消息

```c
	xrtWsMessageReset(&MsgState);  /* 复位后可复用于新连接 */
```

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

### `xrtWsKeyGenerate`

生成 16 字节 base64 的 Sec-WebSocket-Key。

```c
bool xrtWsKeyGenerate(char* sKey, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sKey` | 输出 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | >= 25 | 容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量不足，不写半个结果
- `XERR_IO` — 系统安全随机源失败

#### 范例

[upgrade](../../examples/websocket/upgrade/main.c) · 生成密钥

```c
	if ( !xrtWsKeyGenerate(Key, sizeof(Key)) ||
		!xrtWsUpgradeRequestFields(
			XRT_STR_LITERAL("example.test"),
			(xstrview) { Key, XWS_KEY_SIZE },
			XRT_STR_LITERAL("chat"),
			(xstrview) { 0 },
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			&iFieldCount
		) || !xrtHttp1RequestWrite(
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/socket"),
```

### `xrtWsKeyValid`

校验 Sec-WebSocket-Key 形态。

```c
bool xrtWsKeyValid(xstrview Key)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Key` | 输入 | 借用 | 密钥文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否合法 | — |

#### 错误

- 无 — 纯校验，不设置错误

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 校验密钥

```c
	if ( !xrtWsKeyValid(SV(EXAMPLE_KEY)) ||
		xrtWsKeyValid(SV("short")) ||
		!xrtWsAcceptValid(SV(EXAMPLE_KEY), SV(EXAMPLE_ACCEPT)) ||
		xrtWsAcceptValid(SV(EXAMPLE_KEY), SV("wrong-wrong-wrong-wrong")) ||
		!xrtWsCloseCodeValid(1000u) ||
		!xrtWsCloseCodeValid(3000u) ||
		xrtWsCloseCodeValid(999u) ||
		xrtWsCloseCodeValid(1005u) ) {
```

### `xrtWsAccept`

由密钥计算 Sec-WebSocket-Accept。

```c
bool xrtWsAccept(xstrview Key, char* sAccept, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Key` | 输入 | 借用 | 密钥文本 |
| `sAccept` | 输出 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | >= 29 | 容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[handshake](../../examples/websocket/handshake/main.c) · 计算 Accept

```c
	if ( !xrtWsAccept(
		XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
		Accept,
		sizeof(Accept)
	) ) {
```

### `xrtWsAcceptValid`

校验服务端 Accept 值与密钥匹配。

```c
bool xrtWsAcceptValid(xstrview Key, xstrview Accept)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Key` | 输入 | 借用 | 密钥文本 |
| `Accept` | 输入 | 借用 | Accept 值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否匹配 | — |

#### 错误

- 不匹配返回 `false` 且不设置错误

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 校验 Accept

```c
		!xrtWsAcceptValid(SV(EXAMPLE_KEY), SV(EXAMPLE_ACCEPT)) ||
```

### `xrtWsCloseCodeValid`

判断关闭码是否在协议允许范围内。

```c
bool xrtWsCloseCodeValid(uint16 iCode)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iCode` | 输入 | — | 关闭码 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否合法 | — |

#### 错误

- 无 — 纯校验，不设置错误

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 校验关闭码

```c
		!xrtWsCloseCodeValid(1000u) ||
```

### `xrtWsCloseParse`

解析关闭帧载荷为代码与原因。

```c
bool xrtWsCloseParse(xbytesview Payload, xwsclose* pClose)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Payload` | 输入 | 借用 | 关闭载荷 |
| `pClose` | 输出 | 非空 | 接收关闭描述 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL` — 载荷长度为 1 或代码/原因非法

#### 范例

[close](../../examples/websocket/close/main.c) · 解析关闭帧

```c
	if ( !xrtWsCloseParse(Input, &Close) ) {
```

### `xrtWsCloseWrite`

把代码与原因写为关闭帧载荷。

```c
bool xrtWsCloseWrite(uint16 iCode, xstrview Reason, void* pOutput, size_t iCapacity, size_t* pSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iCode` | 输入 | — | 关闭码 |
| `Reason` | 输入 | 借用 | 原因文本 |
| `pOutput` | 输出 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 非空 | 接收字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量不足，不写半个结果
- `XERR_PROTOCOL` — 关闭码不允许携带原因

#### 范例

[close](../../examples/websocket/close/main.c) · 写出关闭帧

```c
	if ( !xrtWsCloseWrite(
		XWS_CLOSE_NORMAL,
		XRT_STR_LITERAL("shutdown"),
		Payload,
		sizeof(Payload),
		&iSize
	) ) {
```

### `xrtWsProtocolNext`

遍历逗号分隔的子协议列表。

```c
xhttpnext xrtWsProtocolNext(xstrview Protocols, size_t* pOffset, xstrview* pProtocol)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Protocols` | 输入 | 借用 | 列表文本 |
| `pOffset` | 输入/输出 | 非空 | 偏移游标 |
| `pProtocol` | 输出 | 非空 | 接收子协议视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XHTTP_NEXT_OK` | 已产出一项 | — |
| `XHTTP_NEXT_END` | 遍历结束 | 不设错误 |
| `XHTTP_NEXT_ERROR` | 语法非法 | `XERR_PROTOCOL` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（`xrt.ws` 语义） — 帧或扩展语法非法

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 下一子协议

```c
	while ( xrtWsProtocolNext(SV("chat, superchat"), &iOffset,
			&Protocol) == XHTTP_NEXT_ITEM ) {
```

### `xrtWsProtocolSelect`

在客户端与服务器列表交集中选择子协议。

```c
bool xrtWsProtocolSelect(xstrview ClientProtocols, xstrview ServerProtocols, xstrview* pSelected)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `ClientProtocols` | 输入 | 借用 | 客户端列表 |
| `ServerProtocols` | 输入 | 借用 | 服务器列表 |
| `pSelected` | 输出 | 非空 | 接收选中视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 未命中时返回 `false` 且不设置错误

#### 范例

[handshake](../../examples/websocket/handshake/main.c) · 选择子协议

```c
	if ( !xrtWsProtocolSelect(
		XRT_STR_LITERAL("chat, superchat"),
		XRT_STR_LITERAL("superchat, binary"),
		&Selected
	) ) {
```

### `xrtWsProtocolsHas`

判断列表是否包含指定子协议。

```c
bool xrtWsProtocolsHas(xstrview Protocols, xstrview Protocol)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Protocols` | 输入 | 借用 | 列表文本 |
| `Protocol` | 输入 | 借用 | 子协议 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否包含 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 包含子协议

```c
		!xrtWsProtocolsHas(SV("chat, superchat"), SV("superchat")) ||
```

### `xrtWsProtocolsValid`

校验列表中每个子协议形态合法。

```c
bool xrtWsProtocolsValid(xstrview Protocols)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Protocols` | 输入 | 借用 | 列表文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否全部合法 | — |

#### 错误

- 无 — 纯校验，不设置错误

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 校验子协议列表

```c
	if ( !xrtWsProtocolsValid(SV("chat, superchat")) ||
		xrtWsProtocolsValid(SV("chat,,x")) ||  /* 空项非法 */
		!xrtWsProtocolsHas(SV("chat, superchat"), SV("superchat")) ||
		xrtWsProtocolsHas(SV("chat"), SV("super")) ) {
```

### `xrtWsExtensionCount`

统计扩展列表中的扩展个数。

```c
bool xrtWsExtensionCount(xstrview Extensions, size_t* pCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Extensions` | 输入 | 借用 | 扩展列表文本 |
| `pCount` | 输出 | 非空 | 接收数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（`xrt.ws` 语义） — 帧或扩展语法非法

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 扩展数量

```c
	if ( !xrtWsExtensionCount(SV("permessage-deflate; client_max_window_bits"),
			&iSize) ||
		(iSize != 1u) ||
		!xrtWsExtensionWrite(SV("permessage-deflate"),
			SV("client_max_window_bits=12"),
			ExtText, sizeof(ExtText), &iSize) ||
		(iSize == 0u) ||
		(iSize >= sizeof(ExtText)) ) {
```

### `xrtWsExtensionNext`

遍历扩展列表中的下一个扩展。

```c
xhttpnext xrtWsExtensionNext(xstrview Extensions, size_t* pOffset, xwsextension* pExtension)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Extensions` | 输入 | 借用 | 扩展列表文本 |
| `pOffset` | 输入/输出 | 非空 | 偏移游标 |
| `pExtension` | 输出 | 非空 | 接收扩展 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XHTTP_NEXT_OK` | 已产出一项 | — |
| `XHTTP_NEXT_END` | 遍历结束 | 不设错误 |
| `XHTTP_NEXT_ERROR` | 语法非法 | `XERR_PROTOCOL` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（`xrt.ws` 语义） — 帧或扩展语法非法

#### 范例

[deflate](../../examples/websocket/deflate/main.c) · 下一扩展

```c
	if ( xrtWsExtensionNext(
		Text,
		&iOffset,
		&Extension
	) != XHTTP_NEXT_ITEM ) {
```

### `xrtWsExtensionParamNext`

遍历扩展的下一个参数。

```c
xhttpnext xrtWsExtensionParamNext(const xwsextension* pExtension, size_t* pOffset, xhttpparam* pParam)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pExtension` | 输入 | 非空 | 扩展 |
| `pOffset` | 输入/输出 | 非空 | 偏移游标 |
| `pParam` | 输出 | 非空 | 接收参数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XHTTP_NEXT_OK` | 已产出一项 | — |
| `XHTTP_NEXT_END` | 遍历结束 | 不设错误 |
| `XHTTP_NEXT_ERROR` | 语法非法 | `XERR_PROTOCOL` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（`xrt.ws` 语义） — 帧或扩展语法非法

#### 范例

[extension](../../examples/websocket/extension/main.c) · 下一扩展参数

```c
		while ( xrtWsExtensionParamNext(
			&Extension,
			&iParam,
			&Param
		) == XHTTP_NEXT_ITEM ) {
```

### `xrtWsExtensionWrite`

把扩展名与参数写为列表片段。

```c
bool xrtWsExtensionWrite(xstrview Name, xstrview Parameters, void* pOutput, size_t iCapacity, size_t* pSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Name` | 输入 | 借用 | 扩展名 |
| `Parameters` | 输入 | 借用 | 参数文本 |
| `pOutput` | 输出 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 非空 | 接收字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量不足，不写半个结果
- `XERR_PROTOCOL`（`xrt.ws` 语义） — 帧或扩展语法非法

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 写出扩展

```c
		!xrtWsExtensionWrite(SV("permessage-deflate"),
			SV("client_max_window_bits=12"),
			ExtText, sizeof(ExtText), &iSize) ||
```

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

### `xrtWsUpgradeClientConfigInit`

初始化客户端升级默认配置。

```c
void xrtWsUpgradeClientConfigInit(xwsupgradeclientconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[upgrade_tour](../../examples/websocket/upgrade_tour/main.c) · 客户端升级配置

```c
	xrtWsUpgradeClientConfigInit(&ClientConfig);
```

### `xrtWsUpgradeClientConfigValid`

校验客户端升级配置自洽。

```c
bool xrtWsUpgradeClientConfigValid(const xwsupgradeclientconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空 | 待校验配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否自洽 | — |

#### 错误

- 无 — 纯校验，不设置错误

#### 范例

[upgrade_tour](../../examples/websocket/upgrade_tour/main.c) · 客户端配置校验

```c
	if ( !xrtWsUpgradeClientConfigValid(&ClientConfig) ||
		!xrtWsUpgradeServerConfigValid(&ServerConfig) ||
		/* 清零客户端配置=最小合法形态（无协议无压缩）。 */
		!xrtWsUpgradeClientConfigValid(
			&(xwsupgradeclientconfig) { 0 }) ) {
```

### `xrtWsUpgradeServerConfigInit`

初始化服务端升级默认配置。

```c
void xrtWsUpgradeServerConfigInit(xwsupgradeserverconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[upgrade_tour](../../examples/websocket/upgrade_tour/main.c) · 服务端升级配置

```c
	xrtWsUpgradeServerConfigInit(&ServerConfig);
```

### `xrtWsUpgradeServerConfigValid`

校验服务端升级配置自洽。

```c
bool xrtWsUpgradeServerConfigValid(const xwsupgradeserverconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空 | 待校验配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否自洽 | — |

#### 错误

- 无 — 纯校验，不设置错误

#### 范例

[upgrade_tour](../../examples/websocket/upgrade_tour/main.c) · 服务端配置校验

```c
		!xrtWsUpgradeServerConfigValid(&ServerConfig) ||
```

### `xrtWsUpgradeRequestCheck`

校验 HTTP 升级请求并产出协商结果。

```c
bool xrtWsUpgradeRequestCheck(const xhttp1head* pRequest, const xwsupgradeserverconfig* pConfig, xwsupgrade* pUpgrade)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRequest` | 输入 | 非空 | 请求头 |
| `pConfig` | 输入 | 非空 | 服务端配置 |
| `pUpgrade` | 输出 | 非空 | 接收协商结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL` — 必需字段缺失或形态非法
- `XERR_UNSUPPORTED` — 版本或扩展不受支持

#### 范例

[upgrade_tour](../../examples/websocket/upgrade_tour/main.c) · 校验升级请求

```c
			!xrtWsUpgradeRequestCheck(&Head, &ServerConfig,
				&Upgrade) ) {
```

### `xrtWsUpgradeRequestFields`

把主机、密钥、子协议与扩展写为升级请求字段。

```c
bool xrtWsUpgradeRequestFields(xstrview Host, xstrview Key, xstrview Protocols, xstrview Extensions, xhttpfield* pFields, size_t iCapacity, size_t* pCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Host` | 输入 | 借用 | 主机 |
| `Key` | 输入 | 借用 | 密钥 |
| `Protocols` | 输入 | 借用 | 子协议列表 |
| `Extensions` | 输入 | 借用 | 扩展列表 |
| `pFields` | 输出 | 非空数组 | 接收字段数组 |
| `iCapacity` | 输入 | — | 数组容量 |
| `pCount` | 输出 | 非空 | 接收字段数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 字段数组容量不足
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[upgrade](../../examples/websocket/upgrade/main.c) · 写出请求字段

```c
		!xrtWsUpgradeRequestFields(
			XRT_STR_LITERAL("example.test"),
			(xstrview) { Key, XWS_KEY_SIZE },
			XRT_STR_LITERAL("chat"),
			(xstrview) { 0 },
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			&iFieldCount
		) || !xrtHttp1RequestWrite(
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/socket"),
			XHTTP_VERSION_1_1,
```

### `xrtWsUpgradeResponseCheck`

校验服务端 101 响应与密钥匹配并产出协商结果。

```c
bool xrtWsUpgradeResponseCheck(const xhttp1head* pResponse, xstrview Key, const xwsupgradeclientconfig* pConfig, xwsupgrade* pUpgrade)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResponse` | 输入 | 非空 | 响应头 |
| `Key` | 输入 | 借用 | 客户端密钥 |
| `pConfig` | 输入 | 允许空 | 客户端配置 |
| `pUpgrade` | 输出 | 非空 | 接收协商结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL` — 状态、Accept 或协议不匹配

#### 范例

[upgrade_tour](../../examples/websocket/upgrade_tour/main.c) · 校验升级响应

```c
			!xrtWsUpgradeResponseCheck(&Head,
				(xstrview) { arrKey, strlen(arrKey) },
				&ClientConfig, &ClientUpgrade) ||
```

### `xrtWsUpgradeResponseFields`

把 Accept、子协议与扩展写为升级响应字段。

```c
bool xrtWsUpgradeResponseFields(xstrview Accept, xstrview Protocol, xstrview Extensions, xhttpfield* pFields, size_t iCapacity, size_t* pCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Accept` | 输入 | 借用 | Accept 值 |
| `Protocol` | 输入 | 借用 | 子协议 |
| `Extensions` | 输入 | 借用 | 扩展列表 |
| `pFields` | 输出 | 非空数组 | 接收字段数组 |
| `iCapacity` | 输入 | — | 数组容量 |
| `pCount` | 输出 | 非空 | 接收字段数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 字段数组容量不足

#### 范例

[upgrade_tour](../../examples/websocket/upgrade_tour/main.c) · 写出响应字段

```c
	if ( !xrtWsUpgradeResponseFields(
			(xstrview) { Upgrade.Accept,
				strlen(Upgrade.Accept) },
			Upgrade.Protocol,
			XRT_STR_LITERAL(""),
			ClientFields, 8u, &iCount) ||
		(iCount == 0u) ||
		((iSize = exampleJoin(arrResponse, sizeof(arrResponse),
			"HTTP/1.1 101 Switching Protocols", ClientFields,
			iCount)) == 0u) ) {
```

### `xrtWsUpgradeStreamConfig`

把协商结果转换为对应角色的流配置。

```c
bool xrtWsUpgradeStreamConfig(xwsstreamconfig* pConfig, xwsrole Role, const xwsupgrade* pUpgrade)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收流配置 |
| `Role` | 输入 | — | 角色 |
| `pUpgrade` | 输入 | 非空 | 协商结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[upgrade_tour](../../examples/websocket/upgrade_tour/main.c) · 生成流配置

```c
	if ( !xrtWsUpgradeStreamConfig(&StreamConfig, XWS_ROLE_CLIENT,
			&ClientUpgrade) ) {
```

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

### `xrtWsDeflateInit`

初始化 permessage-deflate 参数为默认提议。

```c
void xrtWsDeflateInit(xwsdeflate* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收参数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 初始化协商

```c
	(void)xrtWsDeflateInit(&Offer);
```

### `xrtWsDeflateIs`

判断扩展是否为 permessage-deflate。

```c
bool xrtWsDeflateIs(const xwsextension* pExtension)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pExtension` | 输入 | 非空 | 扩展 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否 | — |

#### 错误

- 无 — 纯判断，不设置错误

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 识别扩展

```c
	if ( !xrtWsDeflateIs(&Extension) ) {
```

### `xrtWsDeflateOfferParse`

解析客户端提议参数。

```c
bool xrtWsDeflateOfferParse(const xwsextension* pExtension, xwsdeflate* pOffer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pExtension` | 输入 | 非空 | 扩展 |
| `pOffer` | 输出 | 非空 | 接收提议 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（`xrt.ws` 语义） — 帧或扩展语法非法

#### 范例

[deflate](../../examples/websocket/deflate/main.c) · 解析提议

```c
	if ( !xrtWsDeflateOfferParse(
		&Extension,
		&Offer
	) || !xrtWsDeflateAccept(
		&Offer,
		&Response
	) || !xrtWsDeflateResponseWrite(
		&Response,
		Output,
		XWS_DEFLATE_MAX_SIZE,
		&iSize
	) ) {
```

### `xrtWsDeflateOfferWrite`

把提议参数写为扩展列表片段。

```c
bool xrtWsDeflateOfferWrite(const xwsdeflate* pOffer, void* pOutput, size_t iCapacity, size_t* pSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOffer` | 输入 | 非空 | 提议 |
| `pOutput` | 输出 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 非空 | 接收字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 写出提议

```c
	if ( !xrtWsDeflateOfferWrite(&Offer, NULL, 0u, &iSize) ||
		(iSize >= sizeof(OfferText)) ||
		!xrtWsDeflateOfferWrite(&Offer, OfferText,
			sizeof(OfferText), &iSize) ) {
```

### `xrtWsDeflateResponseParse`

解析服务端响应参数。

```c
bool xrtWsDeflateResponseParse(const xwsextension* pExtension, xwsdeflate* pResponse)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pExtension` | 输入 | 非空 | 扩展 |
| `pResponse` | 输出 | 非空 | 接收响应 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（`xrt.ws` 语义） — 帧或扩展语法非法

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 解析响应

```c
		if ( !xrtWsDeflateResponseParse(&RespExt, &Parsed) ||
			(Parsed.ServerMaxWindowBits != 10u) ) {
```

### `xrtWsDeflateResponseWrite`

把响应参数写为扩展列表片段。

```c
bool xrtWsDeflateResponseWrite(const xwsdeflate* pResponse, void* pOutput, size_t iCapacity, size_t* pSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResponse` | 输入 | 非空 | 响应 |
| `pOutput` | 输出 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 非空 | 接收字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[deflate](../../examples/websocket/deflate/main.c) · 写出响应

```c
	) || !xrtWsDeflateResponseWrite(
```

### `xrtWsDeflateResponseCheck`

校验服务端响应与客户端提议兼容。

```c
bool xrtWsDeflateResponseCheck(const xwsdeflate* pOffer, const xwsdeflate* pResponse)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOffer` | 输入 | 非空 | 提议 |
| `pResponse` | 输入 | 非空 | 响应 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL` — 响应与提议不兼容

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 校验响应

```c
	if ( !xrtWsDeflateResponseCheck(&Offer, &Response) ) {
```

### `xrtWsDeflateAccept`

按提议生成兼容的服务端响应参数。

```c
bool xrtWsDeflateAccept(const xwsdeflate* pOffer, xwsdeflate* pResponse)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOffer` | 输入 | 非空 | 提议 |
| `pResponse` | 输出 | 非空 | 接收响应 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_UNSUPPORTED` — 提议参数无法满足

#### 范例

[deflate](../../examples/websocket/deflate/main.c) · 生成响应

```c
	) || !xrtWsDeflateAccept(
```

### `xrtWsDeflateDirection`

按角色与方向推导压缩器/解压器配置参数。

```c
bool xrtWsDeflateDirection(const xwsdeflate* pResponse, xwsrole Role, bool bSend, xwsdeflatedirection* pDirection)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResponse` | 输入 | 非空 | 协商响应 |
| `Role` | 输入 | — | 本端角色 |
| `bSend` | 输入 | — | 是否发送方向 |
| `pDirection` | 输出 | 非空 | 接收方向参数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 方向参数

```c
	if ( !xrtWsDeflateDirection(&Response, XWS_ROLE_SERVER, true,
			&SendDir) ||
		!xrtWsDeflateDirection(&Response, XWS_ROLE_SERVER, false,
			&RecvDir) ||
		(SendDir.WindowBits != 10u) ||
		(RecvDir.WindowBits != 15u) ) {
```

### `xrtWsDeflaterConfigInit`

初始化压缩器默认配置。

```c
void xrtWsDeflaterConfigInit(xwsdeflaterconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[deflater](../../examples/websocket/deflater/main.c) · 压缩器配置

```c
	xrtWsDeflaterConfigInit(&Config);
```

### `xrtWsDeflaterConfigApply`

把协商方向参数应用到配置。

```c
bool xrtWsDeflaterConfigApply(xwsdeflaterconfig* pConfig, const xwsdeflatedirection* pDirection)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 目标配置 |
| `pDirection` | 输入 | 非空 | 方向参数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（`xrt.ws` 语义） — 帧或扩展语法非法

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 应用方向参数

```c
		!xrtWsDeflaterConfigApply(&DeflaterConfig, &SendDir) ) {
```

### `xrtWsDeflaterCreate`

创建 permessage-deflate 压缩器。

```c
xwsdeflater* xrtWsDeflaterCreate(const xwsdeflaterconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 允许空 | 配置，空 = 默认 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 压缩器 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败
- `XERR_UNSUPPORTED` — 压缩后端不可用

#### 范例

[deflater](../../examples/websocket/deflater/main.c) · 创建压缩器

```c
	pDeflater = xrtWsDeflaterCreate(&Config);
```

### `xrtWsDeflaterDestroy`

销毁压缩器及其上下文。

```c
void xrtWsDeflaterDestroy(xwsdeflater* pDeflater)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDeflater` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已销毁 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[deflater](../../examples/websocket/deflater/main.c) · 销毁压缩器

```c
		xrtWsDeflaterDestroy(pDeflater);
```

### `xrtWsDeflaterReset`

按新配置重置压缩器上下文。

```c
bool xrtWsDeflaterReset(xwsdeflater* pDeflater, const xwsdeflaterconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDeflater` | 输入/输出 | 非空 | 目标压缩器 |
| `pConfig` | 输入 | 允许空 | 新配置，空 = 保持 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 重置压缩器

```c
		!xrtWsDeflaterReset(pDeflater, &DeflaterConfig) ) {
```

### `xrtWsDeflaterBegin`

开始一条（可压缩）消息。

```c
bool xrtWsDeflaterBegin(xwsdeflater* pDeflater, bool bCompressed)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDeflater` | 输入/输出 | 非空 | 目标压缩器 |
| `bCompressed` | 输入 | — | 本条是否压缩 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[deflater](../../examples/websocket/deflater/main.c) · 开始消息

```c
		!xrtWsDeflaterBegin(pDeflater, true) ||
```

### `xrtWsDeflaterWrite`

把消息数据写入压缩器。

```c
bool xrtWsDeflaterWrite(xwsdeflater* pDeflater, xbytesview Input, xwsoutputproc pOutput, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDeflater` | 输入/输出 | 非空 | 目标压缩器 |
| `Input` | 输入 | 借用 | 消息数据 |
| `pOutput` | 输入 | 非空 | 输出回调 |
| `pData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 未 Begin
- `XERR_MEMORY` — 内部缓冲扩容失败

#### 范例

[deflater](../../examples/websocket/deflater/main.c) · 写入数据

```c
		!xrtWsDeflaterWrite(
			pDeflater,
			XRT_BYTES_LITERAL("Hello permessage-deflate"),
			onCompressed,
			&iSize
		) ||
```

### `xrtWsDeflaterFlush`

冲刷当前块并同步可用输出。

```c
bool xrtWsDeflaterFlush(xwsdeflater* pDeflater, xwsoutputproc pOutput, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDeflater` | 输入/输出 | 非空 | 目标压缩器 |
| `pOutput` | 输入 | 非空 | 输出回调 |
| `pData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 未 Begin

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 冲刷

```c
		!xrtWsDeflaterFlush(pDeflater, exampleCollect, &Sink) ||
```

### `xrtWsDeflaterEnd`

结束消息并写出尾块。

```c
bool xrtWsDeflaterEnd(xwsdeflater* pDeflater, xwsoutputproc pOutput, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDeflater` | 输入/输出 | 非空 | 目标压缩器 |
| `pOutput` | 输入 | 非空 | 输出回调 |
| `pData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 未 Begin

#### 范例

[deflater](../../examples/websocket/deflater/main.c) · 结束消息

```c
		!xrtWsDeflaterEnd(
			pDeflater,
			onCompressed,
			&iSize
		) ) {
```

### `xrtWsDeflaterAbort`

丢弃未完成消息的压缩状态。

```c
bool xrtWsDeflaterAbort(xwsdeflater* pDeflater)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDeflater` | 输入/输出 | 非空 | 目标压缩器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 中止消息

```c
	if ( !xrtWsDeflaterAbort(pDeflater) ) {
```

### `xrtWsDeflaterSize`

返回压缩器累计输出字节数。

```c
uint64 xrtWsDeflaterSize(const xwsdeflater* pDeflater)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDeflater` | 输入 | 非空 | 目标压缩器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 累计输出字节 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 累计输出

```c
	if ( (xrtWsDeflaterSize(pDeflater) == 0u) ||
		(xrtWsDeflaterSize(pDeflater) != (uint64)Sink.Size) ) {
```

### `xrtWsDeflaterBound`

估算输入尺寸的压缩输出上界。

```c
bool xrtWsDeflaterBound(size_t iInputSize, size_t* pOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iInputSize` | 输入 | — | 输入字节数 |
| `pOutputSize` | 输出 | 非空 | 接收上界 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 输出上界

```c
		!xrtWsDeflaterBound(64u, &iBound) ||
```

### `xrtWsInflaterConfigInit`

初始化解压器默认配置。

```c
void xrtWsInflaterConfigInit(xwsinflaterconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 解压器配置

```c
	(void)xrtWsInflaterConfigInit(&InflaterConfig);
```

### `xrtWsInflaterConfigApply`

把协商方向参数应用到配置。

```c
bool xrtWsInflaterConfigApply(xwsinflaterconfig* pConfig, const xwsdeflatedirection* pDirection)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 目标配置 |
| `pDirection` | 输入 | 非空 | 方向参数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（`xrt.ws` 语义） — 帧或扩展语法非法

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 应用方向参数

```c
	if ( !xrtWsInflaterConfigApply(&InflaterConfig, &RecvDir) ||
		!xrtWsDeflaterConfigApply(&DeflaterConfig, &SendDir) ) {
```

### `xrtWsInflaterCreate`

创建 permessage-deflate 解压器。

```c
xwsinflater* xrtWsInflaterCreate(const xwsinflaterconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 允许空 | 配置，空 = 默认 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 解压器 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败
- `XERR_UNSUPPORTED` — 解压后端不可用

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 创建解压器

```c
	pInflater = xrtWsInflaterCreate(&InflaterConfig);
```

### `xrtWsInflaterDestroy`

销毁解压器及其上下文。

```c
void xrtWsInflaterDestroy(xwsinflater* pInflater)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pInflater` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已销毁 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 销毁解压器

```c
	xrtWsInflaterDestroy(pInflater);
```

### `xrtWsInflaterReset`

按新配置重置解压器上下文。

```c
bool xrtWsInflaterReset(xwsinflater* pInflater, const xwsinflaterconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pInflater` | 输入/输出 | 非空 | 目标解压器 |
| `pConfig` | 输入 | 允许空 | 新配置，空 = 保持 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 重置解压器

```c
	if ( !xrtWsInflaterReset(pInflater, &InflaterConfig) ||
		!xrtWsDeflaterReset(pDeflater, &DeflaterConfig) ) {
```

### `xrtWsInflaterBegin`

开始一条（可能压缩）消息。

```c
bool xrtWsInflaterBegin(xwsinflater* pInflater, bool bCompressed)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pInflater` | 输入/输出 | 非空 | 目标解压器 |
| `bCompressed` | 输入 | — | 本条是否压缩 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[inflater](../../examples/websocket/inflater/main.c) · 开始消息

```c
		!xrtWsInflaterBegin(pInflater, true) ||
```

### `xrtWsInflaterWrite`

把压缩数据写入解压器。

```c
bool xrtWsInflaterWrite(xwsinflater* pInflater, xbytesview Input, xwsoutputproc pOutput, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pInflater` | 输入/输出 | 非空 | 目标解压器 |
| `Input` | 输入 | 借用 | 压缩数据 |
| `pOutput` | 输入 | 非空 | 输出回调 |
| `pData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL` — 压缩流损坏
- `XERR_RANGE` — 解压输出超过配置上限
- `XERR_STATE` — 未 Begin

#### 范例

[inflater](../../examples/websocket/inflater/main.c) · 写入数据

```c
		!xrtWsInflaterWrite(
			pInflater,
			(xbytesview){ Encoded, sizeof(Encoded) },
			onText,
			NULL
		) ||
```

### `xrtWsInflaterEnd`

结束消息并校验尾块。

```c
bool xrtWsInflaterEnd(xwsinflater* pInflater, xwsoutputproc pOutput, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pInflater` | 输入/输出 | 非空 | 目标解压器 |
| `pOutput` | 输入 | 非空 | 输出回调 |
| `pData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL` — 尾块不符合 permessage-deflate 约定
- `XERR_STATE` — 未 Begin

#### 范例

[inflater](../../examples/websocket/inflater/main.c) · 结束消息

```c
		!xrtWsInflaterEnd(pInflater, onText, NULL) ) {
```

### `xrtWsInflaterSize`

返回解压器累计输出字节数。

```c
uint64 xrtWsInflaterSize(const xwsinflater* pInflater)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pInflater` | 输入 | 非空 | 目标解压器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 累计输出字节 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[extension_tour](../../examples/websocket/extension_tour/main.c) · 累计输出

```c
	if ( xrtWsInflaterSize(pInflater) != 0u ) {
```

## 所有权与错误

帧、消息、Close、握手和扩展解析器只借用输入视图；成功后也不会接管输入。所有写出函数使用
调用方缓冲，并在容量不足时返回所需长度，不留下半个结构。Upgrade 结果拥有 Accept 与扩展响应，
子协议暂时借用 HTTP Header；Attach 会在消费 Header 前复制它。Inflater、Deflater 和 Stream
持有资源，必须由对应 `Destroy` 释放。

协议错误通过稳定的模块错误码和 `xrtGetError`/结构化错误链表达。输入不足不是协议错误；
只有确定违反协议、参数范围、资源上限、输出回调或压缩状态时才进入失败路径。

## 模块契约：线程

帧、消息与扩展协商 API 为无共享状态的纯函数，可任意线程并发调用；流对象与压缩/解压上下文为单拥有者对象，绑定其所属 Worker 或创建线程。

## 扩展能力

连接对象、Writer、Future/协程桥接、重连与心跳策略、连接组和广播属于独立发布的 `xws`
扩展。基础 TCP/TLS 通信、关闭、背压和连接级压缩留在 XRT；`xws` 在这条稳定路径上提供高级抽象。

### `xrtWsStreamConfigInit`

初始化流层默认配置。

```c
void xrtWsStreamConfigInit(xwsstreamconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 流配置

```c
	xrtWsStreamConfigInit(&pEnd->Config);
```

### `xrtWsStreamConfigValid`

校验流层配置自洽。

```c
bool xrtWsStreamConfigValid(const xwsstreamconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空 | 待校验配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否自洽 | — |

#### 错误

- 无 — 纯校验，不设置错误

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 流配置校验

```c
	if ( !xrtWsStreamConfigValid(&Client.Config) ) goto Cleanup;
```

### `xrtWsStreamAttach`

在已就绪的 TCP 流上挂接 WebSocket 流层。

```c
xwsstream* xrtWsStreamAttach(xnetstream* pTransport, size_t iPrefix, const xwsstreamconfig* pConfig, const xwsstreamevents* pEvents, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTransport` | 输入 | 非空 | 传输 TCP 流 |
| `iPrefix` | 输入 | — | 前缀已消费字节 |
| `pConfig` | 输入 | 允许空 | 流配置 |
| `pEvents` | 输入 | 允许空 | 事件表 |
| `pData` | 输入 | 任意值 | 用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | WebSocket 流（引用 1） | — |
| `NULL` | 挂接失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 传输流非开放状态
- `XERR_MEMORY` — 分配失败

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 挂接 TCP

```c
	pEnd->pStream = xrtWsStreamAttach(pEnd->pTcp, 0,
		&pEnd->Config, &Events, pEnd);
```

### `xrtWsStreamAttachTls`

在已就绪的 TLS 流上挂接 WebSocket 流层。

```c
xwsstream* xrtWsStreamAttachTls(xtlsstream* pTransport, size_t iPrefix, const xwsstreamconfig* pConfig, const xwsstreamevents* pEvents, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTransport` | 输入 | 非空 | 传输 TLS 流 |
| `iPrefix` | 输入 | — | 前缀已消费字节 |
| `pConfig` | 输入 | 允许空 | 流配置 |
| `pEvents` | 输入 | 允许空 | 事件表 |
| `pData` | 输入 | 任意值 | 用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | WebSocket 流（引用 1） | — |
| `NULL` | 挂接失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 传输流非开放状态
- `XERR_MEMORY` — 分配失败

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 挂接 TLS

```c
			(xrtWsStreamAttachTls(NULL, 0, NULL, NULL, NULL) == NULL);
```

### `xrtWsStreamRef`

增加流引用并返回原指针。

```c
xwsstream* xrtWsStreamRef(xwsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 原指针，引用 +1 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 共享引用

```c
	pClientRef = xrtWsStreamRef(Client.pStream);
```

### `xrtWsStreamDestroy`

释放流引用；关闭必须另行请求。

```c
void xrtWsStreamDestroy(xwsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用 -1 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 释放引用

```c
		xrtWsStreamDestroy(pEnd->pStream);
```

### `xrtWsStreamSend`

按操作码发送原始帧载荷。

```c
xnetresult xrtWsStreamSend(xwsstream* pStream, xwsopcode Opcode, xbytesview Payload)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |
| `Opcode` | 输入 | — | 操作码 |
| `Payload` | 输入 | 借用 | 载荷 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送预算已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.ws` / `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 发送预算已满
- `XERR_STATE` — 握手未完成或已关闭

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 发送帧

```c
		(xrtWsStreamSend(pStream, pEnd->Opcode,
			(xbytesview) { (cbytes)pEnd->Buffer, pEnd->Size }) !=
			XNET_RESULT_OK) ) {
```

### `xrtWsStreamSendCompressed`

按操作码压缩发送帧载荷。

```c
xnetresult xrtWsStreamSendCompressed(xwsstream* pStream, xwsopcode Opcode, xbytesview Payload)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |
| `Opcode` | 输入 | — | 操作码 |
| `Payload` | 输入 | 借用 | 载荷 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送预算已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.ws` / `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 发送预算已满
- `XERR_STATE` — 握手未完成或已关闭

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 压缩发送帧

```c
		(xrtWsStreamSendCompressed(pJob->pStream, XWS_OPCODE_TEXT,
			(xbytesview) { (cbytes)"c-s", 3 }) == XNET_RESULT_OK);
```

### `xrtWsStreamSendRef`

零复制发送帧载荷，离开队列时执行释放过程。

```c
xnetresult xrtWsStreamSendRef(xwsstream* pStream, xwsopcode Opcode, const xnetref* pRef)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |
| `Opcode` | 输入 | — | 操作码 |
| `pRef` | 输入 | 非空 | 引用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送预算已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.ws` / `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 发送预算已满
- `XERR_STATE` — 握手未完成或已关闭

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 零复制发送帧

```c
	if ( xrtWsStreamSendRef(pStream, XWS_OPCODE_BINARY, &Ref) !=
		XNET_RESULT_OK ) goto Done;
```

### `xrtWsStreamSendTake`

接管 XRT 分配的载荷并发送。

```c
xnetresult xrtWsStreamSendTake(xwsstream* pStream, xwsopcode Opcode, ptr pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |
| `Opcode` | 输入 | — | 操作码 |
| `pData` | 输入 | 允许空 | 拥有的数据 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送预算已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.ws` / `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 发送预算已满
- `XERR_STATE` — 握手未完成或已关闭
- 受理失败时所有权不转移

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 接管发送帧

```c
	if ( xrtWsStreamSendTake(pStream, XWS_OPCODE_TEXT, pTake, 8) !=
		XNET_RESULT_OK ) goto Done;
```

### `xrtWsStreamText`

发送 UTF-8 文本消息。

```c
xnetresult xrtWsStreamText(xwsstream* pStream, xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |
| `Text` | 输入 | 借用、严格 UTF-8 | 文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送预算已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.ws` / `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 发送预算已满
- `XERR_STATE` — 握手未完成或已关闭
- `XERR_PROTOCOL` — 文本不是合法 UTF-8

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 发送文本

```c
	if ( (xrtWsStreamText(pStream, XRT_STR_LITERAL("hello ws")) !=
			XNET_RESULT_OK) ||
		(xrtWsStreamBinary(pStream,
			(xbytesview) { (cbytes)"\x01\x02\x03", 3 }) != XNET_RESULT_OK) ||
		(xrtWsStreamSend(pStream, XWS_OPCODE_TEXT,
			(xbytesview) { (cbytes)"plain-send", 10 }) != XNET_RESULT_OK) ) {
```

### `xrtWsStreamTextCompressed`

压缩发送 UTF-8 文本消息。

```c
xnetresult xrtWsStreamTextCompressed(xwsstream* pStream, xstrview Text)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |
| `Text` | 输入 | 借用、严格 UTF-8 | 文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送预算已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.ws` / `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 发送预算已满
- `XERR_STATE` — 握手未完成或已关闭
- `XERR_PROTOCOL` — 文本不是合法 UTF-8

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 压缩发送文本

```c
	pJob->Ok = (xrtWsStreamTextCompressed(pJob->pStream,
		XRT_STR_LITERAL("compressed")) == XNET_RESULT_OK) &&
```

### `xrtWsStreamTextRef`

零复制发送文本消息。

```c
xnetresult xrtWsStreamTextRef(xwsstream* pStream, const xnetref* pRef)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |
| `pRef` | 输入 | 非空 | 引用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送预算已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.ws` / `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 发送预算已满
- `XERR_STATE` — 握手未完成或已关闭

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 零复制发送文本

```c
	if ( xrtWsStreamTextRef(pStream, &Ref) != XNET_RESULT_OK ) goto Done;
```

### `xrtWsStreamTextTake`

接管 XRT 分配的文本并发送。

```c
xnetresult xrtWsStreamTextTake(xwsstream* pStream, str sText, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |
| `sText` | 输入 | 允许空 | 拥有的文本 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送预算已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.ws` / `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 发送预算已满
- `XERR_STATE` — 握手未完成或已关闭
- 受理失败时所有权不转移

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 接管发送文本

```c
	if ( xrtWsStreamTextTake(pStream, pTake, 9) != XNET_RESULT_OK ) goto Done;
```

### `xrtWsStreamBinary`

发送二进制消息。

```c
xnetresult xrtWsStreamBinary(xwsstream* pStream, xbytesview Data)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |
| `Data` | 输入 | 借用 | 数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送预算已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.ws` / `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 发送预算已满
- `XERR_STATE` — 握手未完成或已关闭

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 发送二进制

```c
		(xrtWsStreamBinary(pStream,
			(xbytesview) { (cbytes)"\x01\x02\x03", 3 }) != XNET_RESULT_OK) ||
```

### `xrtWsStreamBinaryCompressed`

压缩发送二进制消息。

```c
xnetresult xrtWsStreamBinaryCompressed(xwsstream* pStream, xbytesview Data)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |
| `Data` | 输入 | 借用 | 数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送预算已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.ws` / `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 发送预算已满
- `XERR_STATE` — 握手未完成或已关闭

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 压缩发送二进制

```c
		(xrtWsStreamBinaryCompressed(pJob->pStream,
			(xbytesview) { (cbytes)"c-b", 3 }) == XNET_RESULT_OK) &&
```

### `xrtWsStreamBinaryRef`

零复制发送二进制消息。

```c
xnetresult xrtWsStreamBinaryRef(xwsstream* pStream, const xnetref* pRef)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |
| `pRef` | 输入 | 非空 | 引用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送预算已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.ws` / `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 发送预算已满
- `XERR_STATE` — 握手未完成或已关闭

#### 范例

[stream_ref](../../examples/websocket/stream_ref/main.c) · 零复制发送二进制

```c
	Result = xrtWsStreamBinaryRef(pConnection, &Ref);
```

### `xrtWsStreamBinaryTake`

接管 XRT 分配的数据并发送。

```c
xnetresult xrtWsStreamBinaryTake(xwsstream* pStream, bytes pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |
| `pData` | 输入 | 允许空 | 拥有的数据 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送预算已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.ws` / `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 发送预算已满
- `XERR_STATE` — 握手未完成或已关闭
- 受理失败时所有权不转移

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 接管发送二进制

```c
	if ( xrtWsStreamBinaryTake(pStream, pTake, 10) != XNET_RESULT_OK ) goto Done;
```

### `xrtWsStreamPing`

发送 Ping 控制帧。

```c
xnetresult xrtWsStreamPing(xwsstream* pStream, xbytesview Payload)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |
| `Payload` | 输入 | 借用、<= 125 字节 | 载荷 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送预算已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.ws` / `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 发送预算已满
- `XERR_STATE` — 握手未完成或已关闭
- `XERR_RANGE` — 控制帧载荷超过 125 字节

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 发送 Ping

```c
	pJob->Ok = (xrtWsStreamPing(pStream,
		(xbytesview) { (cbytes)"ping", 4 }) == XNET_RESULT_OK) &&
```

### `xrtWsStreamPong`

发送 Pong 控制帧。

```c
xnetresult xrtWsStreamPong(xwsstream* pStream, xbytesview Payload)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |
| `Payload` | 输入 | 借用、<= 125 字节 | 载荷 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送预算已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.ws` / `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 发送预算已满
- `XERR_STATE` — 握手未完成或已关闭
- `XERR_RANGE` — 控制帧载荷超过 125 字节

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 发送 Pong

```c
		(xrtWsStreamPong(pStream,
			(xbytesview) { (cbytes)"manual", 6 }) == XNET_RESULT_OK);
```

### `xrtWsStreamClose`

发送 Close 帧并进入关闭握手。

```c
xnetresult xrtWsStreamClose(xwsstream* pStream, uint16 iCode, xstrview Reason)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |
| `iCode` | 输入 | — | 关闭码 |
| `Reason` | 输入 | 借用 | 原因文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_OK` | 已受理 | — |
| `XNET_AGAIN` | 发送预算已满 | `XERR_AGAIN` |
| `XNET_ERROR` | 失败 | `xrt.ws` / `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 发送预算已满
- `XERR_STATE` — 握手未完成或已关闭
- `XERR_PROTOCOL` — 关闭码非法

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 发送关闭

```c
	pJob->Ok = xrtWsStreamClose(pJob->pStream, 1000,
		XRT_STR_LITERAL("done")) == XNET_RESULT_OK;
```

### `xrtWsStreamPause`

暂停接收新数据。

```c
void xrtWsStreamPause(xwsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已暂停 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 暂停接收

```c
	xrtWsStreamPause(Client.pStream);
```

### `xrtWsStreamResume`

恢复接收并唤醒所属 Worker。

```c
bool xrtWsStreamResume(xwsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已恢复 | — |
| `false` | 状态非法 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 未处于暂停状态

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 恢复接收

```c
	if ( !xrtWsStreamResume(Client.pStream) ||
		!exampleWait(&Client.Received, iExpected + 11u) ||
		(memcmp(Client.Buffer + iExpected, "after-pause", 11u) != 0) ) goto Cleanup;
```

### `xrtWsStreamState`

返回流当前生命周期状态。

```c
xwsstreamstate xrtWsStreamState(const xwsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 状态枚举值 | 当前状态 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 状态

```c
		(xrtWsStreamState(Client.pStream) != XWS_STREAM_CLOSED) ||
```

### `xrtWsStreamRole`

返回本端协商角色。

```c
xwsrole xrtWsStreamRole(const xwsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XWS_CLIENT` / `XWS_SERVER` | 本端角色 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 角色

```c
	if ( xrtWsStreamRole(pStream) == XWS_ROLE_SERVER ) pEnd->Size = 0;
```

### `xrtWsStreamProtocol`

返回协商选中的子协议。

```c
xstrview xrtWsStreamProtocol(const xwsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 视图 | 子协议借用 | — |
| 空视图 | 未协商 | 不设错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 子协议

```c
		(xrtWsStreamProtocol(Client.pStream).Size != 0) ||
```

### `xrtWsStreamDeflate`

复制本流协商的压缩参数。

```c
bool xrtWsStreamDeflate(const xwsstream* pStream, xwsdeflate* pDeflate)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |
| `pDeflate` | 输出 | 非空 | 接收参数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 未启用压缩 | 不设错误 |

#### 错误

- 未启用 permessage-deflate 返回 `false` 且不设置错误

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 压缩协商

```c
		if ( !xrtWsStreamDeflate(Client.pStream, &Deflate) ||
			!exampleRun(pEngine, &Job, Client.pStream, exampleCompressedTask) ||
			!exampleWait(&Client.Received, iExpected + 16u) ||
			(memcmp(Client.Buffer + iExpected, "compressedc-bc-s", 16u) != 0) ) goto Cleanup;
```

### `xrtWsStreamCloseInfo`

复制对端 Close 帧信息。

```c
bool xrtWsStreamCloseInfo(const xwsstream* pStream, xwsstreamclose* pClose)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |
| `pClose` | 输出 | 非空 | 接收关闭描述 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已复制 | — |
| `false` | 未收到 Close | 不设错误 |

#### 错误

- 未收到 Close 帧返回 `false` 且不设置错误

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 关闭信息

```c
		!xrtWsStreamCloseInfo(Client.pStream, &CloseInfo) ||
```

### `xrtWsStreamError`

返回导致流终止的借用错误。

```c
const xerror* xrtWsStreamError(const xwsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 终止原因借用 | — |
| `NULL` | 正常关闭或未终止 | 不设错误 |

#### 错误

- 无错误 — 正常关闭或未终止返回 `NULL` 且不设置错误

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 终止错误

```c
		(xrtWsStreamError(Client.pStream) != NULL) ||
```

### `xrtWsStreamAbort`

立即丢弃发送队列并异常关闭。

```c
bool xrtWsStreamAbort(xwsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已关闭 | — |
| `false` | 状态非法 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 已关闭

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 异常关闭

```c
		(void)xrtWsStreamAbort(pStream);
```

### `xrtWsStreamPending`

返回发送队列占用字节数。

```c
size_t xrtWsStreamPending(const xwsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 占用字节数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 发送占用

```c
	while ( (xrtWsStreamPending(Client.pStream) != 0u) ||
		(xrtWsStreamPending(Server.pStream) != 0u) ) {
```

### `xrtWsStreamWritable`

返回剩余发送预算。

```c
size_t xrtWsStreamWritable(const xwsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 剩余预算 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 剩余预算

```c
	if ( (xrtWsStreamWritable(Client.pStream) == 0u) ||
		(xrtAtomic32Load(&Client.Received, XMEMORY_ACQUIRE) != iExpected) ||
		!exampleRun(pEngine, &Job, Client.pStream, exampleCloseTask) ||
		!exampleWait(&Client.Closed, 1) || !exampleWait(&Server.Closed, 1) ||
		!xrtWsStreamCloseInfo(Client.pStream, &CloseInfo) ||
		((CloseInfo.Flags & (XWS_STREAM_CLOSE_SENT | XWS_STREAM_CLOSE_RECEIVED |
			XWS_STREAM_CLOSE_CLEAN)) != (XWS_STREAM_CLOSE_SENT |
			XWS_STREAM_CLOSE_RECEIVED | XWS_STREAM_CLOSE_CLEAN)) ||
		(CloseInfo.RemoteCode != 1000u) ||
		(xrtWsStreamError(Client.pStream) != NULL) ||
		(xrtWsStreamError(Server.pStream) != NULL) ||
		(xrtAtomic32Load(&Client.Errors, XMEMORY_ACQUIRE) != 0) ||
```

### `xrtWsStreamPaused`

判断是否处于暂停状态。

```c
bool xrtWsStreamPaused(const xwsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否暂停 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 暂停查询

```c
	if ( !xrtWsStreamPaused(Client.pStream) ||
		!exampleRun(pEngine, &Job, Server.pStream, exampleAfterTask) ) goto Cleanup;
```

### `xrtWsStreamWorker`

返回流所属的借用 Worker。

```c
xnetworker* xrtWsStreamWorker(const xwsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Worker 借用 | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · 所属 Worker

```c
		xrtNetWorkerIndex(xrtWsStreamWorker(pStream)), Proc, pJob) &&
```

### `xrtWsStreamTcp`

返回底层 TCP 流借用（不增引用）。

```c
xnetstream* xrtWsStreamTcp(const xwsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | TCP 流借用 | — |
| `NULL` | 非 TCP 传输 | 不设错误 |

#### 错误

- 非 TCP 挂接返回 `NULL` 且不设置错误

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · TCP 借用

```c
	pJob->Ok = (pTcp != NULL) && (xrtWsStreamTcp(pJob->pStream) == pTcp);
```

### `xrtWsStreamTcpRef`

返回底层 TCP 流并增加引用。

```c
xnetstream* xrtWsStreamTcpRef(const xwsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | TCP 流新引用 | — |
| `NULL` | 非 TCP 传输 | 不设错误 |

#### 错误

- 非 TCP 挂接返回 `NULL` 且不设置错误

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · TCP 引用

```c
	xnetstream* pTcp = xrtWsStreamTcpRef(pJob->pStream);
```

### `xrtWsStreamTls`

返回底层 TLS 流借用（不增引用）。

```c
xtlsstream* xrtWsStreamTls(const xwsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | TLS 流借用 | — |
| `NULL` | 非 TLS 传输 | 不设错误 |

#### 错误

- 非 TLS 挂接返回 `NULL` 且不设置错误

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · TLS 借用

```c
			(xrtWsStreamTls(pJob->pStream) == NULL) &&
```

### `xrtWsStreamTlsRef`

返回底层 TLS 流并增加引用。

```c
xtlsstream* xrtWsStreamTlsRef(const xwsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入 | 非空 | 目标流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | TLS 流新引用 | — |
| `NULL` | 非 TLS 传输 | 不设错误 |

#### 错误

- 非 TLS 挂接返回 `NULL` 且不设置错误

#### 范例

[stream_tour](../../examples/websocket/stream_tour/main.c) · TLS 引用

```c
		xtlsstream* pTls = xrtWsStreamTlsRef(pJob->pStream);
```

## 测试与示例

`tests/websocket/` 覆盖固定向量、分块矩阵、属性测试、no-allocation、OOM、协议模糊测试，以及
真实 Select 回环上的 `ws`/`wss` Upgrade。集成测试会拆分 TLS Header 和一字节帧头，并验证压缩
发送、解压接收和完整 Close。`examples/websocket/` 不依赖高级连接对象。
