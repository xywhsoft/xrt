# FixedStack、PtrFixedStack、Stack、BlockStack 与 PtrStack

Stack 体系提供五种成本清晰的后进先出容器：`fixed_stack` 使用固定容量缓冲；
`ptr_fixed_stack` 提供无分配固定指针栈；`stack` 使用可增长连续数组；
`block_stack` 按块增长并保持活动元素地址稳定；`ptr_stack` 在连续动态栈上提供指针类型友好接口。
各层不隐式加锁，也不析构元素内部资源。

## 类型与常量

### `xfixedstack`

固定栈可借用外部缓冲，也可拥有创建时分配的固定缓冲。

```c
typedef struct xfixedstack {
	bytes Data;
	ptr Allocation;
	size_t ItemSize;
	size_t Count;
	size_t Capacity;
} xfixedstack;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `bytes` | Data |
| `Allocation` | `ptr` | Allocation |
| `ItemSize` | `size_t` | ItemSize |
| `Count` | `size_t` | Count |
| `Capacity` | `size_t` | Capacity |

### `xblockstack`

* 分块栈只移动块索引，不移动块内元素。 * Blocks 的元素类型属于内部实现，调用方只能读取其 Count 和 Capacity 做诊断。

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

| 字段 | 类型 | 语义 |
|---|---|---|
| `Blocks` | `xarray` | Blocks |
| `ItemSize` | `size_t` | ItemSize |
| `Count` | `size_t` | Count |
| `Capacity` | `size_t` | Capacity |
| `BlockItems` | `size_t` | BlockItems |
| `Alignment` | `size_t` | Alignment |

### `xptrfixedstack`

固定指针栈只保存指针值，不拥有指针指向的对象。

```c
typedef xfixedstack xptrfixedstack;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xstack`

动态栈复用连续数组存储，结构性修改可能改变元素地址。

```c
typedef xarray xstack;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xptrstack`

指针栈只保存指针值，不拥有指针指向的对象。

```c
typedef xstack xptrstack;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XRT_BLOCK_STACK_ITEMS_MAX` | `256u` | ITEMS上限 |
| `XRT_BLOCK_STACK_BYTES_DEFAULT` | `16384u` | BYTES默认值 |

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

### `xrtFixedStackInit`

在不与栈结构重叠的调用方缓冲上初始化固定容量栈。

```c
bool xrtFixedStackInit(
	xfixedstack* pStack,
	ptr pMemory,
	size_t iMemorySize,
	size_t iItemSize
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输出 | 非空 | 接收栈 |
| `pMemory` | 输入 | 非空、不与栈重叠 | 元素缓冲 |
| `iMemorySize` | 输入 | 足够容纳容量 | 缓冲字节数 |
| `iItemSize` | 输入 | > 0 | 元素字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 布局参数非法 | `XERR_ARGUMENT` / `XERR_RANGE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 缓冲尺寸与元素大小不匹配

#### 范例

[fixed](../../examples/containers/fixed_stack/main.c) · 外部缓冲初始化

```c
	if ( !xrtFixedStackInit(&tFrames, pStorage, sizeof(pStorage), sizeof(exampleframe)) ) {
```

### `xrtFixedStackCreate`

创建拥有固定容量缓冲的栈。

```c
xfixedstack* xrtFixedStackCreate(size_t iCapacity, size_t iItemSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iCapacity` | 输入 | > 0 | 元素容量 |
| `iItemSize` | 输入 | > 0 | 元素字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 栈（含缓冲） | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 堆创建

```c
		xfixedstack* pFixed = xrtFixedStackCreate(4u, sizeof(int));
```

### `xrtFixedStackUnit`

释放创建时取得的固定缓冲，但不释放栈结构。

```c
void xrtFixedStackUnit(xfixedstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 缓冲已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[fixed](../../examples/containers/fixed_stack/main.c) · 释放缓冲

```c
	xrtFixedStackUnit(&tFrames);
```

### `xrtFixedStackDestroy`

释放固定缓冲和创建的栈结构。

```c
void xrtFixedStackDestroy(xfixedstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 栈已销毁 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 销毁

```c
		xrtFixedStackDestroy(pFixed);
```

### `xrtFixedStackClear`

清空栈内容并保留固定容量。

```c
void xrtFixedStackClear(xfixedstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已清空 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法

#### 范例

[tour](../../examples/stack/tour/main.c) · 清空

```c
		xrtFixedStackClear(pFixed);
```

### `xrtFixedStackSpace`

返回剩余可压入元素数量。

```c
size_t xrtFixedStackSpace(const xfixedstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 剩余容量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法

#### 范例

[tour](../../examples/stack/tour/main.c) · 剩余容量

```c
		iSpace = xrtFixedStackSpace(pFixed);
```

### `xrtFixedStackGet`

返回指定 0 基位置的可写元素借用地址。

```c
ptr xrtFixedStackGet(xfixedstack* pStack, size_t iIndex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |
| `iIndex` | 输入 | < 深度 | 0 基位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 元素借用地址 | — |
| `NULL` | 越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 索引或深度越界

#### 范例

[tour](../../examples/stack/tour/main.c) · 按位取写

```c
		(void)xrtFixedStackGet(pFixed, 0u);
```

### `xrtFixedStackConstGet`

返回指定 0 基位置的只读元素借用地址。

```c
const void* xrtFixedStackConstGet(const xfixedstack* pStack, size_t iIndex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |
| `iIndex` | 输入 | < 深度 | 0 基位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 只读元素借用地址 | — |
| `NULL` | 越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 索引或深度越界

#### 范例

[tour](../../examples/stack/tour/main.c) · 按位取读

```c
		(void)xrtFixedStackConstGet(pFixed, 0u);
```

### `xrtFixedStackAdd`

取得一个未初始化栈顶槽，栈满时失败。

```c
ptr xrtFixedStackAdd(xfixedstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 未初始化栈顶槽 | — |
| `NULL` | 栈满或参数非法 | `XERR_AGAIN` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_AGAIN` — 固定容量已满

#### 范例

[tour](../../examples/stack/tour/main.c) · 取得栈顶槽

```c
		pSlot = (int*)xrtFixedStackAdd(pFixed);
```

### `xrtFixedStackPush`

复制一个元素压入固定栈。

```c
bool xrtFixedStackPush(xfixedstack* pStack, const void* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |
| `pItem` | 输入 | 非空 | 要压入的元素 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已压入 | — |
| `false` | 栈满或参数非法 | `XERR_AGAIN` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_AGAIN` — 固定容量已满

#### 范例

[fixed](../../examples/containers/fixed_stack/main.c) · 复制压入

```c
		if ( !xrtFixedStackPush(&tFrames, &pInput[i]) ) {
```

### `xrtFixedStackPop`

弹出栈顶元素，并可把内容复制到外部输出缓冲。

```c
bool xrtFixedStackPop(xfixedstack* pStack, ptr pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |
| `pItem` | 输出 | 允许空；不与栈重叠 | 接收弹出的元素 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已弹出 | — |
| `false` | 栈空或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 栈为空

#### 范例

[fixed](../../examples/containers/fixed_stack/main.c) · 弹出

```c
	while ( xrtFixedStackPop(&tFrames, &tFrame) ) {
```

### `xrtFixedStackPeek`

返回距栈顶指定深度的可写元素，深度 0 表示栈顶。

```c
ptr xrtFixedStackPeek(xfixedstack* pStack, size_t iDepth)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |
| `iDepth` | 输入 | < 深度 | 距栈顶深度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 元素借用地址 | — |
| `NULL` | 越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 索引或深度越界

#### 范例

[tour](../../examples/stack/tour/main.c) · 深度取写

```c
		(void)xrtFixedStackPeek(pFixed, 0u);
```

### `xrtFixedStackConstPeek`

返回距栈顶指定深度的只读元素，深度 0 表示栈顶。

```c
const void* xrtFixedStackConstPeek(const xfixedstack* pStack, size_t iDepth)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |
| `iDepth` | 输入 | < 深度 | 距栈顶深度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 只读元素借用地址 | — |
| `NULL` | 越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 索引或深度越界

#### 范例

[tour](../../examples/stack/tour/main.c) · 深度取读

```c
			const int* pPeek = (const int*)xrtFixedStackConstPeek(pFixed, 1u);
```

### `xrtFixedStackTop`

返回可写栈顶元素借用地址。

```c
ptr xrtFixedStackTop(xfixedstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 元素借用地址 | — |
| `NULL` | 越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 栈为空

#### 范例

[tour](../../examples/stack/tour/main.c) · 栈顶写

```c
			int* pTop = (int*)xrtFixedStackTop(pFixed);
```

### `xrtFixedStackConstTop`

返回只读栈顶元素借用地址。

```c
const void* xrtFixedStackConstTop(const xfixedstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 只读元素借用地址 | — |
| `NULL` | 越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 栈为空

#### 范例

[tour](../../examples/stack/tour/main.c) · 栈顶读

```c
		(void)xrtFixedStackConstTop(pFixed);
```

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

### `xrtPtrFixedStackInit`

在不与栈结构重叠的调用方指针数组上初始化固定容量指针栈。

```c
bool xrtPtrFixedStackInit(
	xptrfixedstack* pStack,
	ptr* pMemory,
	size_t iCapacity
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输出 | 非空 | 接收栈 |
| `pMemory` | 输入 | 非空 | 指针槽数组 |
| `iCapacity` | 输入 | > 0 | 槽数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法或溢出 | `XERR_ARGUMENT` / `XERR_OVERFLOW` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_OVERFLOW` — 容量尺寸溢出

#### 范例

[pfixed](../../examples/containers/ptr_fixed_stack/main.c) · 外部数组初始化

```c
	if ( !xrtPtrFixedStackInit(&tCleanup, pStorage, 4) ) {
```

### `xrtPtrFixedStackCreate`

创建拥有指定固定容量的指针栈。

```c
xptrfixedstack* xrtPtrFixedStackCreate(size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iCapacity` | 输入 | > 0 | 槽数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 指针栈 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 堆创建

```c
		xptrfixedstack* pPtr = xrtPtrFixedStackCreate(4u);
```

### `xrtPtrFixedStackUnit`

释放拥有的指针存储区，但不释放任何指针目标或栈结构。

```c
void xrtPtrFixedStackUnit(xptrfixedstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 存储已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[pfixed](../../examples/containers/ptr_fixed_stack/main.c) · 释放存储

```c
			xrtPtrFixedStackUnit(&tCleanup);
```

### `xrtPtrFixedStackDestroy`

释放创建的指针栈结构，但不释放任何指针目标。

```c
void xrtPtrFixedStackDestroy(xptrfixedstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 栈已销毁 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 销毁

```c
		xrtPtrFixedStackDestroy(pPtr);
```

### `xrtPtrFixedStackClear`

清空固定指针栈，但不释放任何指针目标。

```c
void xrtPtrFixedStackClear(xptrfixedstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已清空 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法

#### 范例

[tour](../../examples/stack/tour/main.c) · 清空

```c
		xrtPtrFixedStackClear(pPtr);
```

### `xrtPtrFixedStackSpace`

返回固定指针栈剩余容量。

```c
size_t xrtPtrFixedStackSpace(const xptrfixedstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 剩余容量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法

#### 范例

[tour](../../examples/stack/tour/main.c) · 剩余容量

```c
		printf("ptrfixed: space=%zu", xrtPtrFixedStackSpace(pPtr));
```

### `xrtPtrFixedStackGet`

返回指定 0 基位置的指针值。

```c
ptr xrtPtrFixedStackGet(const xptrfixedstack* pStack, size_t iIndex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |
| `iIndex` | 输入 | < 深度 | 0 基位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 指针值 | 存储的值（可为空值） | — |
| `NULL` | 越界 | `XERR_RANGE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 索引或深度越界

#### 范例

[tour](../../examples/stack/tour/main.c) · 按位取值

```c
		(void)xrtPtrFixedStackGet(pPtr, 0u);
```

### `xrtPtrFixedStackPush`

压入一个可为空的指针值。

```c
bool xrtPtrFixedStackPush(xptrfixedstack* pStack, ptr pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |
| `pValue` | 输入 | 任意值 | 要压入的指针 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已压入 | — |
| `false` | 栈满或参数非法 | `XERR_AGAIN` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_AGAIN` — 固定容量已满

#### 范例

[pfixed](../../examples/containers/ptr_fixed_stack/main.c) · 压入

```c
		if ( !xrtPtrFixedStackPush(&tCleanup, &pResources[i]) ) {
```

### `xrtPtrFixedStackPop`

弹出指针值；输出为空表示只删除栈顶。

```c
bool xrtPtrFixedStackPop(xptrfixedstack* pStack, ptr* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |
| `pValue` | 输出 | 允许空 | 接收弹出的值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已弹出 | — |
| `false` | 栈空或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 栈为空

#### 范例

[pfixed](../../examples/containers/ptr_fixed_stack/main.c) · 弹出

```c
	while ( xrtPtrFixedStackPop(&tCleanup, &pResource) ) {
```

### `xrtPtrFixedStackPeek`

返回距栈顶指定深度的指针值，深度 0 表示栈顶。

```c
ptr xrtPtrFixedStackPeek(const xptrfixedstack* pStack, size_t iDepth)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |
| `iDepth` | 输入 | < 深度 | 距栈顶深度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 指针值 | 存储的值（可为空值） | — |
| `NULL` | 越界 | `XERR_RANGE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 索引或深度越界

#### 范例

[tour](../../examples/stack/tour/main.c) · 深度取值

```c
		(void)xrtPtrFixedStackPeek(pPtr, 0u);
```

### `xrtPtrFixedStackTop`

返回栈顶指针值；合法空值与错误通过错误状态区分。

```c
ptr xrtPtrFixedStackTop(const xptrfixedstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 指针值 | 栈顶值（可为合法空值） | — |
| `NULL` | 栈空 | `XERR_RANGE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 栈为空

#### 范例

[tour](../../examples/stack/tour/main.c) · 栈顶值

```c
		(void)xrtPtrFixedStackTop(pPtr);
```

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

### `xrtStackInit`

初始化使用默认对齐的空动态栈。

```c
bool xrtStackInit(xstack* pStack, size_t iItemSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输出 | 非空 | 接收栈 |
| `iItemSize` | 输入 | > 0 | 元素字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 布局参数非法 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_OVERFLOW` — 容量或尺寸计算溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 缓冲分配失败

#### 范例

[cstack](../../examples/containers/stack/main.c) · 默认初始化

```c
	if ( !xrtStackInit(&tValues, sizeof(int)) ) {
```

### `xrtStackInitAligned`

初始化显式过对齐动态栈。

```c
bool xrtStackInitAligned(xstack* pStack, size_t iItemSize, size_t iAlignment)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输出 | 非空 | 接收栈 |
| `iItemSize` | 输入 | > 0 | 元素字节数 |
| `iAlignment` | 输入 | 二次幂 | 元素对齐 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 布局参数非法 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_OVERFLOW` — 容量或尺寸计算溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 缓冲分配失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 对齐初始化

```c
		if ( !xrtStackInitAligned(&Aligned, sizeof(int), 4u) ) {
```

### `xrtStackCreate`

创建使用默认对齐的空动态栈。

```c
xstack* xrtStackCreate(size_t iItemSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iItemSize` | 输入 | > 0 | 元素字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 栈 | — |
| `NULL` | 创建失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_OVERFLOW` — 容量或尺寸计算溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 缓冲分配失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 堆创建

```c
		xstack* pStack = xrtStackCreate(sizeof(int));
```

### `xrtStackCreateAligned`

创建显式过对齐动态栈。

```c
xstack* xrtStackCreateAligned(size_t iItemSize, size_t iAlignment)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iItemSize` | 输入 | > 0 | 元素字节数 |
| `iAlignment` | 输入 | 二次幂 | 元素对齐 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 栈 | — |
| `NULL` | 创建失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_OVERFLOW` — 容量或尺寸计算溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 缓冲分配失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 堆创建（对齐）

```c
		xrtStackDestroy(xrtStackCreateAligned(sizeof(int), 4u));
```

### `xrtStackUnit`

释放动态栈元素内存，但不释放栈结构。

```c
void xrtStackUnit(xstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 元素内存已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[cstack](../../examples/containers/stack/main.c) · 释放元素内存

```c
			xrtStackUnit(&tValues);
```

### `xrtStackDestroy`

释放动态栈全部资源和栈结构。

```c
void xrtStackDestroy(xstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 栈已销毁 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 销毁

```c
		xrtStackDestroy(pStack);
```

### `xrtStackClear`

清空动态栈并保留容量。

```c
void xrtStackClear(xstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已清空 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法

#### 范例

[tour](../../examples/stack/tour/main.c) · 清空

```c
		(void)xrtStackClear(pStack);
```

### `xrtStackReserve`

保证动态栈至少具有指定元素容量。

```c
bool xrtStackReserve(xstack* pStack, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |
| `iCapacity` | 输入 | — | 期望容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 容量已保证 | — |
| `false` | 扩容失败 | `XERR_OVERFLOW` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_OVERFLOW` — 容量溢出
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 预留容量

```c
		(void)xrtStackReserve(pStack, 32u);
```

### `xrtStackTrim`

将动态栈容量裁剪到当前深度。

```c
bool xrtStackTrim(xstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已裁剪（或无需裁剪） | — |
| `false` | 裁剪失败 | `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 裁剪容量

```c
		(void)xrtStackTrim(pStack);
```

### `xrtStackGet`

返回指定 0 基位置的可写元素借用地址。

```c
ptr xrtStackGet(xstack* pStack, size_t iIndex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |
| `iIndex` | 输入 | < 深度 | 0 基位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 元素借用地址 | — |
| `NULL` | 越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 索引或深度越界

#### 范例

[tour](../../examples/stack/tour/main.c) · 按位取写

```c
		(void)xrtStackGet(pStack, 0u);
```

### `xrtStackConstGet`

返回指定 0 基位置的只读元素借用地址。

```c
const void* xrtStackConstGet(const xstack* pStack, size_t iIndex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |
| `iIndex` | 输入 | < 深度 | 0 基位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 只读元素借用地址 | — |
| `NULL` | 越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 索引或深度越界

#### 范例

[tour](../../examples/stack/tour/main.c) · 按位取读

```c
			const int* pGet = (const int*)xrtStackConstGet(pStack, 0u);
```

### `xrtStackAdd`

取得一个未初始化栈顶槽；容量不足时自动扩容。

```c
ptr xrtStackAdd(xstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 未初始化栈顶槽 | — |
| `NULL` | 扩容失败 | `XERR_OVERFLOW` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_OVERFLOW` — 容量溢出
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 取得栈顶槽

```c
			int* pSlot = (int*)xrtStackAdd(pStack);
```

### `xrtStackPush`

复制一个元素压入动态栈；容量不足时自动扩容。

```c
bool xrtStackPush(xstack* pStack, const void* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |
| `pItem` | 输入 | 非空 | 要压入的元素 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已压入 | — |
| `false` | 扩容失败 | `XERR_OVERFLOW` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_OVERFLOW` — 容量溢出
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 复制压入

```c
			if ( !xrtStackPush(pStack, &iValue) ) {
```

### `xrtStackPop`

弹出栈顶元素，并可把内容复制到外部输出缓冲。

```c
bool xrtStackPop(xstack* pStack, ptr pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |
| `pItem` | 输出 | 允许空；不与栈重叠 | 接收弹出的元素 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已弹出 | — |
| `false` | 栈空或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 栈为空

#### 范例

[cstack](../../examples/containers/stack/main.c) · 弹出

```c
	while ( xrtStackPop(&tValues, &iValue) ) {
```

### `xrtStackPeek`

返回距栈顶指定深度的可写元素，深度 0 表示栈顶。

```c
ptr xrtStackPeek(xstack* pStack, size_t iDepth)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |
| `iDepth` | 输入 | < 深度 | 距栈顶深度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 元素借用地址 | — |
| `NULL` | 越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 索引或深度越界

#### 范例

[tour](../../examples/stack/tour/main.c) · 深度取写

```c
		(void)xrtStackPeek(pStack, 0u);
```

### `xrtStackConstPeek`

返回距栈顶指定深度的只读元素，深度 0 表示栈顶。

```c
const void* xrtStackConstPeek(const xstack* pStack, size_t iDepth)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |
| `iDepth` | 输入 | < 深度 | 距栈顶深度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 只读元素借用地址 | — |
| `NULL` | 越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 索引或深度越界

#### 范例

[tour](../../examples/stack/tour/main.c) · 深度取读

```c
			const int* pPeek = (const int*)xrtStackConstPeek(pStack, 0u);
```

### `xrtStackTop`

返回可写栈顶元素借用地址。

```c
ptr xrtStackTop(xstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 元素借用地址 | — |
| `NULL` | 越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 栈为空

#### 范例

[tour](../../examples/stack/tour/main.c) · 栈顶写

```c
			int* pTop = (int*)xrtStackTop(pStack);
```

### `xrtStackConstTop`

返回只读栈顶元素借用地址。

```c
const void* xrtStackConstTop(const xstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 只读元素借用地址 | — |
| `NULL` | 越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 栈为空

#### 范例

[tour](../../examples/stack/tour/main.c) · 栈顶读

```c
		(void)xrtStackConstTop(pStack);
```

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

### `xrtBlockStackInit`

使用自动块尺寸初始化默认对齐分块栈。

```c
bool xrtBlockStackInit(xblockstack* pStack, size_t iItemSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输出 | 非空 | 接收栈 |
| `iItemSize` | 输入 | > 0 | 元素字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 布局参数非法 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_OVERFLOW` — 容量或尺寸计算溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 缓冲分配失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 默认初始化

```c
			(void)xrtBlockStackInit(&tInit, sizeof(int));
```

### `xrtBlockStackInitLayout`

使用指定元素对齐和每块元素数初始化分块栈。

```c
bool xrtBlockStackInitLayout(
	xblockstack* pStack,
	size_t iItemSize,
	size_t iAlignment,
	size_t iBlockItems
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输出 | 非空 | 接收栈 |
| `iItemSize` | 输入 | > 0 | 元素字节数 |
| `iAlignment` | 输入 | 二次幂 | 元素对齐 |
| `iBlockItems` | 输入 | > 0 | 每块元素数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 布局参数非法 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_OVERFLOW` — 容量或尺寸计算溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 缓冲分配失败

#### 范例

[block](../../examples/containers/block_stack/main.c) · 完整布局初始化

```c
	if ( !xrtBlockStackInitLayout(&tFrames, sizeof(int), sizeof(int), 4) ) {
```

### `xrtBlockStackCreate`

创建使用自动块尺寸的默认对齐分块栈。

```c
xblockstack* xrtBlockStackCreate(size_t iItemSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iItemSize` | 输入 | > 0 | 元素字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 分块栈 | — |
| `NULL` | 创建失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_OVERFLOW` — 容量或尺寸计算溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 缓冲分配失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 堆创建

```c
		xblockstack* pBlock = xrtBlockStackCreate(sizeof(int));
```

### `xrtBlockStackCreateLayout`

创建使用指定元素对齐和每块元素数的分块栈。

```c
xblockstack* xrtBlockStackCreateLayout(
	size_t iItemSize,
	size_t iAlignment,
	size_t iBlockItems
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iItemSize` | 输入 | > 0 | 元素字节数 |
| `iAlignment` | 输入 | 二次幂 | 元素对齐 |
| `iBlockItems` | 输入 | > 0 | 每块元素数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 分块栈 | — |
| `NULL` | 创建失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_OVERFLOW` — 容量或尺寸计算溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 缓冲分配失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 堆创建（完整布局）

```c
		xrtBlockStackDestroy(xrtBlockStackCreateLayout(sizeof(int), 4u, 8u));
```

### `xrtBlockStackUnit`

释放全部数据块和块索引，但不释放栈结构。

```c
void xrtBlockStackUnit(xblockstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 数据块已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[block](../../examples/containers/block_stack/main.c) · 释放数据块

```c
		xrtBlockStackUnit(&tFrames);
```

### `xrtBlockStackDestroy`

释放分块栈全部资源和创建的栈结构。

```c
void xrtBlockStackDestroy(xblockstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 栈已销毁 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 销毁

```c
		xrtBlockStackDestroy(xrtBlockStackCreateLayout(sizeof(int), 4u, 8u));
```

### `xrtBlockStackClear`

清空分块栈并保留已经分配的数据块。

```c
void xrtBlockStackClear(xblockstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已清空 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法

#### 范例

[tour](../../examples/stack/tour/main.c) · 清空

```c
		xrtBlockStackClear(pBlock);
```

### `xrtBlockStackReserve`

保证分块栈至少具有指定元素容量，失败时保持原状态。

```c
bool xrtBlockStackReserve(xblockstack* pStack, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |
| `iCapacity` | 输入 | — | 期望容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 容量已保证 | — |
| `false` | 扩容失败，原状态保持 | `XERR_OVERFLOW` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_OVERFLOW` — 容量溢出
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 预留容量

```c
		(void)xrtBlockStackReserve(pBlock, 16u);
```

### `xrtBlockStackTrim`

释放当前深度不再需要的数据块，保留轻量块索引缓存。

```c
bool xrtBlockStackTrim(xblockstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已裁剪 | — |
| `false` | 裁剪失败 | `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 裁剪

```c
		(void)xrtBlockStackTrim(pBlock);
```

### `xrtBlockStackGet`

返回指定 0 基位置的可写元素借用地址。

```c
ptr xrtBlockStackGet(xblockstack* pStack, size_t iIndex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |
| `iIndex` | 输入 | < 深度 | 0 基位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 元素借用地址 | — |
| `NULL` | 越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 索引或深度越界

#### 范例

[block](../../examples/containers/block_stack/main.c) · 按位取写

```c
		xrtBlockStackGet(&tFrames, 0) == pRoot ? "yes" : "no"
```

### `xrtBlockStackConstGet`

返回指定 0 基位置的只读元素借用地址。

```c
const void* xrtBlockStackConstGet(const xblockstack* pStack, size_t iIndex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |
| `iIndex` | 输入 | < 深度 | 0 基位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 只读元素借用地址 | — |
| `NULL` | 越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 索引或深度越界

#### 范例

[tour](../../examples/stack/tour/main.c) · 按位取读

```c
		(void)xrtBlockStackConstGet(pBlock, 0u);
```

### `xrtBlockStackAdd`

取得一个未初始化栈顶槽，既有活动元素地址保持稳定。

```c
ptr xrtBlockStackAdd(xblockstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 未初始化栈顶槽 | — |
| `NULL` | 新块分配失败 | `XERR_OVERFLOW` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_OVERFLOW` — 块布局尺寸溢出
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[block](../../examples/containers/block_stack/main.c) · 取得栈顶槽

```c
	pRoot = (int*)xrtBlockStackAdd(&tFrames);
```

### `xrtBlockStackPush`

浅复制一个完整可读元素压入分块栈，来源可以是任一活动元素。

```c
bool xrtBlockStackPush(xblockstack* pStack, const void* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |
| `pItem` | 输入 | 非空 | 要压入的元素 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已压入 | — |
| `false` | 分配失败 | `XERR_OVERFLOW` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_OVERFLOW` — 块布局尺寸溢出
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[block](../../examples/containers/block_stack/main.c) · 浅复制压入

```c
		if ( !xrtBlockStackPush(&tFrames, &i) ) {
```

### `xrtBlockStackPop`

弹出栈顶元素；调用方必须保证可选输出不与该栈的任何数据块重叠。

```c
bool xrtBlockStackPop(xblockstack* pStack, ptr pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |
| `pItem` | 输出 | 允许空；不与数据块重叠 | 接收弹出的元素 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已弹出 | — |
| `false` | 栈空或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 栈为空

#### 范例

[tour](../../examples/stack/tour/main.c) · 弹出

```c
			(void)xrtBlockStackPop(pBlock, &iPop);
```

### `xrtBlockStackPeek`

返回距栈顶指定深度的可写元素，深度 0 表示栈顶。

```c
ptr xrtBlockStackPeek(xblockstack* pStack, size_t iDepth)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |
| `iDepth` | 输入 | < 深度 | 距栈顶深度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 元素借用地址 | — |
| `NULL` | 越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 索引或深度越界

#### 范例

[tour](../../examples/stack/tour/main.c) · 深度取写

```c
		(void)xrtBlockStackPeek(pBlock, 0u);
```

### `xrtBlockStackConstPeek`

返回距栈顶指定深度的只读元素，深度 0 表示栈顶。

```c
const void* xrtBlockStackConstPeek(const xblockstack* pStack, size_t iDepth)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |
| `iDepth` | 输入 | < 深度 | 距栈顶深度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 只读元素借用地址 | — |
| `NULL` | 越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 索引或深度越界

#### 范例

[tour](../../examples/stack/tour/main.c) · 深度取读

```c
		(void)xrtBlockStackConstPeek(pBlock, 0u);
```

### `xrtBlockStackTop`

返回可写栈顶元素借用地址。

```c
ptr xrtBlockStackTop(xblockstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 元素借用地址 | — |
| `NULL` | 越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 栈为空

#### 范例

[tour](../../examples/stack/tour/main.c) · 栈顶写

```c
		(void)xrtBlockStackTop(pBlock);
```

### `xrtBlockStackConstTop`

返回只读栈顶元素借用地址。

```c
const void* xrtBlockStackConstTop(const xblockstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 只读元素借用地址 | — |
| `NULL` | 越界或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 栈为空

#### 范例

[tour](../../examples/stack/tour/main.c) · 栈顶读

```c
			const int* pTop = (const int*)xrtBlockStackConstTop(pBlock);
```

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

### `xrtPtrStackInit`

初始化空指针栈。

```c
bool xrtPtrStackInit(xptrstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输出 | 非空 | 接收栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法

#### 范例

[pstack](../../examples/containers/ptr_stack/main.c) · 初始化

```c
	if ( !xrtPtrStackInit(&tResources) ) {
```

### `xrtPtrStackCreate`

创建空指针栈。

```c
xptrstack* xrtPtrStackCreate(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 指针栈 | — |
| `NULL` | 分配失败 | `XERR_MEMORY` |

#### 错误

- `XERR_MEMORY` — 结构分配失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 堆创建

```c
		xptrstack* pPtr = xrtPtrStackCreate();
```

### `xrtPtrStackUnit`

释放指针存储区但不释放指针目标。

```c
void xrtPtrStackUnit(xptrstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 存储已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[pstack](../../examples/containers/ptr_stack/main.c) · 释放存储

```c
			xrtPtrStackUnit(&tResources);
```

### `xrtPtrStackDestroy`

释放指针栈全部资源和栈结构，但不释放指针目标。

```c
void xrtPtrStackDestroy(xptrstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 栈已销毁 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 销毁

```c
		xrtPtrStackDestroy(pPtr);
```

### `xrtPtrStackClear`

清空指针栈并保留容量，但不释放指针目标。

```c
void xrtPtrStackClear(xptrstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已清空 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法

#### 范例

[tour](../../examples/stack/tour/main.c) · 清空

```c
		xrtPtrStackClear(pPtr);
```

### `xrtPtrStackReserve`

保证指针栈至少具有指定容量。

```c
bool xrtPtrStackReserve(xptrstack* pStack, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |
| `iCapacity` | 输入 | — | 期望容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 容量已保证 | — |
| `false` | 扩容失败 | `XERR_OVERFLOW` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_OVERFLOW` — 容量溢出
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 预留容量

```c
		(void)xrtPtrStackReserve(pPtr, 16u);
```

### `xrtPtrStackTrim`

将指针栈容量裁剪到当前深度。

```c
bool xrtPtrStackTrim(xptrstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已裁剪 | — |
| `false` | 裁剪失败 | `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[tour](../../examples/stack/tour/main.c) · 裁剪容量

```c
		(void)xrtPtrStackTrim(pPtr);
```

### `xrtPtrStackGet`

返回指定 0 基位置的指针值。

```c
ptr xrtPtrStackGet(const xptrstack* pStack, size_t iIndex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |
| `iIndex` | 输入 | < 深度 | 0 基位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 指针值 | 存储的值（可为空值） | — |
| `NULL` | 越界 | `XERR_RANGE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 索引或深度越界

#### 范例

[tour](../../examples/stack/tour/main.c) · 按位取值

```c
		(void)xrtPtrStackGet(pPtr, 0u);
```

### `xrtPtrStackPush`

压入一个可为空的指针值；容量不足时自动扩容。

```c
bool xrtPtrStackPush(xptrstack* pStack, ptr pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |
| `pValue` | 输入 | 任意值 | 要压入的指针 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已压入 | — |
| `false` | 扩容失败 | `XERR_OVERFLOW` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_OVERFLOW` — 容量溢出
- `XERR_MEMORY` — 扩容分配失败

#### 范例

[pstack](../../examples/containers/ptr_stack/main.c) · 压入

```c
		if ( !xrtPtrStackPush(&tResources, &pValues[i]) ) {
```

### `xrtPtrStackPop`

弹出指针值；输出为空表示只删除栈顶。

```c
bool xrtPtrStackPop(xptrstack* pStack, ptr* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入/输出 | 非空 | 目标栈 |
| `pValue` | 输出 | 允许空 | 接收弹出的值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已弹出 | — |
| `false` | 栈空或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 栈为空

#### 范例

[pstack](../../examples/containers/ptr_stack/main.c) · 弹出

```c
	while ( xrtPtrStackPop(&tResources, &pValue) ) {
```

### `xrtPtrStackPeek`

返回距栈顶指定深度的指针值，深度 0 表示栈顶。

```c
ptr xrtPtrStackPeek(const xptrstack* pStack, size_t iDepth)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |
| `iDepth` | 输入 | < 深度 | 距栈顶深度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 指针值 | 存储的值（可为空值） | — |
| `NULL` | 越界 | `XERR_RANGE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 索引或深度越界

#### 范例

[tour](../../examples/stack/tour/main.c) · 深度取值

```c
		(void)xrtPtrStackPeek(pPtr, 0u);
```

### `xrtPtrStackTop`

返回栈顶指针值；合法空值与错误通过错误状态区分。

```c
ptr xrtPtrStackTop(const xptrstack* pStack)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStack` | 输入 | 非空 | 目标栈 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 指针值 | 栈顶值（可为合法空值） | — |
| `NULL` | 栈空 | `XERR_RANGE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空、元素大小为零或参数非法
- `XERR_RANGE` — 栈为空

#### 范例

[tour](../../examples/stack/tour/main.c) · 栈顶值

```c
			ptr pTop = xrtPtrStackTop(pPtr);
```

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
