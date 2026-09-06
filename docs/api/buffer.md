# Buffer

`buffer` 提供拥有连续内存的通用字节缓冲。它面向二进制协议、序列化、内存流和需要随机覆盖的场景；文本拼接继续使用始终维护零结尾的 `xstrbuf`，网络高吞吐分块队列继续使用 `xnetbuf`。

## 裁剪与依赖

| 能力 | 公开选择宏 | 实现宏 | 依赖 |
|---|---|---|---|
| 连续字节缓冲 | `XRT_MODULE_BUFFER` | `XRT_FEATURE_BUFFER` | `array` |
| HEX 解码构造 | `XRT_MODULE_BUFFER_HEX` | `XRT_FEATURE_BUFFER_HEX` | `buffer`, `codec_hex` |
| Base64 解码构造 | `XRT_MODULE_BUFFER_BASE64` | `XRT_FEATURE_BUFFER_BASE64` | `buffer`, `codec_base64` |

应用只需在第一次包含 XRT 头文件前定义需要的 `XRT_MODULE_*`。生成的功能头会补齐完整依赖；手工使用低层 `XRT_FEATURE_*` 时，公共头会拒绝缺失依赖的组合。

## 稳定契约

- `xbuffer` 独占 `Data`，有效内容是 `[Data, Data + Size)`，保留区是 `[Data + Size, Data + Capacity)`。
- 缓冲不追加隐含零字节。需要 C 字符串结果时使用 `xstrbuf`，或显式追加零字节。
- 容量使用几何增长，不再保留旧版固定 64 KB 步长和 `AllocStep`。
- 所有长度使用 `size_t`，不再有旧版 4 GB 人工上限。
- `Clear` 只清空内容并保留容量；`Trim` 才缩减容量；`Unit` 释放全部内存。
- 分配失败时，原地址、长度、容量和已有内容保持不变。
- `Append`、`Insert`、`Assign`、`Write` 接受缓冲自身的完整有效子视图；引用保留区或跨越有效区会失败。
- 会改变容量、插入或删除内容的操作可能使旧视图和旧地址失效。
- 缓冲不带隐式锁；多个执行流共享时由调用方同步。

## 类型

### `xbuffer`

```c
typedef struct xbuffer {
	bytes Data;
	size_t Size;
	size_t Capacity;
} xbuffer;
```

结构公开是为了底层代码可以直接读取连续内容和长度，不表示调用方可以破坏 `Size <= Capacity`、空容量对应空地址等不变量。

## 生命周期

### `xrtBufferInit`

初始化调用方持有的空缓冲。

```c
bool xrtBufferInit(xbuffer* pBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输出 | 非空 | 接收空缓冲（`Data=NULL`、`Size=0`、`Capacity=0`）；不分配 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 空缓冲已就绪 | — |
| `false` | `pBuffer` 为空 | `*pBuffer` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pBuffer` 为空

#### 范例

[containers/buffer · 内存流](../../examples/containers/buffer/main.c) · 内嵌形态起步

```c
if ( !xrtBufferInit(&tBuffer) ) {
	return 1;
}
```

### `xrtBufferCreate`

创建空缓冲。

```c
xbuffer* xrtBufferCreate(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 无参数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 堆分配的空缓冲；`Destroy` 释放 | — |
| `NULL` | 结构分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_MEMORY` — 结构分配失败

#### 范例

[containers/buffer_tour · 容量族](../../examples/containers/buffer_tour/main.c) · 创建后立即 Reserve

```c
pBuffer = xrtBufferCreate();
if ( (pBuffer == NULL) ||
	!xrtBufferReserve(pBuffer, 16u) ||
```

### `xrtBufferUnit`

释放缓冲持有的连续内存，但不释放缓冲结构。内嵌形态（`Init` 产物）的收尾。

```c
void xrtBufferUnit(xbuffer* pBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 允许空 | 目标缓冲；释放后归零 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作；Take 之后的 Unit 不重复释放 |

#### 范例

[containers/buffer · 内存流](../../examples/containers/buffer/main.c) · Take 之后归位

```c
xrtFree(pResult);
xrtBufferUnit(&tBuffer);
return 0;
```

### `xrtBufferDestroy`

释放缓冲持有的连续内存和缓冲结构。`Create` 族产物的收尾。

```c
void xrtBufferDestroy(xbuffer* pBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 允许空 | 目标缓冲 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作 |

#### 范例

[containers/buffer_tour · 收尾](../../examples/containers/buffer_tour/main.c) · 逐个销毁堆缓冲

```c
xrtBufferDestroy(pFrom);
xrtBufferDestroy(pTaken);
xrtBufferDestroy(pBuffer);
```

### `xrtBufferClear`

清空有效内容但保留容量。

```c
void xrtBufferClear(xbuffer* pBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 允许空 | 目标缓冲；`Size` 归零，`Capacity` 不变 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 适合重复构建同一缓冲 |

#### 范例

[containers/buffer_tour · 容量族](../../examples/containers/buffer_tour/main.c) · 清空后容量保留、长度归零

```c
xrtBufferClear(pBuffer);
if ( xrtBufferView(pBuffer).Size != 0u ) {
	goto Cleanup;
}
```

## 视图与容量

### `xrtBufferView`

返回当前有效内容的借用视图。

```c
xbytesview xrtBufferView(const xbuffer* pBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空 | 目标缓冲 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 视图 | `[Data, Data + Size)` 的借用；下一次容量变更前有效 | — |
| 空视图 | 缓冲为空或参数非法 | 参数非法时设置 `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — `pBuffer` 为空

#### 范例

[containers/buffer_tour · 容量族](../../examples/containers/buffer_tour/main.c) · Resize 前后核对长度

```c
!xrtBufferResize(pBuffer, 4u) ||
(xrtBufferView(pBuffer).Size != 4u) ||
!xrtBufferTrim(pBuffer) ||
(xrtBufferView(pBuffer).Size != 4u) ) {
```

### `xrtBufferReserve`

保证缓冲至少具有指定容量，实际容量可以按几何策略增长。

```c
bool xrtBufferReserve(xbuffer* pBuffer, size_t iCapacity);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标缓冲 |
| `iCapacity` | 输入 | — | 最低容量（字节） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | `Capacity >= iCapacity`；内容与长度不变 | — |
| `false` | 参数非法或重分配失败 | 原地址、长度、容量与内容不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pBuffer` 为空
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[containers/buffer_tour · 容量族](../../examples/containers/buffer_tour/main.c) · 预留后再编辑

```c
pBuffer = xrtBufferCreate();
if ( (pBuffer == NULL) ||
	!xrtBufferReserve(pBuffer, 16u) ||
	!xrtBufferResize(pBuffer, 4u) ||
```

### `xrtBufferResize`

调整有效长度，扩展区域全部填零，缩小时保留容量。

```c
bool xrtBufferResize(xbuffer* pBuffer, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标缓冲 |
| `iSize` | 输入 | — | 新长度；大于当前长度时新增字节填零 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | `Size == iSize` | — |
| `false` | 参数非法或扩容失败 | 缓冲不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pBuffer` 为空
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[containers/buffer_tour · 容量族](../../examples/containers/buffer_tour/main.c) · 先扩 4 再缩 2

```c
if ( !xrtBufferResize(pBuffer, 2u) ||
	(xrtBufferView(pBuffer).Size != 2u) ) {
	goto Cleanup;
}
```

### `xrtBufferTrim`

把容量精确裁剪到有效长度，空缓冲会释放存储。

```c
bool xrtBufferTrim(xbuffer* pBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标缓冲；长度不变 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | `Capacity == Size`（或空缓冲已释放存储） | — |
| `false` | 参数非法或重分配失败 | 缓冲不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pBuffer` 为空
- `XERR_MEMORY` — 收缩重分配失败

#### 范例

[containers/buffer_tour · 容量族](../../examples/containers/buffer_tour/main.c) · 裁剪不影响长度

```c
!xrtBufferTrim(pBuffer) ||
(xrtBufferView(pBuffer).Size != 4u) ) {
	goto Cleanup;
}
```

## 直接写入

### `xrtBufferAdd`

在末尾增加未初始化字节并返回首地址，大小必须大于零。

```c
bytes xrtBufferAdd(xbuffer* pBuffer, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标缓冲 |
| `iSize` | 输入 | `> 0` | 追加的未初始化字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 新增区域首地址，调用方立即完整写入 | — |
| `NULL` | 参数非法或扩容失败 | 缓冲不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pBuffer` 为空或 `iSize` 为零
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[containers/buffer_tour · 编辑族](../../examples/containers/buffer_tour/main.c) · 尾部扩展后直写

```c
pWrite = xrtBufferAdd(pBuffer, 2u);
if ( (pWrite == NULL) ||
	(xrtBufferView(pBuffer).Size != 8u) ) {
	goto Cleanup;
}
memcpy(pWrite, "gh", 2u);
```

### `xrtBufferInsertSpace`

在指定位点插入未初始化字节并返回首地址，大小必须大于零。

```c
bytes xrtBufferInsertSpace(
	xbuffer* pBuffer,
	size_t iOffset,
	size_t iSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标缓冲 |
| `iOffset` | 输入 | `<= Size` | 插入位点；原后缀后移 |
| `iSize` | 输入 | `> 0` | 插入的未初始化字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 新增区域首地址 | — |
| `NULL` | 参数非法或扩容失败 | 缓冲不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pBuffer` 为空或 `iSize` 为零
- `XERR_RANGE` — `iOffset > Size`（越界插入）
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[containers/buffer_tour · 编辑族](../../examples/containers/buffer_tour/main.c) · 中段腾位不零填充

```c
bytes pSpace = xrtBufferInsertSpace(pBuffer, 2u, 3u);

if ( (pSpace == NULL) ||
	(xrtBufferView(pBuffer).Size != 12u) ) {
	goto Cleanup;
}
memcpy(pSpace, "ZZZ", 3u);
```

## 复制与编辑

### `xrtBufferAssign`

用字节视图替换全部有效内容，失败时保留原缓冲。

```c
bool xrtBufferAssign(xbuffer* pBuffer, xbytesview Data);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标缓冲 |
| `Data` | 输入 | 借用 | 新内容；允许是缓冲自身的完整有效子视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 内容已替换；零长度视图清空但保留容量 | — |
| `false` | 参数非法、非法视图或扩容失败 | 原缓冲完全不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pBuffer` 为空或视图非法（空数据配非零长度、引用保留区）
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[containers/buffer_tour · 编辑族](../../examples/containers/buffer_tour/main.c) · 整体替换内容

```c
if ( !xrtBufferAssign(pBuffer, BV("abcdef")) ||
	(xrtBufferView(pBuffer).Size != 6u) ||
```

### `xrtBufferAppend`

复制追加字节视图，允许来源是缓冲自身的有效子视图。

```c
bool xrtBufferAppend(xbuffer* pBuffer, xbytesview Data);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标缓冲 |
| `Data` | 输入 | 借用 | 追加内容；零长度是成功的空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已追加到末尾 | — |
| `false` | 参数非法或扩容失败 | 缓冲不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pBuffer` 为空或视图非法（引用保留区）
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[containers/buffer · 内存流](../../examples/containers/buffer/main.c) · 追加后稀疏写

```c
!xrtBufferAppend(&tBuffer, XRT_BYTES_LITERAL("abc")) ||
!xrtBufferWrite(&tBuffer, 5, XRT_BYTES_LITERAL("z"))
```

### `xrtBufferAppendByte`

追加一个字节。

```c
bool xrtBufferAppendByte(xbuffer* pBuffer, uint8 iByte);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标缓冲 |
| `iByte` | 输入 | — | 追加的字节 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已追加 | — |
| `false` | 参数非法或扩容失败 | 缓冲不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pBuffer` 为空
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[containers/buffer_tour · 编辑族](../../examples/containers/buffer_tour/main.c) · 单字节追加

```c
if ( !xrtBufferAppendByte(pBuffer, '!') ||
	(xrtBufferView(pBuffer).Size != 9u) ||
```

### `xrtBufferInsert`

在指定位点复制插入字节，允许来源是缓冲自身的有效子视图。

```c
bool xrtBufferInsert(
	xbuffer* pBuffer,
	size_t iOffset,
	xbytesview Data
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标缓冲 |
| `iOffset` | 输入 | `<= Size` | 插入位点；原后缀后移 |
| `Data` | 输入 | 借用 | 插入内容；零长度是成功的空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已插入 | — |
| `false` | 参数非法、越界或扩容失败 | 缓冲不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pBuffer` 为空或视图非法
- `XERR_RANGE` — `iOffset > Size`
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[containers/buffer_tour · 编辑族](../../examples/containers/buffer_tour/main.c) · 中段插入 "XY"

```c
if ( !xrtBufferInsert(pBuffer, 2u, BV("XY")) ||
	(xrtBufferView(pBuffer).Size != 8u) ||
```

### `xrtBufferWrite`

从指定位点覆盖字节；末端超出当前长度时扩展并把中间空洞填零。

```c
bool xrtBufferWrite(
	xbuffer* pBuffer,
	size_t iOffset,
	xbytesview Data
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标缓冲 |
| `iOffset` | 输入 | — | 写入起点；可越过当前末尾（空洞补零） |
| `Data` | 输入 | 借用 | 覆盖内容；零长度不改变缓冲 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已覆盖；`Size` 按写入末端扩展 | — |
| `false` | 参数非法、溢出或扩容失败 | 缓冲不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pBuffer` 为空或视图非法
- `XERR_RANGE` — `iOffset + Data.Size` 长度加法溢出
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[containers/buffer · 内存流](../../examples/containers/buffer/main.c) · 偏移 5 写 'z'，中间补零

```c
!xrtBufferAppend(&tBuffer, XRT_BYTES_LITERAL("abc")) ||
!xrtBufferWrite(&tBuffer, 5, XRT_BYTES_LITERAL("z"))
```

### `xrtBufferRemove`

删除完整有效区间，不会静默截断到末尾。

```c
bool xrtBufferRemove(
	xbuffer* pBuffer,
	size_t iOffset,
	size_t iSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标缓冲 |
| `iOffset` | 输入 | — | 删除起点 |
| `iSize` | 输入 | — | 删除字节数；零长度是成功的空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已删除并保序前移后缀 | — |
| `false` | 参数非法或区间越界 | 缓冲不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pBuffer` 为空
- `XERR_RANGE` — `[iOffset, iOffset + iSize)` 超出有效区间

#### 范例

[containers/buffer_tour · 编辑族](../../examples/containers/buffer_tour/main.c) · 删去中段 "XY"

```c
if ( !xrtBufferRemove(pBuffer, 2u, 2u) ||
	(xrtBufferView(pBuffer).Size != 6u) ||
```

## 所有权

### `xrtBufferSetTake`

接管由 `xrtMalloc` 家族分配的连续内存。成功时清空来源槽并释放缓冲原有内存，失败时双方所有权和内容都不变。

```c
bool xrtBufferSetTake(
	xbuffer* pBuffer,
	bytes* pData,
	size_t iSize,
	size_t iCapacity
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标缓冲 |
| `pData` | 输入/输出 | 非空 | 来源槽；成功后被置 `NULL`，失败不变 |
| `iSize` | 输入 | `<= iCapacity` | 有效长度 |
| `iCapacity` | 输入 | 零对应空地址 | 容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 缓冲已接管来源内存，原内存已释放 | — |
| `false` | 参数非法或槽无效 | 双方所有权与内容不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空、`iSize > iCapacity`、零容量配非空地址，或槽不是 XRT 默认对齐的 `xrtMalloc` 内存

#### 范例

[containers/buffer_tour · 接管族](../../examples/containers/buffer_tour/main.c) · 来源槽被清空

```c
if ( !xrtBufferSetTake(pBuffer, &pSlot, 6u, 6u) ||
	(pSlot != NULL) ||  /* 来源槽被清空 */
	(xrtBufferView(pBuffer).Size != 6u) ||
```

### `xrtBufferTake`

取走连续内存并把缓冲重置为空；空缓冲成功返回 `NULL`。

```c
bytes xrtBufferTake(
	xbuffer* pBuffer,
	size_t* pSize,
	size_t* pCapacity
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入/输出 | 非空 | 目标缓冲；成功后归零可复用 |
| `pSize` | 输出 | 可空 | 接收有效长度；不得位于缓冲内存内 |
| `pCapacity` | 输出 | 可空 | 接收容量；与 `pSize` 不得重叠 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 连续内存所有权（`xrtFree` 释放） | — |
| `NULL` | 空缓冲的成功结果，或参数非法 | 参数非法时缓冲不变并设置错误 |

#### 错误

- `XERR_ARGUMENT` — `pBuffer` 为空、输出指针位于被移出内存中、或两输出互相重叠

#### 范例

[containers/buffer · 内存流](../../examples/containers/buffer/main.c) · 拼好即取，之后 Unit 不重复释放

```c
pResult = xrtBufferTake(&tBuffer, &iSize, NULL);
if ( (pResult == NULL) || (iSize != 6) ) {
	xrtBufferUnit(&tBuffer);
	return 3;
}
```

### `xrtBufferFrom`

创建字节视图的独立副本。

```c
xbuffer* xrtBufferFrom(xbytesview Data);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | 借用 | 复制来源；零长度创建空缓冲 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 内容独立的新缓冲；来源不受影响 | — |
| `NULL` | 视图非法或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 视图非法（空数据配非零长度）
- `XERR_MEMORY` — 结构或内容分配失败

#### 范例

[containers/buffer_tour · 接管族](../../examples/containers/buffer_tour/main.c) · 复制创建的对照用法

```c
pFrom = xrtBufferFrom(BV("cp"));
if ( (pFrom == NULL) ||
	(xrtBufferView(pFrom).Size != 2u) ||
```

### `xrtBufferCreateTake`

创建缓冲并接管来源槽，失败时来源所有权不变。

```c
xbuffer* xrtBufferCreateTake(
	bytes* pData,
	size_t iSize,
	size_t iCapacity
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入/输出 | 非空 | 来源槽；成功后置 `NULL` |
| `iSize` | 输入 | `<= iCapacity` | 有效长度 |
| `iCapacity` | 输入 | 零对应空地址 | 容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 已接管内存的新缓冲 | — |
| `NULL` | 参数非法或结构分配失败 | 来源所有权与内容不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 槽无效（同 `SetTake` 的约束）
- `XERR_MEMORY` — 树结构分配失败

#### 范例

[containers/buffer_tour · 接管族](../../examples/containers/buffer_tour/main.c) · 创建即接管

```c
pTaken = xrtBufferCreateTake(&pSlot, 3u, 3u);
if ( (pTaken == NULL) ||
	(pSlot != NULL) ||
```

## 编码构造器

两者先完整验证输入和精确计算输出长度，再直接解码到最终缓冲，不建立中间副本。格式、规范性、空白、URL 字母表和填充规则完全继承相应 codec 契约。

### `xrtBufferFromHex`

严格解码 HEX 文本并创建缓冲。

```c
xbuffer* xrtBufferFromHex(xstrview Text, uint32 iFlags);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | HEX 文本；长度必须为偶数 |
| `iFlags` | 输入 | codec 标志 | 大小写、空白与分组规则（`codec_hex` 契约） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 解码字节的独立缓冲 | — |
| `NULL` | 文本非法或分配失败 | 保留 codec 的稳定错误域与代码 |

#### 错误

- `codec_hex` 错误域 — 文本非法（奇数长度、非 HEX 字符、空白违规）
- `XERR_MEMORY` — 缓冲分配失败

#### 范例

[data/buffer_hex · 解码构造](../../examples/data/buffer_hex/main.c) · "48656c6c6f" → "Hello"

```c
xbuffer* pBuffer = xrtBufferFromHex(
	XRT_STR_LITERAL("48656c6c6f"),
	0
);
```

### `xrtBufferFromBase64`

按 Base64 配置严格解码文本并创建缓冲。

```c
xbuffer* xrtBufferFromBase64(
	xstrview Text,
	const xbase64config* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | Base64 文本 |
| `pConfig` | 输入 | 允许空 | 解码配置；空指针用默认（标准字母表、要求填充） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 解码字节的独立缓冲（可含零字节） | — |
| `NULL` | 文本或配置非法、分配失败 | 保留 codec 的稳定错误域与代码 |

#### 错误

- `codec_base64` 错误域 — 字母表、填充、长度或空白违规
- `XERR_MEMORY` — 缓冲分配失败

#### 范例

[data/buffer_base64 · 解码构造](../../examples/data/buffer_base64/main.c) · "SGVsbG8=" → "Hello"

```c
xbuffer* pBuffer = xrtBufferFromBase64(
	XRT_STR_LITERAL("SGVsbG8="),
	NULL
);
```

## 示例

```c
xbuffer Buffer;
bytes pResult;
size_t iSize;

if ( !xrtBufferInit(&Buffer) ) {
	return false;
}
if ( !xrtBufferAppend(&Buffer, XRT_BYTES_LITERAL("abc")) ||
	 !xrtBufferWrite(&Buffer, 5, XRT_BYTES_LITERAL("z")) ) {
	xrtBufferUnit(&Buffer);
	return false;
}

/* 内容是 61 62 63 00 00 7a。 */
pResult = xrtBufferTake(&Buffer, &iSize, NULL);
xrtFree(pResult);
xrtBufferUnit(&Buffer);
```

可运行示例位于 `examples/containers/buffer/main.c`。

## 错误

- 空参数、非法视图、无效所有权槽或引用保留区：`XERR_ARGUMENT`。
- 被破坏的公开结构：`XERR_STATE`。
- 越界插入、删除或长度加法溢出：`XERR_RANGE`。
- 分配或重分配失败：`XERR_MEMORY`。
- HEX/Base64 配置或文本错误：保留 codec 的稳定错误域、代码和原因。
