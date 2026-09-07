# Core API

`core` 是 XRT 唯一不可裁剪的公共底座，提供版本、固定宽度类型、借用视图和
引用计数。它不需要显式初始化，也不依赖线程、容器、文件或网络模块。

```c
#include <xrt/core.h>
```

聚合入口 `<xrt.h>` 会自动包含核心、[内存](memory.md)和[错误](error.md)
三个头文件。

## 类型与常量

### `xseek`

通用 IO 与文件游标共享的移动基准。

```c
typedef enum xseek {
	XSEEK_START = 0,
	XSEEK_CURRENT,
	XSEEK_END
} xseek;
```

| 值 | 语义 |
|---|---|
| `XSEEK_START` | START |
| `XSEEK_CURRENT` | CURRENT |

### `xrtresourcelimits`

```c
typedef struct xrtresourcelimits {
	uint32 iSize;
	uint32 iVersion;
	uint64 iMaxInputBytes;
	uint64 iMaxOutputBytes;
	uint64 iMaxItemBytes;
	uint64 iMaxEntries;
	uint64 iMaxNodes;
	uint32 iMaxDepth;
	uint32 iMaxCompressionRatio;
	uint32 iFlags;
	uint32 iReserved;
} xrtresourcelimits;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `iSize` | `uint32` | iSize |
| `iVersion` | `uint32` | iVersion |
| `iMaxInputBytes` | `uint64` | iMaxInputBytes |
| `iMaxOutputBytes` | `uint64` | iMaxOutputBytes |
| `iMaxItemBytes` | `uint64` | iMaxItemBytes |
| `iMaxEntries` | `uint64` | iMaxEntries |
| `iMaxNodes` | `uint64` | iMaxNodes |
| `iMaxDepth` | `uint32` | iMaxDepth |
| `iMaxCompressionRatio` | `uint32` | iMaxCompressionRatio |
| `iFlags` | `uint32` | iFlags |
| `iReserved` | `uint32` | iReserved |

### `xrtprogressflag`

```c
typedef enum xrtprogressflag {
	XRT_PROGRESS_TOTAL_KNOWN = 1u << 0,
	XRT_PROGRESS_FINAL = 1u << 1
} xrtprogressflag;
```

| 值 | 语义 |
|---|---|
| `XRT_PROGRESS_TOTAL_KNOWN` | XRTPROGRESSTOTALKNOWN |

### `xrtprogress`

```c
typedef struct xrtprogress {
	uint32 iSize;
	uint32 iVersion;
	uint32 iFlags;
	uint32 iReserved;
	uint64 iInputBytes;
	uint64 iTotalInputBytes;
	uint64 iOutputBytes;
} xrtprogress;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `iSize` | `uint32` | iSize |
| `iVersion` | `uint32` | iVersion |
| `iFlags` | `uint32` | iFlags |
| `iReserved` | `uint32` | iReserved |
| `iInputBytes` | `uint64` | iInputBytes |
| `iTotalInputBytes` | `uint64` | iTotalInputBytes |
| `iOutputBytes` | `uint64` | iOutputBytes |

### `xbytesview`

字节视图只借用内存，不拥有数据，也不要求末尾补零。

```c
typedef struct xbytesview {
	cbytes Data;
	size_t Size;
} xbytesview;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `cbytes` | Data |
| `Size` | `size_t` | Size |

### `xstrview`

字符串视图只借用字节，不拥有数据，也不要求末尾补零。

```c
typedef struct xstrview {
	cstr Data;
	size_t Size;
} xstrview;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `cstr` | Data |
| `Size` | `size_t` | Size |

### `xtime`

绝对时间使用 Unix Epoch 微秒；该标量也是 xlang time 类型的底层表示。

```c
typedef int64 xtime;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xrtprogressproc`

返回 false 请求取消。实现不得在回调返回后继续保存 pProgress 或 pUserData。

```c
typedef bool (*xrtprogressproc)(const xrtprogress* pProgress, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xerrkind`

跨模块稳定的错误类别。

```c
typedef enum xerrkind {
	XERR_NONE = 0,
	XERR_ARGUMENT,
	XERR_TYPE,
	XERR_VALUE,
	XERR_RANGE,
	XERR_STATE,
	XERR_MEMORY,
	XERR_IO,
	XERR_NOT_FOUND,
	XERR_EXISTS,
	XERR_PERMISSION,
	XERR_AGAIN,
	XERR_TIMEOUT,
	XERR_CANCELLED,
	XERR_CLOSED,
	XERR_PROTOCOL,
	XERR_UNSUPPORTED,
	XERR_INTERNAL
} xerrkind;
```

| 值 | 语义 |
|---|---|
| `XERR_NONE` | 无 |
| `XERR_ARGUMENT` | 参数非法 |
| `XERR_TYPE` | 类型 |
| `XERR_VALUE` | 值非法 |
| `XERR_RANGE` | 范围越界 |
| `XERR_STATE` | 状态非法 |
| `XERR_MEMORY` | 内存分配失败 |
| `XERR_IO` | 系统 IO 失败 |
| `XERR_NOT_FOUND` | NOTFOUND |
| `XERR_EXISTS` | 已存在 |
| `XERR_PERMISSION` | PERMISSION |
| `XERR_AGAIN` | 暂不可推进 |
| `XERR_TIMEOUT` | 超时 |
| `XERR_CANCELLED` | 已取消 |
| `XERR_CLOSED` | 已关闭 |
| `XERR_PROTOCOL` | 协议非法 |
| `XERR_UNSUPPORTED` | 不支持 |

### `xerrordesc`

描述一个完整错误，所有字符串在创建时复制。

```c
typedef struct xerrordesc {
	xerrkind Kind;
	int32 Code;
	int32 SystemCode;
	cstr Domain;
	cstr Operation;
	cstr Message;
	cstr Data;
	const xerror* Cause;
} xerrordesc;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Kind` | `xerrkind` | Kind |
| `Code` | `int32` | Code |
| `SystemCode` | `int32` | SystemCode |
| `Domain` | `cstr` | Domain |
| `Operation` | `cstr` | Operation |
| `Message` | `cstr` | Message |
| `Data` | `cstr` | Data |
| `Cause` | `const xerror*` | Cause |

### `xerrorlocation`

可选的源码位置；零值表示调用方没有提供对应信息。

```c
typedef struct xerrorlocation {
	cstr File;
	int32 Line;
	int32 Column;
} xerrorlocation;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `File` | `cstr` | File |
| `Line` | `int32` | Line |
| `Column` | `int32` | Column |

### `xerror`

错误对象由 XRT 管理，对外保持不可变。

```c
typedef struct xerror xerror;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xerrorhandler`

错误处理器只借用错误对象，保存时必须增加引用。

```c
typedef void (*xerrorhandler)(const xerror* pError, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xallocator`

XRT 所有动态内存最终使用同一个底层分配器。

```c
typedef struct xallocator {
	ptr Context;
	xallocproc Alloc;
	xreallocproc Realloc;
	xfreeproc Free;
} xallocator;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Context` | `ptr` | Context |
| `Alloc` | `xallocproc` | Alloc |
| `Realloc` | `xreallocproc` | Realloc |
| `Free` | `xfreeproc` | Free |

### `xallocproc`

自定义底层分配器回调。

```c
typedef ptr (*xallocproc)(ptr pContext, size_t iSize);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xreallocproc`

```c
typedef ptr (*xreallocproc)(ptr pContext, ptr pMemory, size_t iSize);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xfreeproc`

```c
typedef void (*xfreeproc)(ptr pContext, ptr pMemory);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XRT_RESOURCE_LIMITS_VERSION` | `1u` | 解析器、压缩器与归档器共用的资源边界。零值表示不限制对应项目。 |
| `XRT_PROGRESS_VERSION` | `1u` | 长耗时流操作共用的进度事件。回调仅在发起操作的线程内同步调用。 |

## 版本与公共宏

| 名称 | 含义 |
| --- | --- |
| `XRT_VERSION_MAJOR` | 主版本号 |
| `XRT_VERSION_MINOR` | 次版本号 |
| `XRT_VERSION_PATCH` | 修订版本号 |
| `XRT_VERSION_TEXT` | 零结尾版本字符串 |
| `XRT_NPOS` | `size_t` 查找接口共用的未找到值 |
| `XRT_API` | 静态库、导出库和导入库共用的符号规则 |
| `XRT_EXTERN_C_BEGIN` / `XRT_EXTERN_C_END` | C++ 调用时使用 C 链接 |

### `xrtVersion`

返回当前 XRT 版本字符串。

```c
cstr xrtVersion(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 无参数 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `cstr` | `XRT_VERSION_TEXT` 静态借用字符串，不得释放 |

#### 范例

[core/version_limits · 版本](../../examples/core/version_limits/main.c) · 编译期宏与运行期字符串一致

```c
printf("version=%s\n", xrtVersion());
```

## 基础类型

`int8`、`uint8`、`int16`、`uint16`、`int32`、`uint32`、`int64` 和 `uint64`
固定宽度。`ptr` 是可写无类型指针，`str` / `cstr` 是可写和只读 UTF-8
零结尾字符串，`bytes` / `cbytes` 是可写和只读字节指针。

`xtime` 是有符号 64 位 Unix Epoch 微秒，可直接用作 FFI 和跨模块时间标量。
单调时钟不使用该类型，避免把持续时间误当成绝对日期。

## 资源边界

`xrtresourcelimits` 为解析器、压缩器、归档器和其他可能放大输入的模块提供统一边界：

| 字段 | 含义 |
| --- | --- |
| `iMaxInputBytes` | 最多消费的输入字节数 |
| `iMaxOutputBytes` | 最多生成的输出字节数 |
| `iMaxItemBytes` | 单个条目、字段或文件的最大字节数 |
| `iMaxEntries` | 最大条目数 |
| `iMaxNodes` | 最大语法树或对象节点数 |
| `iMaxDepth` | 最大嵌套深度 |
| `iMaxCompressionRatio` | 最大输出输入比；输入为零时由具体模块拒绝非空输出 |
| `iFlags` | 显式允许符号链接、硬链接、设备文件或外部实体 |

允许危险资源必须显式设置 `XRT_RESOURCE_ALLOW_SYMLINKS`、
`XRT_RESOURCE_ALLOW_HARDLINKS`、`XRT_RESOURCE_ALLOW_DEVICE_FILES` 或
`XRT_RESOURCE_ALLOW_EXTERNAL_ENTITIES`。具体模块只解释与自身相关的标志。

### `xrtResourceLimitsInit`

把资源边界初始化为适合处理不受信任输入的保守默认值。

```c
void xrtResourceLimitsInit(xrtresourcelimits* pLimits);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLimits` | 输出 | 非空 | 接收清零后的保守默认边界 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 先清零整个结构再写默认值；零值字段表示不限制该项目 |

#### 范例

[core/version_limits · 资源边界](../../examples/core/version_limits/main.c) · 初始化后按需收紧

```c
printf("version=%s\n", xrtVersion());
xrtResourceLimitsInit(&Limits);
```

## 借用视图

```c
typedef struct xbytesview {
	cbytes Data;
	size_t Size;
} xbytesview;

typedef struct xstrview {
	cstr Data;
	size_t Size;
} xstrview;
```

两个 View 都只借用内存，不拥有数据，也不要求末尾补零。`Size == 0` 时
`Data == NULL` 合法。`XRT_BYTES_INIT` / `XRT_STR_INIT` 用于静态聚合初始化，
`XRT_BYTES_LITERAL` / `XRT_STR_LITERAL` 用于赋值和函数实参。

这四个宏按 `sizeof(array) - 1` 计长，只能接受编译期数组或字符串字面量，
不能用于指针：

```c
static const xstrview Protocols[] = {
	XRT_STR_INIT("h2"),
	XRT_STR_INIT("http/1.1")
};

xstrview Method = XRT_STR_LITERAL("GET");
```

## 引用计数

对象把初始计数设为一。`Retain` 原子加一并返回新值，`Release` 原子减一并
返回新值；返回零的线程负责析构对象。空指针、非正计数、复活已经归零的对象、
释放非正计数及递增 `INT32_MAX` 都返回 `-1`，且不修改计数。

这两个函数只保护计数，不发布对象字段，也不替代对象自己的并发契约。对象在
交给其他线程前仍必须通过锁、原子发布或 XRT 已声明的线程安全 API 建立
happens-before。

### `xrtRefRetain`

原子增加引用计数并返回新值。

```c
int32 xrtRefRetain(volatile int32* pCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCount` | 输入/输出 | 非空、正计数 | 对象内嵌的 `volatile int32` 计数字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `> 0` | 新计数值 | — |
| `-1` | 空指针、非正计数或达到 `INT32_MAX`（溢出保护） | 计数不变，不设置错误 |

#### 错误

- 无 — 原语按返回值报告；调用方把 `-1` 当失败处理

#### 范例

[core/reference · 共享对象](../../examples/core/reference/main.c) · 第二持有者接管前先 Retain

```c
if ( (pObject == NULL) || (xrtRefRetain(&pObject->RefCount) < 0) ) {
	return NULL;
}
```

### `xrtRefRelease`

原子减少引用计数并返回新值；返回零的线程负责析构。

```c
int32 xrtRefRelease(volatile int32* pCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCount` | 输入/输出 | 非空、正计数 | 对象内嵌计数字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `> 0` | 仍有其他持有者 | — |
| `0` | 调用方是最后持有者，执行析构 | — |
| `-1` | 空指针或非正计数（重复释放） | 计数不变，不设置错误 |

#### 错误

- 无 — 原语按返回值报告

#### 范例

[core/reference · 析构判定](../../examples/core/reference/main.c) · 归零者释放对象

```c
if ( (pObject != NULL) && (xrtRefRelease(&pObject->RefCount) == 0) ) {
```

## 内存

全库动态内存统一走这些入口，因此 `xrtSetAllocator` 可以整体替换底层分配器。
`At` 变体（`XRT_FEATURE_MEMORY_DEBUG` 下提供）记录调用位置用于泄漏诊断，语义与对应基础函数一致。

### `xrtSetAllocator`

替换进程级分配器；必须在任何 XRT 分配发生前调用。

```c
bool xrtSetAllocator(const xallocator* pAllocator);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAllocator` | 输入 | 非空、三个回调齐全 | 新分配器（Context + Alloc/Realloc/Free） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 分配器已替换 | — |
| `false` | 已有分配发生后再替换，或分配器非法 | 原分配器保持；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` — 进程内已存在 XRT 分配，无法安全替换
- `XERR_ARGUMENT` — 分配器结构或回调非法

#### 范例

[core/allocator_tour · 自定义分配器](../../examples/core/allocator_tour/main.c) · 计数分配器替换

```c
Custom.Context = (ptr)&Count;
Custom.Alloc = exampleAlloc;
Custom.Realloc = exampleRealloc;
Custom.Free = exampleFree;
if ( !xrtSetAllocator(&Custom) ) {
	goto Cleanup;
}
```

### `xrtGetAllocator`

读取当前进程分配器副本。

```c
void xrtGetAllocator(xallocator* pAllocator);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAllocator` | 输出 | 非空 | 接收当前分配器结构副本 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 副本可用于稍后恢复默认分配器 |

#### 范例

[core/allocator_tour · 恢复](../../examples/core/allocator_tour/main.c) · 先取默认，结束后还原

```c
xrtGetAllocator(&Default);
```

### `xrtMalloc`

分配指定字节的内存。

```c
ptr xrtMalloc(size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 未初始化内存 | — |
| `NULL` | 分配失败 | `XERR_MEMORY` 已设置 |

#### 错误

- `XERR_MEMORY` — 分配失败（线程错误已由分配器路径设置）

#### 范例

[core/reference · 创建对象](../../examples/core/reference/main.c) · 统一收口入口

```c
example_object* pObject = (example_object*)xrtMalloc(sizeof(example_object));
```

### `xrtCalloc`

按元素数 × 大小分配并清零。

```c
ptr xrtCalloc(size_t iCount, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iCount` | 输入 | — | 元素数 |
| `iSize` | 输入 | — | 每元素字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 已清零内存 | — |
| `NULL` | 分配失败或乘法溢出 | `XERR_MEMORY` / `XERR_RANGE` |

#### 错误

- `XERR_MEMORY` — 分配失败
- `XERR_RANGE` — `iCount * iSize` 溢出

#### 范例

[core/memory · 分配](../../examples/core/memory/main.c) · 4 字节清零分配

```c
pValues = (unsigned char*)xrtCalloc(4, sizeof(unsigned char));
if ( pValues == NULL ) {
	return 1;
}
```

### `xrtRealloc`

扩容或缩容已有分配；失败时原块保持有效。

```c
ptr xrtRealloc(ptr pMemory, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMemory` | 输入 | 允许空 | 原块；空指针等价 `Malloc` |
| `iSize` | 输入 | — | 新字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 新块（内容前 min(旧,新) 字节保留） | — |
| `NULL` | 分配失败 | 原块仍有效，调用方继续持有 |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[core/memory · 扩容](../../examples/core/memory/main.c) · 旧内容保留核对

```c
pValues = (unsigned char*)xrtRealloc(pValues, 16);
if ( pValues == NULL ) {
	return 2;
}
```

### `xrtFree`

释放分配；空指针是空操作。

```c
void xrtFree(ptr pMemory);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMemory` | 输入 | 允许空 | `Malloc`/`Calloc`/`Realloc`/`MemDup` 族产物 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作，无需判空 |

#### 范例

[core/memory · 收尾](../../examples/core/memory/main.c) · 与持有顺序相反释放

```c
xrtFree(pCopy);
xrtFree(pValues);
return 0;
```

### `xrtMemDup`

复制一段内存为新的独立分配。

```c
ptr xrtMemDup(const void* pData, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用、非空（`iSize > 0` 时） | 源字节 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 与源逐字节一致的独立副本 | — |
| `NULL` | 分配失败 | `XERR_MEMORY` |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[core/memory · 复制](../../examples/core/memory/main.c) · 分配 + memcpy 一步完成

```c
pCopy = (unsigned char*)xrtMemDup(Source, sizeof(Source));
if ( pCopy == NULL ) {
	xrtFree(pValues);   /* 失败路径也要释放已持有的资源 */
	return 3;
}
```

### `xrtSecureZero`

清零敏感内存；不会被编译器当作死存储删除。

```c
void xrtSecureZero(ptr pData, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用期间有效 | 密钥、令牌等敏感缓冲 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 普通 memset 可能被优化删除；本函数保证清零真实发生 |

#### 范例

[core/memory · 安全清零](../../examples/core/memory/main.c) · 释放敏感缓冲前擦除

```c
xrtSecureZero(pValues, 16);
xrtSecureZero(pCopy, sizeof(Source));
```

### `xrtMallocAt`

记录调用位置的 `Malloc`（`XRT_FEATURE_MEMORY_DEBUG`）；语义与 `xrtMalloc` 一致。

```c
ptr xrtMallocAt(size_t iSize, cstr sFile, uint32 iLine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iSize` | 输入 | — | 字节数 |
| `sFile` | 输入 | 静态字符串 | 通常传 `__FILE__` |
| `iLine` | 输入 | — | 通常传 `__LINE__` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 未初始化内存（位置进入分配记录） | — |
| `NULL` | 分配失败 | `XERR_MEMORY` |

#### 范例

[core/allocator_tour · At 族](../../examples/core/allocator_tour/main.c) · 位置参数不影响堆行为

```c
pBlock = (uint8*)xrtMallocAt(4u, __FILE__, __LINE__);
```

### `xrtCallocAt`

记录调用位置的 `Calloc`。

```c
ptr xrtCallocAt(size_t iCount, size_t iSize, cstr sFile, uint32 iLine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iCount` | 输入 | — | 元素数 |
| `iSize` | 输入 | — | 每元素字节数 |
| `sFile` | 输入 | — | 源文件 |
| `iLine` | 输入 | — | 行号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 已清零内存 | — |
| `NULL` | 失败或溢出 | 同 `xrtCalloc` |

#### 范例

[core/allocator_tour · At 族](../../examples/core/allocator_tour/main.c) · 清零分配带位置

```c
uint8* pZero = (uint8*)xrtCallocAt(2u, 4u, __FILE__, __LINE__);
```

### `xrtReallocAt`

记录调用位置的 `Realloc`。

```c
ptr xrtReallocAt(ptr pMemory, size_t iSize, cstr sFile, uint32 iLine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMemory` | 输入 | 允许空 | 原块 |
| `iSize` | 输入 | — | 新字节数 |
| `sFile` | 输入 | — | 源文件 |
| `iLine` | 输入 | — | 行号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 新块 | — |
| `NULL` | 分配失败 | 原块仍有效 |

#### 范例

[core/allocator_tour · At 族](../../examples/core/allocator_tour/main.c) · 扩容带位置

```c
((pBlock = (uint8*)xrtReallocAt(pBlock, 16u, __FILE__,
	__LINE__)) == NULL) ||
```

### `xrtFreeAt`

记录调用位置的 `Free`。

```c
void xrtFreeAt(ptr pMemory, cstr sFile, uint32 iLine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMemory` | 输入 | 允许空 | 要释放的块 |
| `sFile` | 输入 | — | 源文件 |
| `iLine` | 输入 | — | 行号 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作 |

#### 范例

[core/allocator_tour · At 族](../../examples/core/allocator_tour/main.c) · 配对释放带位置

```c
xrtFreeAt(pBlock, __FILE__, __LINE__);
```

### `xrtMemDupAt`

记录调用位置的 `MemDup`。

```c
ptr xrtMemDupAt(const void* pData, size_t iSize, cstr sFile, uint32 iLine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用 | 源字节 |
| `iSize` | 输入 | — | 字节数 |
| `sFile` | 输入 | — | 源文件 |
| `iLine` | 输入 | — | 行号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 独立副本 | — |
| `NULL` | 分配失败 | `XERR_MEMORY` |

#### 范例

[core/allocator_tour · At 族](../../examples/core/allocator_tour/main.c) · 复制带位置

```c
((pDup = (uint8*)xrtMemDupAt("hi", 2u, __FILE__,
	__LINE__)) == NULL) ||
```

## 错误对象

错误是引用计数的不可变对象：`Create`/`Build` 族返回拥有引用，`Ref`/`Free` 配对；`Wrap` 建立原因链并增加原因的引用。

### `xrtErrorBuild`

从完整描述创建一个错误对象。

```c
xerror* xrtErrorBuild(const xerrordesc* pDesc);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDesc` | 输入 | 非空 | 类别/域/码/操作/消息/数据/原因完整描述 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 拥有引用的错误对象 | — |
| `NULL` | 描述非法或 OOM | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 描述指针非法
- `XERR_MEMORY` — 对象或字段副本分配失败

#### 范例

[error/tour · 构建](../../examples/error/tour/main.c) · 六字段描述逐项核对

```c
pError = xrtErrorBuild(&Desc);
if ( (pError == NULL) ||
	(xrtErrorKind(pError) != XERR_ARGUMENT) ||
	(xrtErrorCode(pError) != 7) ||
	(strcmp(xrtErrorDomain(pError), "demo") != 0) ||
```

### `xrtErrorBuildAt`

从完整描述和可选源码位置创建一个错误对象。

```c
xerror* xrtErrorBuildAt(
	const xerrordesc* pDesc,
	const xerrorlocation* pLocation
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDesc` | 输入 | 非空 | 完整描述 |
| `pLocation` | 输入 | 允许空 | File/Line/Column 源码位置；空 = 无位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 带位置的错误对象 | — |
| `NULL` | 参数非法或 OOM | 同 `xrtErrorBuild` |

#### 错误

- 同 `xrtErrorBuild`

#### 范例

[error/tour · 源码位置](../../examples/error/tour/main.c) · 三定位器核对

```c
Location.File = "democ.c";
Location.Line = 10;
Location.Column = 2;
pInner = xrtErrorBuildAt(&Desc, &Location);
```

### `xrtErrorCreate`

创建一个常用错误对象。

```c
xerror* xrtErrorCreate(xerrkind Kind, cstr sDomain, int32 iCode, cstr sMessage);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Kind` | 输入 | 枚举值 | 错误类别 |
| `sDomain` | 输入 | 非空 | 稳定域字符串 |
| `iCode` | 输入 | — | 域内代码 |
| `sMessage` | 输入 | 非空 | 人类可读消息 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 拥有引用的错误对象 | — |
| `NULL` | 参数非法或 OOM | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_MEMORY`

#### 范例

[core/error · 原因链](../../examples/core/error/main.c) · 下层错误构建

```c
xerror* pCause = xrtErrorCreate(XERR_TIMEOUT, "example.net", 1, "connect timeout");
```

### `xrtErrorWrap`

创建带有原因链的错误对象；内部增加原因的引用。

```c
xerror* xrtErrorWrap(const xerror* pCause, xerrkind Kind, cstr sDomain, int32 iCode, cstr sMessage);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCause` | 输入 | 非空、借用 | 下层原因；Wrap 增加其引用 |
| `Kind` | 输入 | — | 上层类别 |
| `sDomain` | 输入 | — | 上层域 |
| `iCode` | 输入 | — | 上层代码 |
| `sMessage` | 输入 | — | 上层消息 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 上层错误，`Cause` 指向原因对象 | — |
| `NULL` | 参数非法或 OOM | 原因对象不受影响 |

#### 错误

- `XERR_ARGUMENT` / `XERR_MEMORY`

#### 范例

[core/error · 原因链](../../examples/core/error/main.c) · 包装后释放自己的原因引用

```c
pError = xrtErrorWrap(pCause, XERR_IO, "example.client", 2, "request failed");
xrtErrorFree(pCause);
```

### `xrtErrorRef`

增加错误对象引用并返回原指针。

```c
xerror* xrtErrorRef(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 非空 | 目标错误对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 原指针 | 引用 +1；须配一次 `ErrorFree` | — |
| `NULL` | 参数非法或引用耗尽 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — `pError` 为空

#### 范例

[error/tour · 共享引用](../../examples/error/tour/main.c) · Ref 与 Free 配对

```c
pRef = xrtErrorRef(pError);
if ( (pRef != pError) ) {
	goto Cleanup;
}
xrtErrorFree(pRef);  /* Ref 那份 */
```

### `xrtErrorFree`

释放错误对象引用。

```c
void xrtErrorFree(xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 要释放的引用；归零时连同原因链释放 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作 |

#### 范例

[core/error · 生命周期](../../examples/core/error/main.c) · 每份引用各自释放

```c
xrtSetError(pError);
xrtErrorFree(pError);
```

## 错误访问器

访问器全部接受空指针：消息返回 `"(no error)"` 或占位值，其余返回零/`NONE`——便于对 `xrtGetError()` 的空结果直接调用。

### `xrtErrorKind`

返回错误类别。

```c
xerrkind xrtErrorKind(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `xerrkind` | 类别枚举；空指针返回 `XERR_NONE` |

#### 范例

[error/tour · 构建](../../examples/error/tour/main.c) · 构建后核对类别

```c
(xrtErrorKind(pError) != XERR_ARGUMENT) ||
```

### `xrtErrorDomain`

返回错误的稳定域字符串。

```c
cstr xrtErrorDomain(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `cstr` | 域字符串借用；空指针返回空串 |

#### 范例

[error/tour · 构建](../../examples/error/tour/main.c) · 域核对

```c
(strcmp(xrtErrorDomain(pError), "demo") != 0) ||
```

### `xrtErrorCode`

返回域内错误代码。

```c
int32 xrtErrorCode(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `int32` | 域内代码；空指针返回 0 |

#### 范例

[error/tour · 构建](../../examples/error/tour/main.c) · 代码核对

```c
(xrtErrorCode(pError) != 7) ||
```

### `xrtErrorSystemCode`

返回原生系统错误码（若错误由系统调用失败产生）。

```c
int32 xrtErrorSystemCode(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `int32` | 平台原生错误码；无系统码时为 0 |

#### 范例

[process/open · 失败诊断](../../examples/process/open/main.c) · 打开失败时打印系统码

```c
pError != NULL ? xrtErrorSystemCode(pError) : 0
```

### `xrtErrorOperation`

返回产生错误的操作名。

```c
cstr xrtErrorOperation(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `cstr` | 操作名借用；未设置时为空串 |

#### 范例

[error/tour · 构建](../../examples/error/tour/main.c) · 操作名核对

```c
(strcmp(xrtErrorOperation(pError), "load") != 0) ||
```

### `xrtErrorMessage`

返回人类可读消息；永不返回 `NULL`。

```c
cstr xrtErrorMessage(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `cstr` | 消息借用；空错误返回 `"(no error)"` |

#### 范例

[core/error · 读取](../../examples/core/error/main.c) · 未设置错误时也有安全占位

```c
printf("error: %s\n", xrtErrorMessage(xrtGetError()));
```

### `xrtErrorData`

返回结构化附加数据字符串（如 `offset=12`）。

```c
cstr xrtErrorData(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `cstr` | 数据字符串借用；未设置时为空串 |

#### 范例

[error/tour · 构建](../../examples/error/tour/main.c) · 结构化数据核对

```c
(strcmp(xrtErrorData(pError), "ctx") != 0) ) {
```

### `xrtErrorFile`

返回错误记录的源文件名。

```c
cstr xrtErrorFile(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `cstr` | 文件名借用；未记录时为空串 |

#### 范例

[error/tour · 源码位置](../../examples/error/tour/main.c) · BuildAt 的定位器之一

```c
(strcmp(xrtErrorFile(pInner), "democ.c") != 0) ||
```

### `xrtErrorLine`

返回错误记录的源码行号。

```c
int32 xrtErrorLine(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `int32` | 行号；未记录时为 0 |

#### 范例

[error/tour · 源码位置](../../examples/error/tour/main.c) · 行号核对

```c
(xrtErrorLine(pInner) != 10) ||
```

### `xrtErrorColumn`

返回错误记录的源码列号。

```c
int32 xrtErrorColumn(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `int32` | 列号；未记录时为 0 |

#### 范例

[error/tour · 源码位置](../../examples/error/tour/main.c) · 列号核对

```c
(xrtErrorColumn(pInner) != 2) ) {
```

### `xrtErrorCause`

返回原因链中的下层错误（借用）。

```c
const xerror* xrtErrorCause(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 借用指针 | 下层原因；链尾返回 `NULL` |

#### 错误

- 无 — 链尾 `NULL` 是查询结果

#### 范例

[network/proxy_dial · 链遍历](../../examples/network/proxy_dial/main.c) · 沿链逐层打印

```c
pError = xrtErrorCause(pError);
```

### `xrtErrorIs`

沿原因链查找指定类别，命中返回借用的错误。

```c
const xerror* xrtErrorIs(const xerror* pError, xerrkind Kind);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 链起点 |
| `Kind` | 输入 | — | 要匹配的类别 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 借用指针 | 链上第一个该类别的错误 |
| `NULL` | 链上无该类别（查询结果，不设错） |

#### 错误

- 无 — 未命中是查询结果

#### 范例

[core/error · 类别判定](../../examples/core/error/main.c) · 判断失败是否由超时引起

```c
xrtErrorIs(xrtGetError(), XERR_TIMEOUT) != NULL ? "yes" : "no");
```

### `xrtErrorFind`

沿原因链查找完全匹配的错误域和代码，返回借用的错误对象。

```c
const xerror* xrtErrorFind(const xerror* pError, cstr sDomain, int32 iCode);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 链起点 |
| `sDomain` | 输入 | 非空 | 要匹配的域 |
| `iCode` | 输入 | — | 要匹配的代码 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 借用指针 | 链上第一个域 + 代码完全匹配的错误 |
| `NULL` | 未命中（查询结果，不设错） |

#### 错误

- 无 — 未命中是查询结果

#### 范例

[error/tour · 链查找](../../examples/error/tour/main.c) · 命中内层 / 未命中为空

```c
pFound = (xerror*)xrtErrorFind(pError, "inner", 42);
pMiss = xrtErrorFind(pError, "inner", 99);
```

## 线程错误上下文

每个线程有独立的错误槽。成功调用不会清除已有错误；判断前按需 `xrtClearError()`。

### `xrtGetError`

返回当前执行上下文借用的错误对象。

```c
const xerror* xrtGetError(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 无参数 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 借用指针 | 线程错误槽中的对象；借用有效期到下一次本线程错误设置 |
| `NULL` | 无错误 |

#### 错误

- 无 — 空槽返回 `NULL` 是查询结果

#### 范例

[error/tour · 读取](../../examples/error/tour/main.c) · 设置后立即读取核对

```c
if ( (xrtGetError() == NULL) ||
	(xrtErrorKind(xrtGetError()) != XERR_TIMEOUT) ) {
```

### `xrtTakeError`

取走当前执行上下文的错误对象。

```c
xerror* xrtTakeError(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 无参数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 拥有引用；槽被清空，调用方负责 `ErrorFree` | — |
| `NULL` | 槽为空 | 纯取走，不设置错误 |

#### 错误

- 无 — 空槽返回 `NULL` 是查询结果

#### 范例

[data/json_tour · 取走](../../examples/data/json_tour/main.c) · 解析失败后取走向法定位

```c
((pError = xrtTakeError()) == NULL) ||
!xrtJsonErrorLocation(pError, &Location) ||
```

### `xrtSetError`

将错误对象设置到当前执行上下文，函数会增加引用。

```c
void xrtSetError(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 要设置的错误；空 = 清除 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 槽持有自己的引用；调用方仍须释放自己的引用（“SetError 不偷引用”） |

#### 范例

[core/error · 设置](../../examples/core/error/main.c) · 设置后立即释放自己的引用

```c
xrtSetError(pError);
xrtErrorFree(pError);
```

### `xrtSetErrorTake`

将错误对象所有权转移到当前执行上下文，调用后不得继续使用原引用。

```c
void xrtSetErrorTake(xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 要转移的错误；空 = 清除 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 所有权转移；原指针此后无效 |

#### 范例

[error/tour · 所有权转移](../../examples/error/tour/main.c) · 转移后置空原指针

```c
xrtSetErrorTake(pTaken);
pTaken = NULL;  /* 所有权已转移，不得继续使用 */
```

### `xrtSetErrorInfo`

创建常用错误并直接设置到当前执行上下文。

```c
void xrtSetErrorInfo(
	xerrkind Kind,
	cstr sDomain,
	int32 iCode,
	cstr sMessage
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Kind` | 输入 | — | 类别 |
| `sDomain` | 输入 | 非空 | 域 |
| `iCode` | 输入 | — | 代码 |
| `sMessage` | 输入 | 非空 | 消息 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 一步完成创建 + 设置；OOM 时槽设置内存错误 |

#### 范例

[error/tour · 便捷设置](../../examples/error/tour/main.c) · 四参数直设

```c
xrtSetErrorInfo(XERR_RANGE, "demo", 5, "out of range");
```

### `xrtSetErrorKind`

设置无分配的通用错误；`NONE` 清除错误，无效类别设置参数错误。

```c
void xrtSetErrorKind(xerrkind Kind);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Kind` | 输入 | — | 通用类别 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 零分配路径，适合热路径 |

#### 范例

[error/tour · 通用错误](../../examples/error/tour/main.c) · 设置即触发全局 Handler

```c
xrtSetErrorKind(XERR_TIMEOUT);  /* 触发全局 Handler 一次 */
```

### `xrtClearError`

清除当前执行上下文的错误。

```c
void xrtClearError(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 无参数 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 释放槽持有的引用；后续 `GetError` 返回 `NULL` |

#### 范例

[error/tour · 清空](../../examples/error/tour/main.c) · 每段核对前清理基线

```c
xrtClearError();
xrtSetErrorInfo(XERR_RANGE, "demo", 5, "out of range");
```

### `xrtSetErrorHandler`

设置进程级错误通知处理器。

```c
void xrtSetErrorHandler(xerrorhandler pHandler, ptr pUserData);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandler` | 输入 | 允许空 | 每次任何线程设置错误时收到通知；空 = 注销 |
| `pUserData` | 输入 | 任意值 | 原样传给处理器 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 处理器在错误设置点同步执行；不得递归设置错误 |

#### 范例

[error/tour · 全局通知](../../examples/error/tour/main.c) · 计数通知后注销

```c
(void)xrtSetErrorHandler(exampleHandler, NULL);
xrtSetErrorKind(XERR_TIMEOUT);  /* 触发全局 Handler 一次 */
```

## 生命周期

XRT 2.0 的核心按需初始化内部进程资产，不再要求每个程序配对调用
`xrtInit()` / `xrtUnit()`。有状态模块都由自己的创建和销毁函数管理对象，
进程级分配器和共享缓存保持到进程结束。

旧版公开可变 `xCore` 把应用路径、错误回调、分配器、近似比较策略、线程状态
和内存池混入一个结构，导致模块边界与裁剪失效。新版将这些能力归入各自模块，
不保留第二套全局兼容入口。

## 旧版资产决策

新版保留旧 `xrt.h` / `base.h` 中简短类型名称、微秒 `xtime`、原子引用及
无需复杂对象即可使用的风格，并补齐固定宽度、只读指针和显式借用 View。
旧 `test_base.h` 的有效引用边界由核心单元测试和并发测试承接；可变全局状态、
隐式字符串所有权和初始化引用计数被明确退役。

完整范例位于 `examples/core/`（error / memory / reference / version_limits /
allocator_tour）与 `examples/error/tour`。
