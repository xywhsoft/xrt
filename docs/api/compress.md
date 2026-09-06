# 压缩与解压缩

`<xrt/compress.h>` 提供与网络和 HTTP 解耦的压缩数据流能力。
`XRT_FEATURE_INFLATE` 包含原始 DEFLATE、zlib、HTTP 兼容 `deflate`
和 gzip 解码；`XRT_FEATURE_DEFLATE` 独立提供 raw、zlib 和 gzip 编码。
两个方向可以分别裁剪，HTTP 与 WebSocket 只在需要时组合它们。

## 对象与内存

`xrtInflateCreate` 只在真正需要解码时分配一个对象。对象包含 DEFLATE
算法必需的 32 KiB 滑动窗口；普通网络连接和未压缩 HTTP 调用不会承担这份
内存。`WindowBits` 接受 8 到 15，并严格拒绝超过配置窗口的回溯距离，而不只是
作为内存提示。`xrtInflateReset` 保留窗口并复位状态，适合反复处理独立数据流。

输出由 `xinflateoutputproc` 同步接收，视图只在回调期间有效。回调为空时
执行完整验证但丢弃输出。`xrtInflateAll` 是常见整块场景的便捷函数，返回值
由 `xrtFree` 释放并额外带一个不计入长度的零字节。

`xrtDeflateCreate` 同样按需分配算法状态。编码器内部包含 DEFLATE 的 32 KiB
历史字典、匹配表和 Huffman 工作区，因此不能按连接或请求提前常驻创建；
HTTP 应只在协商选中压缩后创建，WebSocket 只有启用上下文接管时才长期复用。
XRT 默认使用约 164 KiB 的低内存编码布局，而不是约 312 KiB 的普通布局；
`xrtDeflateReset` 保留这块状态并开始独立的新数据流。

两个方向的配置初始化、Create 和 Reset 都只要求配置位于有效的连续存储中，
不要求调用方保证结构体自然对齐。实现通过 `memcpy` 建立本地配置快照，不会在
对象生命周期内引用调用方配置；因此栈内配置、封包内配置和临时配置都遵循同一契约。

## 格式

- `XINFLATE_RAW`：没有包装的 RFC 1951 DEFLATE。
- `XINFLATE_ZLIB`：带 RFC 1950 Header 和 Adler-32 的 zlib。
- `XINFLATE_DEFLATE`：先检查 zlib Header，否则按 raw 解码，用于兼容实际
  HTTP `Content-Encoding: deflate` 服务。
- `XINFLATE_GZIP`：校验 Header、可选 Header CRC、正文 CRC32 和 ISIZE，
  并接受规范的拼接 gzip member。

Deflate 的 `RAW` 与 `ZLIB` 生成对应标准数据流；`GZIP` 使用 `MTIME=0`、
无可选字段和 `OS=255` 的确定性 Header，并生成 CRC32 与 ISIZE trailer。
`WindowBits` 同样接受 8 到 15，限制编码器可生成的最大回溯距离；zlib Header
中的 CINFO 会同步反映该窗口。相同输入、配置和 Flush 序列产生稳定输出，不启用
依赖未初始化内存的快速模式。

## Inflate 解码 API

### `xrtInflateConfigInit`

初始化默认配置：`Format=XINFLATE_RAW`、15 位窗口、`OutputLimit=0`（不限）。

```c
void xrtInflateConfigInit(xinflateconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 有效连续存储 | 只要求可读写字节范围，不要求自然对齐 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 纯初始化，不失败 |

#### 范例

[compress/inflate · gzip](../../examples/compress/inflate/main.c) · 默认值 + 显式切格式

```c
xrtInflateConfigInit(&Config);
Config.Format = XINFLATE_GZIP;
```

### `xrtInflateConfigValid`

验证 Inflate 配置；输入只需是有效连续存储。

```c
bool xrtInflateConfigValid(const xinflateconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 有效连续存储 | 待验证配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 配置可安全用于 Create/Reset | — |
| `false` | 字段越界或范围非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pConfig` 范围非法
- `XERR_VALUE` + `XINFLATE_ERROR_CONFIG` — 格式枚举、窗口位（8–15）或限额字段越界

#### 范例

[compress/stream_tour · 配置拒绝](../../examples/compress/stream_tour/main.c) · 非法格式枚举必须被拒

```c
xrtInflateConfigInit(&BadI);
BadI.Format = (xinflateformat)77;
if ( xrtInflateConfigValid(&BadI) ) {
	goto Cleanup;
}
```

### `xrtInflateCreate`

创建流式解码器；配置为空时使用默认值，否则立即复制配置快照。

```c
xinflate* xrtInflateCreate(const xinflateconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 允许空 | 空 = 默认配置；非空时立即快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 解码器（含 32 KiB 滑动窗口按 WindowBits 分配） | — |
| `NULL` | 配置非法或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_VALUE` + `XINFLATE_ERROR_CONFIG` — 配置非法
- `XERR_MEMORY` — 对象或窗口分配失败

#### 范例

[compress/stream_tour · 流式](../../examples/compress/stream_tour/main.c) · 配对 GZIP 格式后创建

```c
pInflate = xrtInflateCreate(&InflateConfig);
Plain.Size = 0;
if ( (pInflate == NULL) ||
```

### `xrtInflateReset`

失败原子地复位解码器并保留已经分配的滑动窗口。

```c
bool xrtInflateReset(
	xinflate* pInflate,
	const xinflateconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pInflate` | 输入/输出 | 非空 | 目标解码器；失败终态后复用的唯一途径 |
| `pConfig` | 输入 | 允许空 | 新流配置；空 = 保持原配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已复位，可开始新流 | — |
| `false` | 参数/配置非法，或从输出回调内调用 | 解码器状态不变 |

#### 错误

- `XERR_ARGUMENT` — 指针非法
- `XERR_VALUE` + `XINFLATE_ERROR_CONFIG` — 新配置非法
- `XERR_STATE` — 在输出回调执行期间重入

#### 范例

[compress/stream_tour · 复用](../../examples/compress/stream_tour/main.c) · 复位后 Done 必须回到假

```c
if ( !xrtInflateReset(pInflate, &InflateConfig) ||
	xrtInflateDone(pInflate) ) {
	goto Cleanup;  /* 复位后未完成 */
}
```

### `xrtInflateWrite`

同步消费完整输入片段并把输出分段交给回调；`pOutput` 为空时丢弃输出（仍完整验证）。`bFinal` 表示不会再提供输入，成功时要求压缩流完整结束且校验通过。

```c
bool xrtInflateWrite(
	xinflate* pInflate,
	xbytesview Input,
	bool bFinal,
	xinflateoutputproc pOutput,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pInflate` | 输入/输出 | 非空 | 目标解码器 |
| `Input` | 输入 | 借用、有效范围 | 本段压缩字节 |
| `bFinal` | 输入 | — | 末段标记；成功要求流完整结束 |
| `pOutput` | 输入 | 允许空 | 输出回调；空 = 校验并丢弃 |
| `pData` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 本段已消费；`bFinal` 时流完整结束 | — |
| `false` | 数据损坏、超限、回调中止或参数非法 | 参数失败不改状态；其余进入失败终态，须 `Reset` |

#### 错误

- `XERR_ARGUMENT` + `XINFLATE_ERROR_ARGUMENT` — 指针/输入范围非法（对象仍可用）
- `XERR_PROTOCOL` + `XINFLATE_ERROR_DATA` — 损坏数据、截断、校验不符或尾随数据
- `XERR_RANGE` + `XINFLATE_ERROR_LIMIT` — 解码总字节超过 `OutputLimit`
- `XERR_CANCELLED` + `XINFLATE_ERROR_OUTPUT` — 输出回调返回假并中止
- `XERR_STATE` — 从输出回调内重入同一对象

#### 范例

[compress/stream_tour · 流式](../../examples/compress/stream_tour/main.c) · 空回调丢弃输出，用 OutputSize 核对

```c
!xrtInflateWrite(pInflate,
	(xbytesview) { Coded.Buffer, Coded.Size },
	true, NULL, NULL) ||
!xrtInflateDone(pInflate) ) {
	goto Cleanup;
}
```

### `xrtInflateDone`

判断解码器是否已经完整结束；失败状态返回 false。

```c
bool xrtInflateDone(const xinflate* pInflate);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pInflate` | 输入 | 非空 | 目标解码器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 流已完整结束且校验通过 | — |
| `false` | 未结束或处于失败终态 | 失败终态不额外设置错误 |

#### 错误

- `XERR_ARGUMENT` — `pInflate` 为空

#### 范例

[compress/stream_tour · 流式](../../examples/compress/stream_tour/main.c) · Final 写入成功即完成

```c
!xrtInflateDone(pInflate) ||
(xrtInflateOutputSize(pInflate) != iOriginal) ) {
	goto Cleanup;
}
```

### `xrtInflateOutputSize`

返回当前流已经产生的解码字节总数。

```c
uint64 xrtInflateOutputSize(const xinflate* pInflate);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pInflate` | 输入 | 非空 | 目标解码器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 累计解码字节数（含失败前已产出部分） | — |
| `0` | 未产出或参数非法 | 非法时设置 `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — `pInflate` 为空

#### 范例

[compress/stream_tour · 流式](../../examples/compress/stream_tour/main.c) · 空回调形态的长度核对

```c
if ( xrtInflateOutputSize(pInflate) != iOriginal ) {
	goto Cleanup;
}
```

### `xrtInflateDestroy`

销毁解码器；空指针为空操作，输出回调中的同对象销毁会被拒绝。

```c
void xrtInflateDestroy(xinflate* pInflate);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pInflate` | 输入 | 允许空 | 目标解码器 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作；回调内销毁被拒后对象保留 |

#### 范例

[compress/stream_tour · 收尾](../../examples/compress/stream_tour/main.c) · 统一清理路径

```c
xrtInflateDestroy(pInflate);
xrtDeflateDestroy(pDeflate);
return iResult;
```

### `xrtInflateAll`

一次性解码完整输入并返回由 `xrtFree` 释放的字节；结果额外带一个不计入 `OutputSize` 的零字节。

```c
bytes xrtInflateAll(
	xbytesview Input,
	const xinflateconfig* pConfig,
	size_t* pOutputSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Input` | 输入 | 借用、有效范围 | 完整压缩输入 |
| `pConfig` | 输入 | 允许空 | 配置；空 = 默认 |
| `pOutputSize` | 输出 | 可空、有效连续存储 | 接收解码字节数；只在成功时写入 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 解码字节 + 零哨兵 | — |
| `NULL` | 参数、配置、分配或解码失败 | `*pOutputSize` 保持调用前值 |

#### 错误

- 同流式路径：`XERR_ARGUMENT` / `XERR_VALUE`+`CONFIG` / `XERR_PROTOCOL`+`DATA` / `XERR_RANGE`+`LIMIT` / `XERR_MEMORY`

#### 范例

[compress/inflate · gzip](../../examples/compress/inflate/main.c) · CRC 与长度 trailer 任一不符都失败

```c
pText = xrtInflateAll(
	(xbytesview){ Gzip, sizeof(Gzip) },
	&Config,
	&iSize
);
if ( pText == NULL ) {
	return 1;
}
```

## Deflate 编码 API

Flush 语义：

- `XDEFLATE_FLUSH_NONE`：只推进输入，不强制输出边界。
- `XDEFLATE_FLUSH_SYNC`：输出可同步解码的空块并保留历史字典，适合 WebSocket `permessage-deflate` 消息边界。
- `XDEFLATE_FLUSH_FULL`：建立同步边界并清空后续匹配历史。
- `XDEFLATE_FLUSH_FINISH`：结束唯一数据流；成功后只能查询或 Reset。

输出回调同步执行。回调内对同一对象的 Reset、再次 Write 或 Destroy 会被 `XERR_STATE` 拒绝，只读状态查询仍然安全。回调返回 `false` 可保留自己设置的结构化错误；没有错误时生成 `XERR_CANCELLED`。`OutputLimit` 计算全部线路字节，包括容器 Header/trailer。

### `xrtDeflateConfigInit`

初始化默认配置：`Format=XDEFLATE_RAW`、`Level=6`、15 位窗口、输出不限。

```c
void xrtDeflateConfigInit(xdeflateconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 有效连续存储 | 接收默认配置 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 纯初始化，不失败 |

#### 范例

[compress/deflate · gzip](../../examples/compress/deflate/main.c) · 默认配置直接出合法 gzip

```c
xrtDeflateConfigInit(&Config);
```

### `xrtDeflateConfigValid`

验证 Deflate 配置；输入只需是有效连续存储。

```c
bool xrtDeflateConfigValid(const xdeflateconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 有效连续存储 | 待验证配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 配置可用 | — |
| `false` | 字段越界或范围非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 范围非法
- `XERR_VALUE` + `XDEFLATE_ERROR_CONFIG` — 级别（0–9）、格式、窗口位或限额越界

#### 范例

[compress/stream_tour · 配置拒绝](../../examples/compress/stream_tour/main.c) · 越界级别必须被拒

```c
xrtDeflateConfigInit(&Bad);
Bad.Level = 99;  /* 越界级别 */
if ( xrtDeflateConfigValid(&Bad) ) {
	goto Cleanup;
}
```

### `xrtDeflateCreate`

创建流式编码器；配置为空时使用默认值，否则立即复制配置快照。

```c
xdeflate* xrtDeflateCreate(
	const xdeflateconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 允许空 | 空 = 默认配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 编码器（约 164 KiB 低内存布局） | — |
| `NULL` | 配置非法或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_VALUE` + `XDEFLATE_ERROR_CONFIG` — 配置非法
- `XERR_MEMORY` — 算法状态分配失败

#### 范例

[compress/stream_tour · 流式](../../examples/compress/stream_tour/main.c) · 验证配置后创建

```c
pDeflate = xrtDeflateCreate(&DeflateConfig);
if ( pDeflate == NULL ) {
	goto Cleanup;
}
```

### `xrtDeflateReset`

失败原子地复位编码器，并保留已经分配的算法状态存储。

```c
bool xrtDeflateReset(
	xdeflate* pDeflate,
	const xdeflateconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDeflate` | 输入/输出 | 非空 | 目标编码器；FINISH 后复用的途径 |
| `pConfig` | 输入 | 允许空 | 新流配置；空 = 保持原配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已复位，开始独立新数据流 | — |
| `false` | 参数/配置非法或回调内重入 | 状态不变 |

#### 错误

- `XERR_ARGUMENT` / `XERR_VALUE` + `XDEFLATE_ERROR_CONFIG`
- `XERR_STATE` — 输出回调执行期间重入

#### 范例

[compress/stream_tour · 复用](../../examples/compress/stream_tour/main.c) · 同对象跑第二条流

```c
if ( !xrtDeflateReset(pDeflate, &DeflateConfig) ||
	xrtDeflateDone(pDeflate) ) {
	goto Cleanup;
}
```

### `xrtDeflateWrite`

同步消费完整输入片段并把输出分段交给回调；FINISH 成功后对象进入完成终态，SYNC 和 FULL 保持数据流可继续写入。

```c
bool xrtDeflateWrite(
	xdeflate* pDeflate,
	xbytesview Input,
	xdeflateflush Flush,
	xdeflateoutputproc pOutput,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDeflate` | 输入/输出 | 非空 | 目标编码器 |
| `Input` | 输入 | 借用、有效范围 | 本段原始字节 |
| `Flush` | 输入 | 枚举 | `NONE`/`SYNC`/`FULL`/`FINISH` |
| `pOutput` | 输入 | 允许空 | 输出回调；空 = 丢弃（仅测试意义） |
| `pData` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 本段已编码并交付；FINISH 后进入终态 | — |
| `false` | 参数、限额、回调中止或内部异常 | 参数失败不改状态；其余进入失败终态 |

#### 错误

- `XERR_ARGUMENT` + `XDEFLATE_ERROR_ARGUMENT` — 指针/范围非法
- `XERR_RANGE` + `XDEFLATE_ERROR_LIMIT` — 线路字节超过 `OutputLimit`
- `XERR_CANCELLED` + `XDEFLATE_ERROR_OUTPUT` — 回调返回假中止
- `XERR_STATE` — 终态后再写或回调内重入
- `XERR_PROTOCOL` + `XDEFLATE_ERROR_CODEC` — 内部编码异常

#### 范例

[compress/stream_tour · 流式](../../examples/compress/stream_tour/main.c) · SYNC 分段 + FINISH 收尾

```c
if ( !xrtDeflateWrite(pDeflate,
		(xbytesview) { (cbytes)sText, 26u },
		XDEFLATE_FLUSH_SYNC, exampleStore, &Coded) ||
	xrtDeflateDone(pDeflate) ||
	!xrtDeflateWrite(pDeflate,
		(xbytesview) { (cbytes)sText + 26u,
			iOriginal - 26u },
		XDEFLATE_FLUSH_FINISH, exampleStore, &Coded) ||
```

### `xrtDeflateDone`

判断编码器是否已经通过 FINISH 完整结束；失败状态返回 false。

```c
bool xrtDeflateDone(const xdeflate* pDeflate);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDeflate` | 输入 | 非空 | 目标编码器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已 FINISH 完整结束 | — |
| `false` | 未结束或失败终态 | 失败终态不额外设置错误 |

#### 错误

- `XERR_ARGUMENT` — `pDeflate` 为空

#### 范例

[compress/stream_tour · 流式](../../examples/compress/stream_tour/main.c) · SYNC 后未完成、FINISH 后完成

```c
xrtDeflateWrite(pDeflate,
	(xbytesview) { (cbytes)sText, 26u },
	XDEFLATE_FLUSH_SYNC, exampleStore, &Coded) ||
	xrtDeflateDone(pDeflate) ||
```

### `xrtDeflateOutputSize`

返回当前数据流已经成功交付的编码字节总数。

```c
uint64 xrtDeflateOutputSize(
	const xdeflate* pDeflate
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDeflate` | 输入 | 非空 | 目标编码器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 累计线路字节数（含容器 Header/trailer） | — |
| `0` | 未交付或参数非法 | 非法时设置 `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — `pDeflate` 为空

#### 范例

[compress/stream_tour · 流式](../../examples/compress/stream_tour/main.c) · 与收集回调字节数一致

```c
(xrtDeflateOutputSize(pDeflate) != Coded.Size) ||
(Coded.Size == 0u) ) {
```

### `xrtDeflateDestroy`

销毁编码器；空指针为空操作，输出回调中的同对象销毁会被拒绝。

```c
void xrtDeflateDestroy(xdeflate* pDeflate);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDeflate` | 输入 | 允许空 | 目标编码器 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作；回调内销毁被拒后对象保留 |

#### 范例

[compress/stream_tour · 收尾](../../examples/compress/stream_tour/main.c) · 统一清理路径

```c
xrtInflateDestroy(pInflate);
xrtDeflateDestroy(pDeflate);
return iResult;
```

### `xrtDeflateAll`

一次性编码完整输入并返回由 `xrtFree` 释放的字节；结果额外带一个不计入 `OutputSize` 的零字节。

```c
bytes xrtDeflateAll(
	xbytesview Input,
	const xdeflateconfig* pConfig,
	size_t* pOutputSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Input` | 输入 | 借用、有效范围 | 完整原始输入 |
| `pConfig` | 输入 | 允许空 | 配置；空 = 默认 |
| `pOutputSize` | 输出 | 可空、有效连续存储 | 接收编码字节数；只在成功时写入 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 编码字节 + 零哨兵；同配置同输入逐字节稳定 | — |
| `NULL` | 参数、配置、分配或编码失败 | `*pOutputSize` 保持调用前值 |

#### 错误

- 同流式路径：`XERR_ARGUMENT` / `XERR_VALUE`+`CONFIG` / `XERR_RANGE`+`LIMIT` / `XERR_MEMORY`

#### 范例

[compress/deflate · gzip](../../examples/compress/deflate/main.c) · 34 字节重复文本 → 30 字节 gzip

```c
pGzip = xrtDeflateAll(
	XRT_BYTES_LITERAL(Text),
	&Config,
	&iSize
);
if ( pGzip == NULL ) {
	return 1;
}
```

## 安全边界

`OutputLimit` 是解码后总字节硬上限，可阻止压缩炸弹无限扩张。
`GzipHeaderLimit` 分别限制每个 gzip member 的 Header，默认 64 KiB。
默认 Inflate 和 Deflate 窗口都是 15 位；较小窗口必须由两端显式配置。解码器同时
校验 zlib CINFO 和每个实际匹配距离，因此协议层可以把协商后的窗口作为安全边界。
损坏校验、截断、尾随数据、超过限额和输出回调中止都会使对象进入失败终态；
调用 `xrtInflateReset` 后才能复用。

所有公开输入视图和输出长度槽都会先校验完整地址范围，包括整数地址回绕。
无效参数在读取输入和改变算法状态前同步失败，因此无效 `Write` 后对象仍可继续使用。
`xrtInflateAll` 与 `xrtDeflateAll` 只在完整成功后写入输出长度；参数、配置、分配、
编解码或消费者失败都保留调用前的长度值。输出长度槽也只要求有效连续存储，
不要求自然对齐。

Deflate 的配置错误、输出限额、消费者拒绝或内部编码异常同样进入失败终态；
调用 `xrtDeflateReset` 可以保留算法内存并重新开始。`xrtDeflateAll` 返回
`xrtFree` 释放的连续结果，并且只在成功时修改输出长度。

错误使用 `xrt.inflate` / `xrt.deflate` 结构化域：

| 代码 | Inflate | Deflate |
|---|---|---|
| `*_ERROR_ARGUMENT` | 指针/范围非法 | 指针/范围非法 |
| `*_ERROR_CONFIG` | 配置越界（`XERR_VALUE`） | 配置越界（`XERR_VALUE`） |
| `*_ERROR_STATE` | 终态后再用/回调重入 | 终态后再用/回调重入 |
| `*_ERROR_DATA` | 损坏/截断/校验不符（`XERR_PROTOCOL`） | — |
| `*_ERROR_LIMIT` | 超输出上限（`XERR_RANGE`） | 超输出上限（`XERR_RANGE`） |
| `*_ERROR_OUTPUT` | 回调中止（`XERR_CANCELLED`） | 回调中止（`XERR_CANCELLED`） |
| `XDEFLATE_ERROR_CODEC` | — | 内部编码异常（`XERR_PROTOCOL`） |

完整示例见 `examples/compress/inflate/main.c`、`examples/compress/deflate/main.c`
与 `examples/compress/stream_tour/main.c`。
