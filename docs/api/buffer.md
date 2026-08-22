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

```c
bool xrtBufferInit(xbuffer* pBuffer);
xbuffer* xrtBufferCreate(void);
void xrtBufferUnit(xbuffer* pBuffer);
void xrtBufferDestroy(xbuffer* pBuffer);
void xrtBufferClear(xbuffer* pBuffer);
```

`Init` 初始化栈上或内嵌结构，成功后由 `Unit` 释放。`Create` 创建堆上结构，成功后由 `Destroy` 释放。`Unit`、`Destroy` 允许空指针。`Clear` 保留容量，适合重复构建。

## 视图与容量

```c
xbytesview xrtBufferView(const xbuffer* pBuffer);
bool xrtBufferReserve(xbuffer* pBuffer, size_t iCapacity);
bool xrtBufferResize(xbuffer* pBuffer, size_t iSize);
bool xrtBufferTrim(xbuffer* pBuffer);
```

`View` 返回借用视图。`Reserve` 只保证最低容量。`Resize` 增长时把新增有效字节填零，缩小时不释放容量。`Trim` 把容量精确调整到长度，空缓冲会释放存储。

## 直接写入

```c
bytes xrtBufferAdd(xbuffer* pBuffer, size_t iSize);
bytes xrtBufferInsertSpace(xbuffer* pBuffer, size_t iOffset, size_t iSize);
```

两者增加未初始化的有效字节并返回首地址，适合调用方立即完整写入。大小必须大于零。`InsertSpace` 的位点范围是 `[0, Size]`，并保留原后缀。

## 复制与编辑

```c
bool xrtBufferAssign(xbuffer* pBuffer, xbytesview Data);
bool xrtBufferAppend(xbuffer* pBuffer, xbytesview Data);
bool xrtBufferAppendByte(xbuffer* pBuffer, uint8 iByte);
bool xrtBufferInsert(xbuffer* pBuffer, size_t iOffset, xbytesview Data);
bool xrtBufferWrite(xbuffer* pBuffer, size_t iOffset, xbytesview Data);
bool xrtBufferRemove(xbuffer* pBuffer, size_t iOffset, size_t iSize);
```

`Assign` 原子地替换全部有效内容。`Append` 和 `Insert` 复制字节并支持自引用；零长度视图是成功的空操作。`Remove` 要求完整区间有效，零长度或越界不会被静默截断。

`Write` 从偏移处覆盖，不移动现有后缀。写入末端超过 `Size` 时扩展到末端，并把旧末端到写入起点之间的空洞填零；这保留了旧版内存流经过验证的稀疏写语义。零长度写入不改变缓冲。

## 所有权

```c
bool xrtBufferSetTake(
	xbuffer* pBuffer,
	bytes* pData,
	size_t iSize,
	size_t iCapacity
);

bytes xrtBufferTake(
	xbuffer* pBuffer,
	size_t* pSize,
	size_t* pCapacity
);

xbuffer* xrtBufferFrom(xbytesview Data);
xbuffer* xrtBufferCreateTake(
	bytes* pData,
	size_t iSize,
	size_t iCapacity
);
```

`SetTake` 和 `CreateTake` 只接收 `xrtMalloc` 家族分配且满足 XRT 默认对齐的内存。必须满足 `iSize <= iCapacity`；零容量必须对应空地址。成功时来源槽被设为 `NULL`，失败时来源和原缓冲均不变。

`Take` 无复制地移出连续内存并重置缓冲。`pSize`、`pCapacity` 可为空；非空时不得位于被移出的内存中，也不得互相重叠。空缓冲成功返回 `NULL`，调用方可结合输出长度或调用前状态区分成功的空结果。

`From` 是常见复制构造入口。需要无复制接管时使用 `CreateTake`。

## 编码构造器

```c
xbuffer* xrtBufferFromHex(xstrview Text, uint32 iFlags);
xbuffer* xrtBufferFromBase64(
	xstrview Text,
	const xbase64config* pConfig
);
```

两者先完整验证输入和精确计算输出长度，再直接解码到最终缓冲，不建立中间副本。格式、规范性、空白、URL 字母表和填充规则完全继承相应 codec 契约。

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
