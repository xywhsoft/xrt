# HTTP 正文解码

`<xrt/http_decode.h>` 把零分配的 `Content-Encoding` 解析与通用 Inflate 组合为
独立、可裁剪的流式正文解码器。它不依赖 HTTP 客户端、服务器、Body 对象或网络
缓冲区，可以直接接在 `xrtHttp1BodyRead` 产生的数据片段之后。

## 类型与常量

### `xhttpdecodemode`

解码模式明确区分无编码、成功接管的内置编码和显式允许的原样回退。

```c
typedef enum xhttpdecodemode {
	XHTTP_DECODE_IDENTITY = 0,
	XHTTP_DECODE_CONTENT,
	XHTTP_DECODE_RAW
} xhttpdecodemode;
```

| 值 | 语义 |
|---|---|
| `XHTTP_DECODE_IDENTITY` | 无变换 |
| `XHTTP_DECODE_CONTENT` | 按内容编码解码 |
| `XHTTP_DECODE_RAW` | 允许透传原始字节 |

### `xhttpdecodeflag`

默认拒绝未知编码；调用方可显式选择保留整个原始表示。

```c
typedef enum xhttpdecodeflag {
	XHTTP_DECODE_ALLOW_RAW = UINT32_C(0x00000001)
} xhttpdecodeflag;
```

| 值 | 语义 |
|---|---|
| `XHTTP_DECODE_ALLOW_RAW` | 允许透传原始字节 |

### `xhttpdecodeerror`

错误码覆盖配置、Content-Encoding、状态和输出边界。

```c
typedef enum xhttpdecodeerror {
	XHTTP_DECODE_ERROR_ARGUMENT = 1,
	XHTTP_DECODE_ERROR_CONFIG,
	XHTTP_DECODE_ERROR_CONTENT_ENCODING,
	XHTTP_DECODE_ERROR_UNSUPPORTED,
	XHTTP_DECODE_ERROR_STATE,
	XHTTP_DECODE_ERROR_LIMIT,
	XHTTP_DECODE_ERROR_OUTPUT
} xhttpdecodeerror;
```

| 值 | 语义 |
|---|---|
| `XHTTP_DECODE_ERROR_ARGUMENT` | 参数非法 |
| `XHTTP_DECODE_ERROR_CONFIG` | 配置非法 |
| `XHTTP_DECODE_ERROR_CONTENT_ENCODING` | 失败 |
| `XHTTP_DECODE_ERROR_UNSUPPORTED` | 不支持 |
| `XHTTP_DECODE_ERROR_STATE` | 状态非法 |
| `XHTTP_DECODE_ERROR_LIMIT` | 超限 |
| `XHTTP_DECODE_ERROR_OUTPUT` | 输出回调失败 |

### `xhttpdecodeconfig`

每个解码层和最终明文都受同一个硬限额约束。

```c
typedef struct xhttpdecodeconfig {
	uint64 OutputLimit;
	uint32 GzipHeaderLimit;
	uint32 MaxCodings;
	uint32 Flags;
} xhttpdecodeconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `OutputLimit` | `uint64` | OutputLimit |
| `GzipHeaderLimit` | `uint32` | GzipHeaderLimit |
| `MaxCodings` | `uint32` | MaxCodings |
| `Flags` | `uint32` | 标志位 |

### `xhttpdecode`

HTTP 解码器拥有并复用底层 Inflate 状态。

```c
typedef struct xhttpdecode xhttpdecode;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xhttpdecodeoutputproc`

输出视图只在回调期间有效，返回 false 会终止当前解码器。

```c
typedef bool (*xhttpdecodeoutputproc)(xbytesview Data, ptr pData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XHTTP_DECODE_OUTPUT_SAFE_DEFAULT` | `(UINT64_C(16) * 1024u * 1024u)` | XHTTPDECODE输出失败SAFE默认值 |

## 模式

- `XHTTP_DECODE_IDENTITY`：没有内容编码或只有 `identity`，输入视图直接交给回调；
- `XHTTP_DECODE_CONTENT`：按线路声明的逆序执行 gzip 或 HTTP deflate 解码；
- `XHTTP_DECODE_RAW`：仅在显式设置 `XHTTP_DECODE_ALLOW_RAW` 后，对未知编码原样交付。

默认策略严格拒绝未知编码，避免调用方把未解码字节误当作明文。代理、缓存或需要
保留扩展编码的程序可以选择原样回退，并通过 `xrtHttpDecodeMode` 保留元数据。

## 模块契约：线程

解码与分块 API 为无共享状态的纯函数，可任意线程并发调用。

## 限额

`OutputLimit` 同时限制每个中间解码层和最终明文，防止嵌套压缩绕过膨胀限制。
`GzipHeaderLimit` 限制可选 gzip Header，`MaxCodings` 限制嵌套层数。默认最多四层，
实现硬上限为 `XHTTP_CONTENT_CODINGS_MAX`。

解码器不保存正文。输出视图只在回调期间有效，回调必须在返回前消费或复制数据。
`xrtHttpDecodeReset` 会复用已经分配的 Inflate 滑动窗口，适合连接池和长连接逐消息
处理，不需要为每个响应重新分配 32 KiB 窗口。

输出回调返回 `false` 会把当前解码器置为失败终态；本次输入与输出计数不会发布，后续
写入返回状态错误。调用方处理完自己的回调错误后，可以显式调用 `xrtHttpDecodeReset`
开始下一条消息。字段描述符和配置允许未对齐存储，多层创建的每个 OOM 点都保证释放
已经建立的解码层。

```c
xhttpdecode* pDecode = xrtHttpDecodeCreate(
	pHead->Fields,
	pHead->FieldCount,
	NULL
);

xrtHttpDecodeWrite(
	pDecode,
	BodyData,
	bBodyDone,
	onBody,
	pContext
);
```

HTTP/1 的报文边界仍由 `xrtHttp1BodyRead` 决定。只有该 reader 返回最终完成状态时，
最后一次 `xrtHttpDecodeWrite` 才传入 `bFinal = true`；解码器随后会验证压缩流结束、
gzip CRC 与长度 trailer。

## API

### 配置与生命周期

### `xrtHttpDecodeConfigInit`

初始化兼容配置：最多四层、64 KiB gzip Header、明文长度不设上限。

```c
void xrtHttpDecodeConfigInit(xhttpdecodeconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收兼容配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无

#### 范例

[http/decode_tour · 配置](../../examples/http/decode_tour/main.c) · 观察

```c
	xrtHttpDecodeConfigInit(&Compat);
```

### `xrtHttpDecodeConfigInitSafe`

初始化面向不可信对端的安全配置；明文最多 16 MiB，更大正文需显式修改 OutputLimit。

```c
void xrtHttpDecodeConfigInitSafe(xhttpdecodeconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收安全配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 纯初始化 | — |

#### 错误

- 无；无限明文须显式设 `XHTTP_DECODE_OUTPUT_UNLIMITED`

#### 范例

[http/decode_tour · 配置](../../examples/http/decode_tour/main.c) · 观察

```c
	xrtHttpDecodeConfigInitSafe(&Safe);
```

### `xrtHttpDecodeCreate`

根据全部 Header 创建解码器；字段和值只在本次调用期间借用。

```c
xhttpdecode* xrtHttpDecodeCreate(
	const xhttpfield* pFields,
	size_t iCount,
	const xhttpdecodeconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFields` | 输入 | 借用（仅调用期间） | 字段数组 |
| `iCount` | 输入 | — | 条目数 |
| `pConfig` | 输入 | 允许空 | 空 = 兼容配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 解码器（按 Content-Encoding/Content-Length 决定模式） | — |
| `NULL` | Header 不一致或 OOM | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误 — 编码组合非法或长度矛盾
- `XERR_MEMORY`

#### 范例

[http/decode · 创建](../../examples/http/decode/main.c) · 观察

```c
	xhttpdecode* pDecode = xrtHttpDecodeCreate(Fields, 1, NULL);
```

### `xrtHttpDecodeReset`

为下一条消息复位并复用已经分配的 Inflate 窗口。

```c
bool xrtHttpDecodeReset(
	xhttpdecode* pDecode,
	const xhttpfield* pFields,
	size_t iCount,
	const xhttpdecodeconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDecode` | 输入/输出 | 非空 | 解码器 |
| `pFields` | 输入 | 借用 | 新消息字段 |
| `iCount` | 输入 | — | 条目数 |
| `pConfig` | 输入 | 允许空 | 新配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已复位可处理下一条 | — |
| `false` | 前一条未完成或 Header 非法 | 状态不变 |

#### 错误

- `XERR_STATE` — 前一条消息未终结
- `xrt.http` 域错误

#### 范例

[http/decode_tour · 复用](../../examples/http/decode_tour/main.c) · 观察

```c
	if ( !xrtHttpDecodeReset(pDecode, Fields, 1u, &Compat) ||
		(xrtHttpDecodeMode(pDecode) != XHTTP_DECODE_IDENTITY) ||
		!xrtHttpDecodeWrite(pDecode,
			(xbytesview) { arrGzip, 4u }, true,
			exampleOutput, (ptr)&Out) ||
		!xrtHttpDecodeDone(pDecode) ||
		(Out.iBytes != 4u) ||
		(xrtHttpDecodeInputSize(pDecode) != 4u) ) {
```

### `xrtHttpDecodeDestroy`

销毁解码器；空指针是安全的空操作。

```c
void xrtHttpDecodeDestroy(xhttpdecode* pDecode);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDecode` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 引用与窗口已释放 | — |

#### 错误

- 无 — 销毁不失败

#### 范例

[http/decode · 收尾](../../examples/http/decode/main.c) · 观察

```c
	xrtHttpDecodeDestroy(pDecode);
```


### 流式解码与观测

### `xrtHttpDecodeWrite`

同步消费完整输入片段；`bFinal` 表示正文已达协议边界。无编码和原样回退路径直接调用 Output，不复制输入。

```c
bool xrtHttpDecodeWrite(
	xhttpdecode* pDecode,
	xbytesview Input,
	bool bFinal,
	xhttpdecodeoutputproc pOutput,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDecode` | 输入/输出 | 非空 | 解码器 |
| `Input` | 输入 | 借用 | 本段线路字节 |
| `bFinal` | 输入 | — | 末段标记 |
| `pOutput` | 输入 | 允许空 | 明文回调；空 = 丢弃 |
| `pData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已消费（终段校验通过） | — |
| `false` | 数据损坏、超限或回调中止 | 进入失败终态 |

#### 错误

- `xrt.http` 域错误 — 压缩流损坏/截断
- `XERR_RANGE` — 超 OutputLimit
- `XERR_CANCELLED` — 回调中止

#### 范例

[http/decode · 解码](../../examples/http/decode/main.c) · 观察

```c
	bSuccess = xrtHttpDecodeWrite(
		pDecode,
		(xbytesview){ Gzip, sizeof(Gzip) },
		true,
		printBody,
		stdout
	) && xrtHttpDecodeDone(pDecode);
```

### `xrtHttpDecodeMode`

返回当前消息的交付模式。

```c
xhttpdecodemode xrtHttpDecodeMode(
	const xhttpdecode* pDecode
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDecode` | 输入 | 允许空 | 解码器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XHTTP_DECODE_*` | 直通/identity/解码模式 | — |
| 零值 | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[http/decode_tour · 自省](../../examples/http/decode_tour/main.c) · 观察

```c
	if ( (xrtHttpDecodeMode(pDecode) != XHTTP_DECODE_CONTENT) ||
		!xrtHttpDecodeWrite(pDecode,
			(xbytesview) { arrGzip, sizeof(arrGzip) }, true,
			exampleOutput, (ptr)&Out) ||
		!xrtHttpDecodeDone(pDecode) ||
		(Out.iBytes != 26u) ||
		(memcmp(Out.arrText, "identity-passthrough-check",
			26u) != 0) ||
		(xrtHttpDecodeInputSize(pDecode) != 46u) ||
		(xrtHttpDecodeOutputSize(pDecode) != 26u) ) {
```

### `xrtHttpDecodeDone`

判断最终正文边界和全部压缩流 trailer 均已验证。

```c
bool xrtHttpDecodeDone(const xhttpdecode* pDecode);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDecode` | 输入 | 允许空 | 解码器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 消息完整终结 | — |
| `false` | 未完成或失败终态 | 纯查询 |

#### 错误

- 无 — 纯查询

#### 范例

[http/decode · 完成](../../examples/http/decode/main.c) · 观察

```c
	) && xrtHttpDecodeDone(pDecode);
```

### `xrtHttpDecodeInputSize`

返回成功提交给当前消息的线路正文总字节数。

```c
uint64 xrtHttpDecodeInputSize(const xhttpdecode* pDecode);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDecode` | 输入 | 允许空 | 解码器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 线路字节数 | — |
| `0` | 无或参数非法 | `XERR_ARGUMENT`（非法时） |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[http/decode_tour · 自省](../../examples/http/decode_tour/main.c) · 观察

```c
		(xrtHttpDecodeInputSize(pDecode) != 46u) ||
		(xrtHttpDecodeOutputSize(pDecode) != 26u) ) {
```

### `xrtHttpDecodeOutputSize`

返回已经被输出回调接受或明确丢弃的正文总字节数。

```c
uint64 xrtHttpDecodeOutputSize(const xhttpdecode* pDecode);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDecode` | 输入 | 允许空 | 解码器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 明文字节数（含丢弃） | — |
| `0` | 无或参数非法 | `XERR_ARGUMENT`（非法时） |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法

#### 范例

[http/decode_tour · 自省](../../examples/http/decode_tour/main.c) · 观察

```c
		(xrtHttpDecodeOutputSize(pDecode) != 26u) ) {
```

