# Core API

`core` 是 XRT 唯一不可裁剪的公共底座，提供版本、固定宽度类型、借用视图和
引用计数。它不需要显式初始化，也不依赖线程、容器、文件或网络模块。

```c
#include <xrt/core.h>
```

聚合入口 `<xrt.h>` 会自动包含核心、[内存](memory.md)和[错误](error.md)
三个头文件。

## Windows 动态嵌入的最终线程存储退役

`bool xrtRuntimeRetireThreadStorage(void)` 退役**当前 XRT 实例**注册的内部
FLS/TLS 槽，包括堆缓存、默认错误、默认临时 arena、TinyCC 随机/故障注入状态、
Future 通知与线程/协程借用槽。清理顺序是 payload → error → heap → borrowed，
不依赖惰性初始化的先后。它不会禁用正常运行期间的线程缓存。

宿主必须先关闭所有调用/回调入口，等待 XRT 工作结束，释放外部持有对象、任务、
动态线程键等资源，并在各自线程清理动态键、解绑执行上下文。宿主线程和 Fiber
可以保持存活，但不得再进入该实例，也不得与退役并发退出或销毁 Fiber。
之后，在代码和分配器仍驻留时、`DllMain` 和加载器锁之外调用此接口，成功返回后
才可卸载代码。FLS 槽释放会同步析构所有 Fiber 的非空值；先保留错误和堆的依赖，
最后移除保护槽。不会在持有注册表锁时执行这些析构。

这是终态操作，不是 reset：成功后重复调用返回 true；失败可能已经完成部分清理，
只能保持代码驻留并重试，不能恢复业务或调用 XRT 错误接口。新槽注册被关闭，
重入/并发退役返回 false。本接口自身不分配内存、不访问错误 TLS。
非 Windows 平台当前返回 false 且不改变状态；不宣称支持 POSIX 共享库退役。

此接口只消除内部线程存储的代码引用，不收集对象图、不判定外部引用是否已经释放，
不清空中央堆 span 或其他模块级资源。对象和代码租约的正确性仍由宿主负责。

## 类型与常量

### 完整拥有关系检查（不是垃圾收集或代码保活）

`xrtOwnershipInspect(anchors, anchorCount, internalSlots, slotCount, result)`
在调用方提供的静止安全点检查真实强引用图。每个 `xrtownershipref` 是物理身份
`Data` 和稳定的 `Ops`；`Count` 读实际强引用数，`Trace` 枚举每个实际拥有槽。
重复指向同一节点的两个槽仍是两条边，但一个共享节点只展开一次。

`anchors` 只提供待观察节点和图发现入口，不算拥有引用；`internalSlots`
代表即将退役的外层所有者（如模块全局变量）实际持有的每个强引用槽。不可把
所有 anchor 都当作内部引用扣除，否则会吞掉宿主真正拥有的引用。

检查将物理入边数从强引用数中扣除，余数大于零的节点是外部根；从这些根向下
传播可达性。`ReachableAnchorCount` 为外部可达的唯一 anchor 数量。
静态标量是无出边的常驻叶子，不会反向保活其父节点。

Value 通过 `xrtValueOwnership` 暴露**外壳 → backing → 元素外壳**。
Value 别名保留同一外壳的实际引用；COW 别名保留共享 backing 的实际引用；
多个父节点共享一个 backing 时，不会重复计算 backing 持有的子边。
Handle 的 producer 必须用 `xrtValueHandleOwnershipBind` 提供完整边适配器；
克隆传播适配器，但由新外壳读取自己的 handle，不能缓存旧 handle 地址。
Value 弱引用和 runtime-object 弱引用不是强拥有边。未知 handle（含其
UserData）明确检查失败。非空 finalizer context 由
`xrtValueObjectFinalizerOwnershipBind` 独立描述；它归属于 backing，不是每个
共享外壳各有一份。非空 IdentityUserData 目前没有完整图适配合同，检查会拒绝，
不能猜测它不持有资源。

`xrtOwnershipInspectReachable` 还按输入顺序写出每个 anchor 的可达性位，
重复身份得到相同结果，NULL anchor 为 false。失败时位数组和统计均不修改。
可选的只读 root policy 只能增加保守根，不能消除实际外部根、伪造引用计数
或跳过出边；用于保护本次收集域之外仍可被弱引用观察的对象。

返回 false 时不写结果、不改变图、不调用析构；覆盖分配失败、错误边计数、
同一身份的冲突 descriptor、Trace/Count 失败。忽略 visitor 的失败也不能
把检查改成成功。实现使用迭代遍历和身份哈希表，没有递归深度截断。

此 API **不建立安全点、不阻止并发修改、不持有代码租约、不清环、不调用 Drop**。
调用方必须先保证图和描述符驻留、整个图停止变更，再持有这个保证直到后续
生命周期决策提交。仅检查结果为零不能直接当作可并发卸载代码的授权。

验证入口：`tools/build.py --suite ownership_graph_tests`；分别覆盖模块化和单头。

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
| `XSEEK_START` | 从文件起点 |
| `XSEEK_CURRENT` | 从当前位置 |
| `XSEEK_END` | 遍历结束 |

### `xrtresourcelimits`

资源限额结构：限制输入/输出/单项字节数、条目数、节点数、深度与压缩比；`iFlags` 启用符号链接/硬链接/设备文件/外部实体的放行位。结构带 `iSize`/`iVersion` 前向兼容字段，未指定字段保持零值。


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

进度事件标志：`TOTAL_KNOWN` 表示总输入量已知，`FINAL` 表示本次事件为最后一次。


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

长耗时流操作共用的进度事件（带 `iSize`/`iVersion` 前向兼容字段）；回调仅在发起操作的线程内同步调用。


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
| `Data` | `cbytes` | 数据 |
| `Size` | `size_t` | 字节数 |

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
| `Data` | `cstr` | 数据 |
| `Size` | `size_t` | 字节数 |

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
| `XERR_NOT_FOUND` | 未找到 |
| `XERR_EXISTS` | 已存在 |
| `XERR_PERMISSION` | 权限不足 |
| `XERR_AGAIN` | 暂不可推进 |
| `XERR_TIMEOUT` | 超时 |
| `XERR_CANCELLED` | 已取消 |
| `XERR_CLOSED` | 已关闭 |
| `XERR_PROTOCOL` | 协议非法 |
| `XERR_UNSUPPORTED` | 不支持 |
| `XERR_INTERNAL` | 内部不变量破坏 |

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
| `Kind` | `xerrkind` | 错误种类 |
| `Code` | `int32` | 错误码 |
| `SystemCode` | `int32` | 平台错误码 |
| `Domain` | `cstr` | 错误域 |
| `Operation` | `cstr` | 失败操作名 |
| `Message` | `cstr` | 消息文本 |
| `Data` | `cstr` | 数据 |
| `Cause` | `const xerror*` | 原因链 |

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
| `File` | `cstr` | 文件名 |
| `Line` | `int32` | 行号 |
| `Column` | `int32` | 列号 |

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
| `Context` | `ptr` | 回调上下文 |
| `Alloc` | `xallocproc` | Alloc |
| `Realloc` | `xreallocproc` | Realloc |
| `Free` | `xfreeproc` | 空闲量 |

### `xallocproc`

自定义底层分配器回调。

```c
typedef ptr (*xallocproc)(ptr pContext, size_t iSize);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xreallocproc`

底层分配器的重分配回调：把 `pMemory` 调整为 `iSize` 字节，失败返回 `NULL`；`pMemory == NULL` 等价分配，`iSize == 0` 等价释放。


```c
typedef ptr (*xreallocproc)(ptr pContext, ptr pMemory, size_t iSize);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xfreeproc`

底层分配器的释放回调：释放 `pMemory`；`pMemory == NULL` 应为空操作。


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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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
|---|---|---|
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

### 拥有关系的并发准入

`xrtOwnershipMutationBegin`、`xrtOwnershipFreezeTryBegin` 和
`xrtOwnershipScopeEnd` 为同一份已链接 XRT 提供一个共同冻结的协作域。scope 在
调用栈上零初始化，不分配堆或 TLS；不能复制、移动、跨原生线程结束，也不是
对象强引用或代码租约。多个 mutation 可同时存在，嵌套调用也可继续进入。
freeze 只尝试独占准入：存在任一未结束 mutation 时立即返回 false，保留
scope 和既有错误，不排队等待、不把当前 reader 升级为 writer。

取得 freeze 后，其他线程的参与操作等待 ScopeEnd；持有线程可嵌套平衡的
mutation，用于只读 Trace 的临时游标或已知的原子提交步骤。父 freeze 不能
早于其子 scope 结束。成功进入/退出保持既有错误；非法参数、重复进入、复制
scope、跨线程或重复退出被拒绝。scope 必须从参与对象自己的锁之外进入，覆盖
整个状态转换，不能先持有对象锁再等待 freeze，否则会形成锁序反转。

当前自动参与的生产路径：

- `xrtRefRetain/Release` 的实际原子计数更新；原 CAS、死亡引用和溢出语义不变。
- Value 的强/弱保有、复制、拥有式接管、类型/identity/生命周期绑定、最后释放。
- Value 容器的字段写入、COW、清空/取出、容量调整、游标生命周期和集合合并；
  DeepClone/Equal 及直接 Hash/ScalarEqual 回调也在其完整外层转换内。私有 body 配合统一返回
  包装，所有提前返回都归还准入，不只锁住一条 RC 指令。
- Future/Promise 的生产端双计数、引用/销毁、终态发布、转发及 Watch 状态转换；
  Cancel 父链创建/销毁和监听装配/请求/注销；Any/All/Race 和 continuation
  从共同工厂入口覆盖完整装配、立即回调和失败回滚。通知/Release/析构尚未
  返回时仍不能冻结。阻塞 FutureWait 和协程 park 不持有跨等待的 scope，
  它们实际保有的 Future 引用仍按外部根计算，内部 waiter 变更单独受保护。

这些 mutation 之间仍是并发关系，并不让同一容器的任意读写自动线程安全。
调用者原有的同对象同步责任不变，宿主也无需把普通 Value Retain 改成 root
登记 API。每个运行时实例只有一个域，跨 XRT 实例不能互相冻结。普通准入按
原生线程 ID 分散到 64 个独立缓存行的分片，线程散列碰撞仍保持正确性；只有
freeze 才访问协调锁并尝试一次性锁定全部分片，任一分片繁忙就归还已经取得
的锁。冻结指针在任一分片锁下读取、全部分片锁下发布或撤销，未增加线程登记
或 TLS 初始化。该方案消除了单一热计数器，但尚不是高争用性能验收结论。

**尚不能据此卸载语言模块。** 用户回调拥有的数据、task/transport、native
object/callable 状态、生成的 cell/frame/global 写入、其他直接原子计数和跨
模块入口仍须接通。旧 opaque waiter 仍拒绝图检查，不能因为分发有 scope 就
把缺失的拥有边视为空边。
冻结期间的 Trace、分配器及错误通知也必须满足不会等待被阻塞参与者的合同；
任意用户回调、线程 join、fiber 挂起不能放在 freeze 内。收集器需要在所有
节点能力都得到证明之后建立稳定快照并原子接管，在退出 freeze 后执行用户
析构，最后才决定是否可以卸载代码。mutation 接口本身不提供候选节点 claim/commit，
不替代 `xrtOwnershipInspect` 对完整图静止与代码驻留的要求。

专项入口为 `--suite ownership_scope_tests`，同时保留原有 ref/ref-threads
测试。它覆盖真实原生弱提升、容器 COW/合并/取出与 freeze 的竞争，以及字段
析构或直接 identity 哈希尚未返回时拒绝检查准入；没有把这些专项解释成模块
环收集已经接通。

分片专项额外启动 65 个同时存活的原生 mutation（必然包含分片碰撞），逐个
退出并检查最后一个 reader 之前都不能冻结；四个并发收集线程各自取得至少
30 次独占窗口，确认互斥和失败准入回滚，不将尝试次数算作成功次数。

XRT 2.0 的核心按需初始化内部进程资产，不再要求每个程序配对调用
`xrtInit()` / `xrtUnit()`。有状态模块都由自己的创建和销毁函数管理对象，
进程级分配器和共享缓存保持到进程结束。

旧版公开可变 `xCore` 把应用路径、错误回调、分配器、近似比较策略、线程状态
和内存池混入一个结构，导致模块边界与裁剪失效。新版将这些能力归入各自模块，
不保留第二套全局兼容入口。

## 结构快照与显式接管协议

`xrtOwnershipSnapshotCreate/NodeCount/Node/Destroy` 复用 Inspect 的同一物理图算法。
每个节点保存真实强计数、内部入边计数和可达性；不保有节点。准入回调先于该
节点的 Count/Trace，可在调用未知代码之前拒绝整个图。节点顺序是首次发现
顺序：先输入 anchor，再 internal slot，最后展开出边。重复物理身份只出现一次。
失败不修改输出；读取节点不重新扫描、不分配，销毁只释放快照记账。
调用方必须独立保持完整图静止及节点、回调代码有效，直到建立真正的保有引用。

`xrtownershipadapterv1` 是独立的生命周期协议，不扩展旧二函数 `xrtownershipops`
的内存布局。解析器须证明完整 mutation 参与与上下文能力，不能仅凭 Trace 存在
就认证。Hold/Drop 对应实际引用；Claim/Restore 以独占 token 隔离弱/raw 新准入，
允许已有合法强拥有者在 Finalize 后产生可检测的复活。Finalize 在 freeze 外运行，
其物理“已执行”状态跨撤销保留。重新判断根、检查新析构职责与 Clear 必须在同一
freeze 内完成；Clear 不能分配、失败、等待或运行用户代码。Finish 在 freeze 外
处理已准备的机械释放尾部，不得开始新的用户语义析构或发布已接管的 receiver。

XRT 提供普通 Value/backing、明确授权的对象/Handle 生命周期适配器，见 Value
文档；完整事务协调由上层实现。`xrtErrorOwnershipAdapterV1` 另外识别本实例的
不可变 Error/Cause DAG：文本内联拥有，Cause 只指向更早的错误，不存在回指语言
模块的边或用户回调。其 Clear 不修改不可变数据，真实 Cause 边保留到最后一次
Drop；外部错误别名因此不会反向保活模块。这不是 XRT 自动 GC，也不证明任意
原生上下文或语言模块可以回收。

## 析构前语义准备

`xrtownershippreparationv1` 与生命周期表是两个独立、不可变的描述符；`Adapter`
必须精确指向同一生命周期表，不能扩展旧表或把未知描述符降级为空准备职责。
`Ready` 在 freeze 内只读检查，`Prepare` 在 freeze/mutation 外运行，可正常通知
已受理的回调。READY 后必须重建完整物理图，再决定下一项准备或析构；通知可能
增加新节点或外部引用，不能继续使用通知前的可达性。每一次对象 Finalize 后也须
重新检查，防止析构新建资源后提前析构其捕获对象。

BUSY 不允许本次准备调用产生语义回调或改变拥有边，可非阻塞地尝试其他依赖。
FAILED 是显式失败，即使没有诊断对象也不能当作成功。上层应区分快照/准入失败
和语义调用失败；已经发生的通知和物理析构职责不能因撤销、分配失败或重试而重放。

## 旧版资产决策

新版保留旧 `xrt.h` / `base.h` 中简短类型名称、微秒 `xtime`、原子引用及
无需复杂对象即可使用的风格，并补齐固定宽度、只读指针和显式借用 View。
旧 `test_base.h` 的有效引用边界由核心单元测试和并发测试承接；可变全局状态、
隐式字符串所有权和初始化引用计数被明确退役。

完整范例位于 `examples/core/`（error / memory / reference / version_limits /
allocator_tour）与 `examples/error/tour`。
