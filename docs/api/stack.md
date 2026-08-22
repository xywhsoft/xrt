# FixedStack、PtrFixedStack、Stack、BlockStack 与 PtrStack

Stack 体系提供五种成本清晰的后进先出容器：`fixed_stack` 使用固定容量缓冲；
`ptr_fixed_stack` 提供无分配固定指针栈；`stack` 使用可增长连续数组；
`block_stack` 按块增长并保持活动元素地址稳定；`ptr_stack` 在连续动态栈上提供指针类型友好接口。
各层不隐式加锁，也不析构元素内部资源。

## 裁剪与依赖

| 能力 | 宏 | 依赖 |
|---|---|---|
| 固定容量栈 | `XRT_FEATURE_FIXED_STACK` | `core` |
| 固定容量指针栈 | `XRT_FEATURE_PTR_FIXED_STACK` | `fixed_stack` |
| 动态栈 | `XRT_FEATURE_STACK` | `array` |
| 稳定地址分块栈 | `XRT_FEATURE_BLOCK_STACK` | `array` |
| 指针栈 | `XRT_FEATURE_PTR_STACK` | `stack` |

固定栈不依赖动态数组。只需要本地有界工作栈时，不会带入任何扩容代码。
BlockStack 与连续 Stack 可分别裁剪；两者都只复用 Array 的块索引或连续存储底座。
启用 `ptr_stack` 必须同时启用 `stack` 和 `array`，公共头会拒绝所有不完整依赖。

## 共同契约

- 栈顶是最后压入的元素，`Push`、`Top`、`Pop` 均为 O(1) 摊销操作；固定栈始终为 O(1)。
- `Push` 浅复制一个完整元素；`Add` 返回未初始化新槽，调用方必须立即写满。
- `Get(index)` 使用统一的 0 基栈底索引；`Peek(depth)` 从栈顶计数，`depth == 0` 等价于 `Top`。
- `Top`、`Get` 和 `Peek` 返回栈内借用地址。FixedStack 地址在生命周期内稳定；连续 Stack 扩容可能移动全部元素；BlockStack 的活动元素在 Push、Pop 和 Reserve 后仍保持地址稳定。
- `Pop` 把值复制到外部缓冲后再删除，不返回失活槽地址。输出可以为 `NULL`，表示只删除栈顶；输出不得与同一栈的存储区重叠。FixedStack 和连续 Stack 会主动检查这一条件；BlockStack 的低成本前置条件见其专节。
- 容器只管理元素字节。结构体中的指针、句柄和其他资源仍由调用方管理。
- 空栈 `Top` 或 `Pop` 返回失败并设置 `XERR_RANGE`。固定栈已满时 `Add` 或 `Push` 设置 `XERR_AGAIN`，且原栈不变。
- 容器不隐式加锁。一个栈同时只能由一个执行流修改；跨线程共享时使用外部同步。

## FixedStack

### 类型

```c
typedef struct xfixedstack {
	bytes Data;
	ptr Allocation;
	size_t ItemSize;
	size_t Count;
	size_t Capacity;
} xfixedstack;
```

`Data` 是连续元素区，`Count` 是当前深度，`Capacity` 是固定上限。
`Allocation` 只在 `xrtFixedStackCreate()` 创建的拥有型固定栈中非空。
结构公开用于热路径读取状态，不允许调用方修改不变量。

### 生命周期

```c
bool xrtFixedStackInit(
	xfixedstack* pStack,
	ptr pMemory,
	size_t iMemorySize,
	size_t iItemSize
);

xfixedstack* xrtFixedStackCreate(size_t iCapacity, size_t iItemSize);
void xrtFixedStackUnit(xfixedstack* pStack);
void xrtFixedStackDestroy(xfixedstack* pStack);
void xrtFixedStackClear(xfixedstack* pStack);
size_t xrtFixedStackSpace(const xfixedstack* pStack);
```

`Init` 借用调用方缓冲，不分配也不释放它，容量为 `iMemorySize / iItemSize`，尾部不足一个元素的字节不使用。缓冲不得与 `xfixedstack` 结构本身重叠，否则后续元素写入会破坏元数据并以 `XERR_ARGUMENT` 拒绝初始化。缓冲还必须满足所存类型的 C 对齐要求；最简单可靠的写法是直接传入同类型数组。

`Create` 分配栈结构和固定数据区，容量在生命周期内不变。它使用全局堆的常规对齐；需要过对齐元素时，应使用带正确对齐的外部数组和 `Init`。

`Unit` 只释放 `Create` 取得的数据区，并把结构归零；外部缓冲不受影响。`Destroy` 只用于 `Create` 返回的结构。`Clear` 保留容量，`Space` 返回还能压入的元素数。

### LIFO 操作

```c
ptr xrtFixedStackAdd(xfixedstack* pStack);
bool xrtFixedStackPush(xfixedstack* pStack, const void* pItem);
bool xrtFixedStackPop(xfixedstack* pStack, ptr pItem);
ptr xrtFixedStackGet(xfixedstack* pStack, size_t iIndex);
const void* xrtFixedStackConstGet(const xfixedstack* pStack, size_t iIndex);
ptr xrtFixedStackPeek(xfixedstack* pStack, size_t iDepth);
const void* xrtFixedStackConstPeek(const xfixedstack* pStack, size_t iDepth);
ptr xrtFixedStackTop(xfixedstack* pStack);
const void* xrtFixedStackConstTop(const xfixedstack* pStack);
```

`Push` 允许来源是当前活动区中的一个完整元素，因此可以复制旧栈帧到新栈顶。来源触及未使用容量、跨元素边界或只有部分重叠时失败。

固定栈适合解析器深度上限、小型回溯、协议状态机和实时路径。它的运行期不会分配内存，满栈是必须处理的正常背压状态。

固定栈的 `Add/Pop`、`Push/Pop` 和 `ConstTop` 性能基线位于
`dev/bench/fixed_stack/FIXED_STACK_BENCH_20260728.md`，用于约束后续安全检查的热路径成本。

## PtrFixedStack

```c
typedef xfixedstack xptrfixedstack;

bool xrtPtrFixedStackInit(
	xptrfixedstack* pStack,
	ptr* pMemory,
	size_t iCapacity
);
xptrfixedstack* xrtPtrFixedStackCreate(size_t iCapacity);
void xrtPtrFixedStackUnit(xptrfixedstack* pStack);
void xrtPtrFixedStackDestroy(xptrfixedstack* pStack);
void xrtPtrFixedStackClear(xptrfixedstack* pStack);
size_t xrtPtrFixedStackSpace(const xptrfixedstack* pStack);
ptr xrtPtrFixedStackGet(const xptrfixedstack* pStack, size_t iIndex);
bool xrtPtrFixedStackPush(xptrfixedstack* pStack, ptr pValue);
bool xrtPtrFixedStackPop(xptrfixedstack* pStack, ptr* pValue);
ptr xrtPtrFixedStackPeek(const xptrfixedstack* pStack, size_t iDepth);
ptr xrtPtrFixedStackTop(const xptrfixedstack* pStack);
```

`Init` 直接借用调用方的 `ptr[]`，容量参数使用元素数量而不是字节数。指针数组不得与栈结构本身重叠。整个生命周期可以完全不分配内存，适合固定上限的资源回滚、清理动作和句柄栈。`Create` 则复用 FixedStack 的拥有型固定缓冲。

PtrFixedStack 只保存指针值，不拥有目标。它允许保存合法 `NULL`；
此时 `Get`、`Peek` 或 `Top` 返回 `NULL` 且不设置新错误，空栈或越界则设置 `XERR_RANGE`。
需要区分合法空值与失败时，应在读取前清除旧错误，再检查本次调用后的错误状态；
已知索引小于公开 `Count` 时，返回的 `NULL` 就是合法值。
所有入口都会拒绝元素宽度不是 `sizeof(ptr)` 的 FixedStack，
避免旧版“在更宽结构体前几个字节写入指针”的隐式布局。

## Stack

### 类型与底座

```c
typedef xarray xstack;
```

动态栈直接复用 Array 的连续存储。需要中间插入、排序等数组能力时，可以使用同一对象调用公开的 `xrtArray*` 底层 API；普通栈访问使用 `Get`、`Peek` 和 `Top`，不需要跨模块调用。

连续 Stack 取代旧版 DynStack 作为默认动态栈，减少重复内存管理并改善连续遍历和缓存局部性；
旧版有价值的分块稳定地址能力由独立 BlockStack 承接。
任何连续 Stack 的 `Top`、`Get` 或 `Add` 返回地址都只能保留到下一次可能扩容或重排的操作之前。

连续栈的预留 `Push/Pop` 与摊销增长性能基线位于
`dev/bench/stack/STACK_BENCH_20260728.md`，用于约束包装层和 Array 增长策略的性能回退。

### 生命周期与容量

```c
bool xrtStackInit(xstack* pStack, size_t iItemSize);
bool xrtStackInitAligned(xstack* pStack, size_t iItemSize, size_t iAlignment);
xstack* xrtStackCreate(size_t iItemSize);
xstack* xrtStackCreateAligned(size_t iItemSize, size_t iAlignment);
void xrtStackUnit(xstack* pStack);
void xrtStackDestroy(xstack* pStack);
void xrtStackClear(xstack* pStack);
bool xrtStackReserve(xstack* pStack, size_t iCapacity);
bool xrtStackTrim(xstack* pStack);
```

生命周期和对齐规则与 Array 完全一致。`Reserve` 可在热路径前一次性准备深度，`Clear` 保留容量，`Trim` 才主动缩小到当前深度。扩容 OOM 时地址、深度、容量和已有元素保持不变。

### LIFO 操作

```c
ptr xrtStackAdd(xstack* pStack);
bool xrtStackPush(xstack* pStack, const void* pItem);
bool xrtStackPop(xstack* pStack, ptr pItem);
ptr xrtStackGet(xstack* pStack, size_t iIndex);
const void* xrtStackConstGet(const xstack* pStack, size_t iIndex);
ptr xrtStackPeek(xstack* pStack, size_t iDepth);
const void* xrtStackConstPeek(const xstack* pStack, size_t iDepth);
ptr xrtStackTop(xstack* pStack);
const void* xrtStackConstTop(const xstack* pStack);
```

`Push` 继承 Array 的自引用支持：来源可以是当前完整活动元素，即使本次压栈触发扩容也能得到正确副本。`Pop` 拒绝输出到栈自身存储区，失败时不减少深度。

## BlockStack

### 用途与存储合同

BlockStack 面向需要“深度未知、LIFO、活动元素地址不能随增长改变”的解析器、解释器、递归展开器和大型工作帧。它把元素放入独立数据块，只让小型块指针索引随 Array 扩容；因此添加新块不会搬移旧块元素。

```c
typedef struct xblockstack {
	xarray Blocks;
	size_t ItemSize;
	size_t Count;
	size_t Capacity;
	size_t BlockItems;
	size_t Alignment;
} xblockstack;
```

`Blocks` 的元素类型属于内部实现，只允许读取 `Count` 和 `Capacity` 做诊断。`Capacity` 始终等于 `Blocks.Count * BlockItems`。调用方不得修改公开摘要。

默认布局以 16 KiB 为块字节目标，并把每块元素数限制在 1 到 256 之间：
小元素延续旧 DynStack 的 256 元素块，大元素自动减少块深度，
不再发生“一个大对象乘以 256”的突发浪费。
需要确定延迟或特殊对齐时使用 `InitLayout` 或 `CreateLayout`。

### 生命周期与容量

```c
bool xrtBlockStackInit(xblockstack* pStack, size_t iItemSize);
bool xrtBlockStackInitLayout(
	xblockstack* pStack,
	size_t iItemSize,
	size_t iAlignment,
	size_t iBlockItems
);
xblockstack* xrtBlockStackCreate(size_t iItemSize);
xblockstack* xrtBlockStackCreateLayout(
	size_t iItemSize,
	size_t iAlignment,
	size_t iBlockItems
);
void xrtBlockStackUnit(xblockstack* pStack);
void xrtBlockStackDestroy(xblockstack* pStack);
void xrtBlockStackClear(xblockstack* pStack);
bool xrtBlockStackReserve(xblockstack* pStack, size_t iCapacity);
bool xrtBlockStackTrim(xblockstack* pStack);
```

`Reserve` 先取得全部新数据块，再一次性扩展块索引；
任一分配失败都会释放临时块，保持深度、容量、块数和既有地址不变。
`Pop` 和 `Clear` 不隐式释放块，避免块边界反复分配；
`Trim` 才释放当前深度不需要的尾部块。
`Trim` 保留很小的块索引缓存，`Unit` 释放全部内存。
所有公开操作都会检查块布局尺寸和摘要乘法；即使调用方意外破坏公开字段，
也不会把溢出的尺寸送入分配器。访问和裁剪还会验证实际涉及的数据块。

### LIFO 与低层访问

```c
ptr xrtBlockStackAdd(xblockstack* pStack);
bool xrtBlockStackPush(xblockstack* pStack, const void* pItem);
bool xrtBlockStackPop(xblockstack* pStack, ptr pItem);
ptr xrtBlockStackGet(xblockstack* pStack, size_t iIndex);
const void* xrtBlockStackConstGet(const xblockstack* pStack, size_t iIndex);
ptr xrtBlockStackPeek(xblockstack* pStack, size_t iDepth);
const void* xrtBlockStackConstPeek(const xblockstack* pStack, size_t iDepth);
ptr xrtBlockStackTop(xblockstack* pStack);
const void* xrtBlockStackConstTop(const xblockstack* pStack);
```

BlockStack 的常用 LIFO 和随机检查路径均为 O(1)；新块分配是摊销成本。`Push` 可以直接复制任一仍活动的 BlockStack 元素，因为既有块不会移动。

为保持 O(1) 热路径，BlockStack 不线性扫描全部块来判断任意输入或输出指针是否属于自身。调用方必须保证 `Push` 来源可读完整 `ItemSize` 字节，并保证 `Pop` 输出不与该栈任何块重叠。

这是 C 指针前置条件；违反时行为未定义。连续 Stack 和 FixedStack 可以常数时间判断完整存储区，因此会主动拒绝内部输出别名。

## PtrStack

```c
typedef xstack xptrstack;

bool xrtPtrStackInit(xptrstack* pStack);
xptrstack* xrtPtrStackCreate(void);
void xrtPtrStackUnit(xptrstack* pStack);
void xrtPtrStackDestroy(xptrstack* pStack);
void xrtPtrStackClear(xptrstack* pStack);
bool xrtPtrStackReserve(xptrstack* pStack, size_t iCapacity);
bool xrtPtrStackTrim(xptrstack* pStack);
ptr xrtPtrStackGet(const xptrstack* pStack, size_t iIndex);
bool xrtPtrStackPush(xptrstack* pStack, ptr pValue);
bool xrtPtrStackPop(xptrstack* pStack, ptr* pValue);
ptr xrtPtrStackPeek(const xptrstack* pStack, size_t iDepth);
ptr xrtPtrStackTop(const xptrstack* pStack);
```

指针栈允许保存合法 `NULL`。此时 `Top` 返回 `NULL` 但不会设置新错误；
空栈也返回 `NULL`，同时设置 `XERR_RANGE`。
需要无歧义处理时，在调用前清除错误并检查 `xrtGetError()`，或直接使用 `Count`。

`Pop` 返回布尔结果，因此能无歧义弹出空指针。输出参数可以为空，表示丢弃栈顶。清空、弹出和销毁都不会释放指针目标。
类型化入口会检查底层连续栈确实使用 `sizeof(ptr)` 元素，并直接复用已验证的
Array 增长原语；因此 `Push` 不需要通用字节复制和来源别名分析。分配失败保持
地址、深度、容量和已有指针值不变。`Pop` 继续复用通用栈的输出别名检查，
若输出指向自身存储区则设置 `XERR_ARGUMENT` 且不删除栈顶。

## 错误与原子性

| 场景 | 错误种类 |
|---|---|
| 空结构、空来源、零元素大小或无效外部缓冲 | `XERR_ARGUMENT` |
| 公开结构摘要损坏 | `XERR_STATE` |
| 空栈访问、尺寸乘法或容量取整溢出 | `XERR_RANGE` |
| 固定栈容量已满 | `XERR_AGAIN` |
| 创建、预留、连续扩容或新块分配失败 | `XERR_MEMORY` |

所有失败操作都保持原有深度和已有元素。BlockStack 的 `Reserve` 还保证失败时不保留部分数据块。成功和正常状态查询不会主动清除调用前已有错误。

## 示例

### 固定工作栈

```c
frame Storage[64];
xfixedstack Frames;

if ( xrtFixedStackInit(&Frames, Storage, sizeof(Storage), sizeof(frame)) ) {
	frame Value = { 0 };
	xrtFixedStackPush(&Frames, &Value);
	xrtFixedStackPop(&Frames, &Value);
	xrtFixedStackUnit(&Frames);
}
```

### 无分配固定指针栈

```c
ptr Storage[32];
xptrfixedstack Cleanup;

xrtPtrFixedStackInit(&Cleanup, Storage, 32);
xrtPtrFixedStackPush(&Cleanup, pResource);
xrtPtrFixedStackPop(&Cleanup, &pResource);
xrtPtrFixedStackUnit(&Cleanup);
```

### 动态和指针栈

```c
xstack Values;
xptrstack Objects;

xrtStackInit(&Values, sizeof(value));
xrtPtrStackInit(&Objects);

xrtStackPush(&Values, &Value);
xrtPtrStackPush(&Objects, pObject);

xrtStackUnit(&Values);
xrtPtrStackUnit(&Objects);
```

### 稳定地址分块栈

```c
xblockstack Frames;
frame* pRoot;

xrtBlockStackInitLayout(&Frames, sizeof(frame), _Alignof(frame), 64);
pRoot = (frame*)xrtBlockStackAdd(&Frames);

/* 跨过任意多个块后，仍可使用活动的 pRoot。 */
for ( size_t i = 0; i < 10000; i++ ) {
	frame* pFrame = (frame*)xrtBlockStackAdd(&Frames);
	(void)pFrame;
}

xrtBlockStackUnit(&Frames);
```

完整可编译示例位于 `examples/containers/fixed_stack`、`examples/containers/ptr_fixed_stack`、
`examples/containers/stack`、`examples/containers/block_stack` 和
`examples/containers/ptr_stack`。
