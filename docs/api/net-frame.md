# 网络 Framing API

## 类型与常量

### `xnetframestatus`

增量 framing 只区分失败、等待更多字节和完整帧。

```c
typedef enum xnetframestatus {
	XNET_FRAME_ERROR = -1,
	XNET_FRAME_MORE = 0,
	XNET_FRAME_READY = 1
} xnetframestatus;
```

| 值 | 语义 |
|---|---|
| `XNET_FRAME_ERROR` | 失败 |
| `XNET_FRAME_MORE` | 需要更多输入 |

### `xnetframe`

所有偏移都相对当前输入头部，Declared 保存协议字段原值。

```c
typedef struct xnetframe {
	size_t PayloadOffset;
	size_t PayloadSize;
	size_t FrameSize;
	uint64 Declared;
} xnetframe;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `PayloadOffset` | `size_t` | PayloadOffset |
| `PayloadSize` | `size_t` | PayloadSize |
| `FrameSize` | `size_t` | FrameSize |
| `Declared` | `uint64` | Declared |

### `xnetlineconfig`

分隔符只借用调用方字节，并且必须存活到 Framer 不再使用。

```c
typedef struct xnetlineconfig {
	xbytesview Delimiter;
	size_t MaxPayload;
	bool IncludeDelimiter;
} xnetlineconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Delimiter` | `xbytesview` | 分隔符 |
| `MaxPayload` | `size_t` | MaxPayload |
| `IncludeDelimiter` | `bool` | IncludeDelimiter |

### `xnetlineframer`

Line Framer 保存块内增量游标，字段公开只用于无分配栈存储。

```c
typedef struct xnetlineframer {
	xnetlineconfig Config;
	const xnetbuf* Input;
	xnetblock* Cursor;
	size_t CursorOffset;
	size_t Search;
	size_t PreviousSize;
	uint32 Guard;
} xnetlineframer;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Config` | `xnetlineconfig` | 配置 |
| `Input` | `const xnetbuf*` | 输入视图 |
| `Cursor` | `xnetblock*` | Cursor |
| `CursorOffset` | `size_t` | CursorOffset |
| `Search` | `size_t` | Search |
| `PreviousSize` | `size_t` | PreviousSize |
| `Guard` | `uint32` | 守卫字（防误用） |

### `xnetframeorder`

长度字段字节序独立于主机字节序。

```c
typedef enum xnetframeorder {
	XNET_FRAME_BIG_ENDIAN = 0,
	XNET_FRAME_LITTLE_ENDIAN = 1
} xnetframeorder;
```

| 值 | 语义 |
|---|---|
| `XNET_FRAME_BIG_ENDIAN` | XNETFRAMEBIGENDIAN |

### `xnetlengthconfig`

FrameSize = LengthOffset + LengthSize + Declared + Adjustment。

```c
typedef struct xnetlengthconfig {
	size_t LengthOffset;
	size_t LengthSize;
	int64 Adjustment;
	size_t Strip;
	size_t MaxFrame;
	xnetframeorder Order;
} xnetlengthconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `LengthOffset` | `size_t` | LengthOffset |
| `LengthSize` | `size_t` | LengthSize |
| `Adjustment` | `int64` | Adjustment |
| `Strip` | `size_t` | Strip |
| `MaxFrame` | `size_t` | MaxFrame |
| `Order` | `xnetframeorder` | 字节序 |

### `xnetlengthframer`

Length Framer 复制并验证配置，解析本身不保存输入状态。

```c
typedef struct xnetlengthframer {
	xnetlengthconfig Config;
	uint32 Guard;
} xnetlengthframer;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Config` | `xnetlengthconfig` | 配置 |
| `Guard` | `uint32` | 守卫字（防误用） |

## 分层与裁剪

- `XRT_FEATURE_NET_FRAME`：统一帧描述、payload 复制和精确消费，只依赖 `NET_BUFFER`。
- `XRT_FEATURE_NET_FRAME_LINE`：任意长度分隔符的增量行 framing，只依赖 `NET_FRAME`。
- `XRT_FEATURE_NET_FRAME_LENGTH`：1 到 8 字节长度字段 framing，只依赖 `NET_FRAME`。

HTTP/1 和 WebSocket 有各自的协议状态机，不经过通用 framing vtable。这里用于 TCP 上的文本协议、长度前缀 RPC、自定义消息协议和协议探测层。

## 通用帧

```c
typedef enum xnetframestatus {
	XNET_FRAME_ERROR = -1,
	XNET_FRAME_MORE = 0,
	XNET_FRAME_READY = 1
} xnetframestatus;

typedef struct xnetframe {
	size_t PayloadOffset;
	size_t PayloadSize;
	size_t FrameSize;
	uint64 Declared;
} xnetframe;
```

帧只借用当前 `xnetbuf`。`PayloadOffset` 和 `FrameSize` 都相对缓冲头部；输入前缀被消费、替换或重排后，调用方不得继续使用旧帧。`xrtNetFrameCopy` 最多复制输出容量个 payload 字节，适合小消息便利路径；零拷贝用户可直接使用 `xrtNetBufSpans`、`xrtNetBufPeek` 和偏移。`xrtNetFrameConsume` 会验证当前输入仍能完整容纳该范围，再精确消费 `FrameSize` 字节。

### `xrtNetFrameCopy`

从完整帧复制不超过输出容量的 payload 字节，返回实际复制数。

```c
size_t xrtNetFrameCopy(
	const xnetbuf* pInput,
	const xnetframe* pFrame,
	void* pOutput,
	size_t iCapacity
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pInput` | 输入 | 非空 | 帧所在的输入缓冲 |
| `pFrame` | 输入 | 非空 | `Next` 产出的帧描述 |
| `pOutput` | 输出 | 容量非零时非空 | 接收 payload |
| `iCapacity` | 输入 | — | 输出容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 实际复制字节数（不超过 payload 与容量的较小者）；0 = 失败或空 payload | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空，或容量非零但输出为空
- `xrt.net` / `XNET_ERROR_FRAME_STATE`（`XERR_STATE`） — 帧范围越出当前输入头部

#### 范例

[frame_line](../../examples/network/frame_line/main.c) · 复制 payload

```c
		size_t iSize = xrtNetFrameCopy(
			&Input, &Frame, sLine, sizeof(sLine) - 1u
		);
```

### `xrtNetFrameConsume`

只有帧范围仍完整位于输入头部时才精确消费 `FrameSize` 字节。

```c
bool xrtNetFrameConsume(
	xnetbuf* pInput,
	const xnetframe* pFrame
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pInput` | 输入/输出 | 非空 | 要消费的输入缓冲 |
| `pFrame` | 输入 | 非空 | `Next` 产出的帧描述 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已从输入头部移除整帧 | — |
| `false` | 帧越出当前输入 | `XERR_STATE` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `xrt.net` / `XNET_ERROR_FRAME_STATE`（`XERR_STATE`） — 帧范围越出当前输入头部

#### 范例

[frame_line](../../examples/network/frame_line/main.c) · 消费帧

```c
		if ( !xrtNetFrameConsume(&Input, &Frame) ) {
```

## 行分隔帧

```c
typedef struct xnetlineconfig {
	xbytesview Delimiter;
	size_t MaxPayload;
	bool IncludeDelimiter;
} xnetlineconfig;
```

`xrtNetLineConfigInit` 默认使用 LF、8192 字节 payload 上限并从 payload 去除分隔符。Delimiter 是借用视图，没有固定长度上限，必须在 Framer 使用期间保持存活且内容不变。`MaxPayload` 只限制分隔符之前的字节；需要无界模式时显式设为 `SIZE_MAX`。

`xrtNetLineNext` 返回 `MORE` 后，调用方只能保留原输入前缀并在同一个 `xnetbuf` 尾部追加；消费、替换、换用另一缓冲或重排输入前应调用 `xrtNetLineReset`。Framer 保存块指针、块内偏移和未决候选位置，因此即使逐次追加大量单字节引用块，也不会为定位旧偏移反复遍历链头。分隔符跨缓冲块、自重叠和位于 payload 上限的情况都受支持；分隔符开始位置超过上限立即返回 `XNET_FRAME_ERROR` 和 `XNET_ERROR_FRAME_LIMIT`。

完整示例位于 `examples/network/frame_line/main.c`。

### `xrtNetLineConfigInit`

初始化 LF 分隔、8192 字节 payload 上限和去除分隔符的默认配置。

```c
void xrtNetLineConfigInit(xnetlineconfig* pConfig)
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

[frame_line](../../examples/network/frame_line/main.c) · 默认配置

```c
	xrtNetLineConfigInit(&Config);
```

### `xrtNetLineInit`

复制配置并开始一条新的增量行帧搜索。

```c
bool xrtNetLineInit(
	xnetlineframer* pFramer,
	const xnetlineconfig* pConfig
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFramer` | 输出 | 非空 | 接收搜索器 |
| `pConfig` | 输入 | 非空 | 行帧配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 配置非法 | `xrt.net` 域错误 |

#### 错误

- `xrt.net` / `XNET_ERROR_FRAME_CONFIG`（`XERR_ARGUMENT`） — 分隔符为空、上限为 0 或成员组合非法

#### 范例

[frame_line](../../examples/network/frame_line/main.c) · 初始化

```c
	if ( !xrtNetLineInit(&Framer, &Config) ||
		 !xrtNetBufInit(&Input, NULL) ||
		 !xrtNetBufAppend(&Input, "first\r", 6) ) {
```

### `xrtNetLineReset`

保留配置并丢弃当前增量搜索进度。

```c
bool xrtNetLineReset(xnetlineframer* pFramer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFramer` | 输入/输出 | 已初始化 | 目标搜索器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 进度已清空，配置保留 | — |
| `false` | 搜索器状态非法 | `XERR_STATE` 域错误 |

#### 错误

- `xrt.net` / `XNET_ERROR_FRAME_STATE`（`XERR_STATE`） — 搜索器未初始化或守卫字被破坏

#### 范例

[frame_line](../../examples/network/frame_line/main.c) · 重置进度

```c
	(void)xrtNetLineReset(&Framer);
```

### `xrtNetLineNext`

解析输入头部的下一条分隔帧；返回 `MORE` 后只允许保留前缀并追加输入。

```c
xnetframestatus xrtNetLineNext(
	xnetlineframer* pFramer,
	const xnetbuf* pInput,
	xnetframe* pFrame
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFramer` | 输入/输出 | 已初始化 | 目标搜索器 |
| `pInput` | 输入 | 非空 | 输入缓冲（`MORE` 后须保留未决前缀） |
| `pFrame` | 输出 | 非空 | 接收帧描述 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_FRAME_READY` | 已解析一条完整帧，`pFrame` 有效 | — |
| `XNET_FRAME_MORE` | 输入不足，需保留未决前缀并追加数据 | 不设错误 |
| `XNET_FRAME_ERROR` | 参数非法、状态破坏或帧超限 | `XERR_ARGUMENT` / `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `xrt.net` / `XNET_ERROR_FRAME_STATE`（`XERR_STATE`） — 搜索器状态非法或违反 `MORE` 后的前缀保留契约
- `xrt.net` / `XNET_ERROR_FRAME_LIMIT`（`XERR_RANGE`） — 行 payload 超过配置上限

#### 范例

[frame_line](../../examples/network/frame_line/main.c) · 解析下一行

```c
	if ( xrtNetLineNext(&Framer, &Input, &Frame) != XNET_FRAME_MORE ||
		 !xrtNetBufAppend(&Input, "\nsecond\r\n", 10) ) {
```

## 长度前缀帧

```c
typedef struct xnetlengthconfig {
	size_t LengthOffset;
	size_t LengthSize;
	int64 Adjustment;
	size_t Strip;
	size_t MaxFrame;
	xnetframeorder Order;
} xnetlengthconfig;
```

帧总长按以下公式计算：

```text
FrameSize = LengthOffset + LengthSize + Declared + Adjustment
```

`LengthSize` 支持 1 到 8 字节，`Order` 明确选择大小端。`Strip` 指定 payload 起点，可以保留完整头部、只去除长度字段，或去除应用头。若协议长度字段已经包含头部，可使用负 `Adjustment`。所有字段末端、无符号声明值、有符号调整、`size_t` 转换、strip 和 `MaxFrame` 都在访问 payload 前验证。

默认配置是四字节大端 payload 长度、去除四字节字段和 1 MiB 总帧上限。完整示例位于 `examples/network/frame_length/main.c`。

### `xrtNetLengthConfigInit`

初始化四字节大端长度、去除长度字段和 1 MiB 帧上限的默认配置。

```c
void xrtNetLengthConfigInit(xnetlengthconfig* pConfig)
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

[frame_length](../../examples/network/frame_length/main.c) · 默认配置

```c
	xrtNetLengthConfigInit(&Config);
```

### `xrtNetLengthInit`

复制并验证长度字段偏移、宽度、字节序、调整值、strip 和帧上限。

```c
bool xrtNetLengthInit(
	xnetlengthframer* pFramer,
	const xnetlengthconfig* pConfig
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFramer` | 输出 | 非空 | 接收搜索器 |
| `pConfig` | 输入 | 非空 | 长度前缀配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 配置非法 | `xrt.net` 域错误 |

#### 错误

- `xrt.net` / `XNET_ERROR_FRAME_CONFIG`（`XERR_ARGUMENT`） — 偏移 + 宽度、调整值或上限组合非法

#### 范例

[frame_length](../../examples/network/frame_length/main.c) · 初始化

```c
	if ( !xrtNetLengthInit(&Framer, &Config) ||
		 !xrtNetBufInit(&Input, NULL) ||
		 !xrtNetBufAppend(&Input, Packet, sizeof(Packet)) ||
		 (xrtNetLengthNext(&Framer, &Input, &Frame) !=
			XNET_FRAME_READY) ||
		 (xrtNetFrameCopy(
			&Input, &Frame, sPayload, sizeof(sPayload) - 1u
		 ) != 5) ) {
```

### `xrtNetLengthNext`

从输入头部解析下一条长度前缀帧。

```c
xnetframestatus xrtNetLengthNext(
	const xnetlengthframer* pFramer,
	const xnetbuf* pInput,
	xnetframe* pFrame
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFramer` | 输入/输出 | 已初始化 | 目标搜索器 |
| `pInput` | 输入 | 非空 | 输入缓冲 |
| `pFrame` | 输出 | 非空 | 接收帧描述 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_FRAME_READY` | 已解析一条完整帧，`pFrame` 有效 | — |
| `XNET_FRAME_MORE` | 输入不足，需保留未决前缀并追加数据 | 不设错误 |
| `XNET_FRAME_ERROR` | 参数非法、状态破坏或帧超限 | `XERR_ARGUMENT` / `xrt.net` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `xrt.net` / `XNET_ERROR_FRAME_STATE`（`XERR_STATE`） — 搜索器状态非法或长度字段越出输入
- `xrt.net` / `XNET_ERROR_FRAME_LENGTH`（`XERR_RANGE` / `XERR_VALUE`） — 声明长度溢出、超过上限或调整后非法

#### 范例

[frame_length](../../examples/network/frame_length/main.c) · 解析下一帧

```c
		 (xrtNetLengthNext(&Framer, &Input, &Frame) !=
			XNET_FRAME_READY) ||
```

## 模块契约：线程

帧解析器为调用方栈上的值对象，无共享状态；解析与帧操作可任意线程并发调用（不同解析器实例之间与同一实例的顺序调用均安全，实例本身非多线程共享）。

## 错误

- `XNET_ERROR_FRAME_CONFIG`：配置本身不可能形成合法帧。
- `XNET_ERROR_FRAME_STATE`：Framer 或帧范围已失效。
- `XNET_ERROR_FRAME_LIMIT`：完整帧或行 payload 超过硬上限。
- `XNET_ERROR_FRAME_LENGTH`：声明长度、调整或本机长度转换溢出。

`MORE` 是正常增量控制结果，不设置错误。`ERROR` 保留结构化错误；调用方通常应关闭当前协议流或按协议定义丢弃输入后重置 Framer。
