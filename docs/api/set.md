# Set

`set.h` 提供固定大小元素的通用哈希集合。空集合不分配内存，每个元素使用一个紧凑独立条目，平均查找复杂度为 `O(1)`，并保持元素地址和首次插入顺序稳定。

## 类型与常量

### `xset`

集合使用独立哈希条目保存固定大小元素，并保持元素地址和插入顺序稳定。

```c
typedef struct xset {
	xsetentry** Buckets;
	xsetentry* First;
	xsetentry* Last;
	size_t ItemSize;
	size_t ItemOffset;
	size_t Alignment;
	size_t Count;
	size_t BucketCount;
	size_t Threshold;
	uint64 Version;
	xsethash Hash;
	xsetequal Equal;
	xsetcopy Copy;
	xsetdrop Drop;
	ptr KeyUserData;
	ptr LifecycleUserData;
	uint32 Flags;
} xset;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Buckets` | `xsetentry**` | 桶数组 |
| `First` | `xsetentry*` | 首元素 |
| `Last` | `xsetentry*` | 末元素 |
| `ItemSize` | `size_t` | 单元素字节数 |
| `ItemOffset` | `size_t` | 元素偏移 |
| `Alignment` | `size_t` | 对齐（二次幂） |
| `Count` | `size_t` | 数量 |
| `BucketCount` | `size_t` | 桶数量 |
| `Threshold` | `size_t` | 阈值 |
| `Version` | `uint64` | 结构版本 |
| `Hash` | `xsethash` | Hash |
| `Equal` | `xsetequal` | Equal |
| `Copy` | `xsetcopy` | Copy |
| `Drop` | `xsetdrop` | Drop |
| `KeyUserData` | `ptr` | KeyUserData |
| `LifecycleUserData` | `ptr` | LifecycleUserData |
| `Flags` | `uint32` | 标志位 |

### `xsetiter`

外置迭代器允许同一集合存在多个独立遍历状态。

```c
typedef struct xsetiter {
	xset* Set;
	xsetentry* Next;
	uint64 Version;
	int Direction;
} xsetiter;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Set` | `xset*` | Set |
| `Next` | `xsetentry*` | 后继 |
| `Version` | `uint64` | 结构版本 |
| `Direction` | `int` | Direction |

### `xsetentry`

Set 内部条目结构（不透明，仅实现内部使用）。


```c
typedef struct xsetentry xsetentry;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xsethash`

哈希器必须保证相等元素产生相同哈希值，回调中不得调用同一集合的 API。

```c
typedef uint64 (*xsethash)(const void* pItem, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xsetequal`

相等器只借用元素且不得调用同一集合的 API。

```c
typedef bool (*xsetequal)(
	const void* pLeft,
	const void* pRight,
	ptr pUserData
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xsetcopy`

复制器成功时保持键等价，失败时不得在已清零目标槽遗留资源。

```c
typedef bool (*xsetcopy)(ptr pTarget, const void* pSource, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xsetdrop`

释放器处理元素内部资源且不得调用同一集合的 API，不释放元素槽本身。

```c
typedef void (*xsetdrop)(ptr pItem, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xsetvisitor`

访问器可查询同一集合但不得修改、结束或再次访问，返回 false 时停止。

```c
typedef bool (*xsetvisitor)(const void* pItem, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XRT_SET_ALIGNMENT_DEFAULT` | `16u` | ALIGNMENT默认值 |
| `XRT_SET_BUCKETS_MIN` | `16u` | BUCKETS下限 |

## 启用与依赖

```c
#define XRT_FEATURE_SET
```

依赖关系：

```text
set -> hash64 -> core
```

## 设计契约

- 默认按完整元素字节使用稳定 `xrtHash64` 和精确比较。
- 桶数组负载上限为 75%，扩缩容只重建桶链，不移动元素。
- 删除后重新加入的元素位于插入顺序尾部。
- `GetOrAdd`、`Add` 和单元素扩容在分配失败时不改变集合。
- `Merge` 预先暂存全部缺失元素和新桶，成功后一次提交；失败时目标完全不变。
- `Merge` 不复制或移动目标已有元素，并保持这些元素的地址与相对顺序。
- 集合不内置锁；跨线程并发访问由调用方同步。
- 结构修改使迭代器失效；`Reserve` 和 `Trim` 不使迭代器失效。
- 键策略和生命周期回调不得调用同一集合的任何 API。
- 访问器可查询当前集合，但不得修改、结束或再次访问当前集合。

## 初始化与生命周期

```c
bool xrtSetInit(xset* set, size_t item_size);
bool xrtSetInitAligned(xset* set, size_t item_size, size_t alignment);
xset* xrtSetCreate(size_t item_size);
xset* xrtSetCreateAligned(size_t item_size, size_t alignment);
void xrtSetUnit(xset* set);
void xrtSetDestroy(xset* set);
void xrtSetClear(xset* set);
```

`Init` 用于栈对象和嵌入对象，配对 `Unit`。`Create` 用于堆对象，配对 `Destroy`。`Clear` 释放元素但保留桶数组，`Trim` 可释放空闲桶。

```c
xset tStack;
xset* pHeap;
xset* pAligned;

if ( !xrtSetInitAligned(&tStack, sizeof(uint64), 32) ) {
	return false;
}
pHeap = xrtSetCreate(sizeof(uint64));
pAligned = xrtSetCreateAligned(sizeof(uint64), 32);
if ( (pHeap == NULL) || (pAligned == NULL) ) {
	if ( pAligned != NULL ) {
		xrtSetDestroy(pAligned);
	}
	if ( pHeap != NULL ) {
		xrtSetDestroy(pHeap);
	}
	xrtSetUnit(&tStack);
	return false;
}

xrtSetDestroy(pAligned);
xrtSetDestroy(pHeap);
xrtSetUnit(&tStack);
```

### `xrtSetInit`

使用默认 16 字节对齐初始化空集合。

```c
bool xrtSetInit(xset* pSet, size_t iItemSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输出 | 非空 | 接收集合 |
| `iItemSize` | 输入 | > 0 | 元素字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 布局参数非法 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_RANGE` — 容量或尺寸计算溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 桶数组分配失败

#### 范例

[set](../../examples/containers/set/main.c) · 内嵌初始化

```c
	if ( !xrtSetInit(&tEnabled, sizeof(int)) ) {
```

### `xrtSetInitAligned`

使用显式元素对齐初始化空集合。

```c
bool xrtSetInitAligned(
	xset* pSet,
	size_t iItemSize,
	size_t iAlignment
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输出 | 非空 | 接收集合 |
| `iItemSize` | 输入 | > 0 | 元素字节数 |
| `iAlignment` | 输入 | 二次幂 | 元素对齐 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 布局参数非法 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_RANGE` — 容量或尺寸计算溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 桶数组分配失败

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 对齐初始化

```c
	if ( !xrtSetInitAligned(&tAligned, sizeof(int),
			sizeof(int)) ||
		!xrtSetAdd(&tAligned, &Values[2]) ||
		(xrtSetCount(&tAligned) != 1u) ) {
```

### `xrtSetCreate`

创建使用默认 16 字节对齐的空集合。

```c
xset* xrtSetCreate(size_t iItemSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iItemSize` | 输入 | > 0 | 元素字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 集合 | — |
| `NULL` | 创建失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_RANGE` — 容量或尺寸计算溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 桶数组分配失败

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 堆创建

```c
	pA = xrtSetCreate(sizeof(int));
```

### `xrtSetCreateAligned`

创建使用显式元素对齐的空集合。

```c
xset* xrtSetCreateAligned(size_t iItemSize, size_t iAlignment)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iItemSize` | 输入 | > 0 | 元素字节数 |
| `iAlignment` | 输入 | 二次幂 | 元素对齐 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 集合 | — |
| `NULL` | 创建失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_RANGE` — 容量或尺寸计算溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 桶数组分配失败

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 堆创建（对齐）

```c
		xset* pAligned = xrtSetCreateAligned(sizeof(int),
			sizeof(int));
```

### `xrtSetUnit`

释放全部元素和桶数组，但不释放集合结构。

```c
void xrtSetUnit(xset* pSet)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[set](../../examples/containers/set/main.c) · 释放内部资源

```c
		xrtSetUnit(&tEnabled);
```

### `xrtSetDestroy`

释放全部元素、桶数组和集合结构。

```c
void xrtSetDestroy(xset* pSet)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已销毁 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[set](../../examples/containers/set/main.c) · 销毁集合

```c
		xrtSetDestroy(pAllowed);
```

## 键策略

```c
typedef uint64 (*xsethash)(const void* item, ptr user_data);
typedef bool (*xsetequal)(const void* left, const void* right, ptr user_data);

bool xrtSetSetKeyPolicy(
	xset* set,
	xsethash hash,
	xsetequal equal,
	ptr user_data
);
```

哈希器和相等器必须成对设置，并且只可在集合为空时修改。相等关系必须满足自反、对称和传递，相等元素必须产生相同哈希。自定义策略适合忽略结构填充、按业务键去重、字符串内容比较或抗碰撞哈希。

键策略回调只借用输入元素，不得保留指针，也不得调用同一集合的任何 API。回调期间尝试重入同一集合会得到 `XERR_STATE`。不同集合之间不存在隐式同步；共享回调上下文时仍由调用方保证线程安全。

### `xrtSetSetKeyPolicy`

为仍为空的集合设置成对的自定义哈希器和相等器。

```c
bool xrtSetSetKeyPolicy(
	xset* pSet,
	xsethash pHash,
	xsetequal pEqual,
	ptr pUserData
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 非空且为空 | 目标集合 |
| `pHash` | 输入 | 非空 | 哈希函数 |
| `pEqual` | 输入 | 非空 | 相等函数 |
| `pUserData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已设置 | — |
| `false` | 集合非空或参数非法 | `XERR_STATE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_STATE` — 集合非空、状态非法或两个集合不兼容

#### 范例

[owned](../../examples/containers/set/owned/main.c) · 键策略

```c
		!xrtSetSetKeyPolicy(
			&tTags,
			exampleTagHash,
			exampleTagEqual,
			NULL
		) ||
```

## 资源生命周期

```c
typedef bool (*xsetcopy)(ptr target, const void* source, ptr user_data);
typedef void (*xsetdrop)(ptr item, ptr user_data);

bool xrtSetSetLifecycle(
	xset* set,
	xsetcopy copy,
	xsetdrop drop,
	ptr user_data
);
```

复制器和释放器必须成对设置。目标槽在调用复制器前已经清零；复制器失败时不得留下需要释放的资源，并应设置能够说明失败原因的错误。复制成功后的目标必须与源元素拥有相同哈希且相等，否则集合会调用释放器回滚并报告 `XERR_STATE`。该契约使字符串、引用计数句柄和 `xvalue` 等拥有型元素可以安全参与克隆与集合运算。

没有生命周期回调时，集合按字节复制元素，也不会释放元素内部指针。

复制器和释放器不得调用同一集合的任何 API。释放器只处理元素内部拥有的资源，集合负责释放条目槽。

完整的拥有型结构示例见 `examples/containers/set/owned/main.c`。它按业务编号实现
`xsethash`、`xsetequal`，用 `xsetcopy` 深复制字符串，并用 `xsetdrop` 释放资源。

### `xrtSetSetLifecycle`

为仍为空的集合设置成对的资源复制器和释放器。

```c
bool xrtSetSetLifecycle(
	xset* pSet,
	xsetcopy pCopy,
	xsetdrop pDrop,
	ptr pUserData
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 非空且为空 | 目标集合 |
| `pCopy` | 输入 | 非空 | 复制器 |
| `pDrop` | 输入 | 非空 | 释放器 |
| `pUserData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已设置 | — |
| `false` | 集合非空或参数非法 | `XERR_STATE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_STATE` — 集合非空、状态非法或两个集合不兼容

#### 范例

[owned](../../examples/containers/set/owned/main.c) · 资源生命周期

```c
		!xrtSetSetLifecycle(
			&tTags,
			exampleTagCopy,
			exampleTagDrop,
			NULL
		)
```

## 基础操作

```c
const void* xrtSetGetOrAdd(xset* set, const void* item, bool* is_new);
bool xrtSetAdd(xset* set, const void* item);
const void* xrtSetGet(const xset* set, const void* item);
bool xrtSetHas(const xset* set, const void* item);
bool xrtSetRemove(xset* set, const void* item);
bool xrtSetTake(xset* set, const void* item, ptr output);
```

`GetOrAdd` 和 `Get` 返回集合内部规范元素。加入等价元素不会替换第一次保存的元素。返回地址在该元素删除或集合销毁前保持稳定。

`Remove` 调用释放器。`Take` 把完整元素字节移交给输出后删除，不调用释放器。为避免删除后悬空、覆盖桶或破坏元数据，完整输出区间不得与集合结构、桶数组或任何元素条目重叠；该安全检查需要扫描条目，因此 `Take` 为 `O(n)`，普通查找和删除仍为平均 `O(1)`。

```c
xset tSet;
const int* pStored;
int iValue = 42;
int iTaken;
bool bNew;

if ( !xrtSetInit(&tSet, sizeof(int)) ) {
	return false;
}
if ( !xrtSetReserve(&tSet, 128) ) {
	xrtSetUnit(&tSet);
	return false;
}
pStored = (const int*)xrtSetGetOrAdd(&tSet, &iValue, &bNew);
if ( (pStored == NULL) || !bNew ||
	!xrtSetHas(&tSet, &iValue) ||
	(xrtSetGet(&tSet, &iValue) != pStored) ||
	(xrtSetCount(&tSet) != 1) ||
	(xrtSetCapacity(&tSet) < 128) ) {
	xrtSetUnit(&tSet);
	return false;
}
if ( !xrtSetTake(&tSet, &iValue, &iTaken) || (iTaken != iValue) ) {
	xrtSetUnit(&tSet);
	return false;
}

if ( !xrtSetAdd(&tSet, &iValue) ||
	!xrtSetRemove(&tSet, &iValue) ) {
	xrtSetUnit(&tSet);
	return false;
}
xrtSetClear(&tSet);
if ( !xrtSetTrim(&tSet) ) {
	xrtSetUnit(&tSet);
	return false;
}
xrtSetUnit(&tSet);
```

### `xrtSetGetOrAdd`

返回规范存储元素，缺失时失败原子地复制插入。

```c
const void* xrtSetGetOrAdd(
	xset* pSet,
	const void* pItem,
	bool* pNew
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入/输出 | 非空 | 目标集合 |
| `pItem` | 输入 | 非空 | 查找或插入的元素 |
| `pNew` | 输出 | 允许空 | 接收是否本次新插入 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 规范元素借用（集合存活且未修改期间有效） | — |
| `NULL` | 缺失或失败 | 不设错误 / 见错误 |
（插入失败时见错误）

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_RANGE` — 容量增长溢出
- `XERR_MEMORY` — 扩容或复制分配失败

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 取或加

```c
	pSlot = xrtSetGetOrAdd(pA, &Values[0], &bNew);
```

### `xrtSetAdd`

复制加入元素，已有等价元素时成功且不替换规范元素。

```c
bool xrtSetAdd(xset* pSet, const void* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入/输出 | 非空 | 目标集合 |
| `pItem` | 输入 | 非空 | 要加入的元素 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已加入或已存在 | — |
| `false` | 失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_RANGE` — 容量增长溢出
- `XERR_MEMORY` — 扩容或复制分配失败

#### 范例

[set](../../examples/containers/set/main.c) · 复制加入

```c
		if ( !xrtSetAdd(&tEnabled, &arrEnabled[i]) ||
			!xrtSetAdd(&tRequested, &arrRequested[i]) ) {
```

### `xrtSetGet`

返回集合内部的规范元素，缺失是正常结果。

```c
const void* xrtSetGet(const xset* pSet, const void* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 非空 | 目标集合 |
| `pItem` | 输入 | 非空 | 查找键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 规范元素借用（集合存活且未修改期间有效） | — |
| `NULL` | 缺失或失败 | 不设错误 / 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零

#### 范例

[owned](../../examples/containers/set/owned/main.c) · 规范元素查询

```c
	pStored = (const exampletag*)xrtSetGet(&tTags, &tDuplicate);
```

### `xrtSetHas`

判断等价元素是否存在。

```c
bool xrtSetHas(const xset* pSet, const void* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 非空 | 目标集合 |
| `pItem` | 输入 | 非空 | 查找键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是存在 | — |
| `false` | 不是存在 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 存在判断

```c
		!xrtSetHas(pA, &Values[0]) ) {
```

### `xrtSetRemove`

删除等价元素并调用资源释放器。

```c
bool xrtSetRemove(xset* pSet, const void* pItem)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入/输出 | 非空 | 目标集合 |
| `pItem` | 输入 | 非空 | 查找键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已删除 | — |
| `false` | 不存在 | 不设错误 |

#### 错误

- 元素不存在返回 `false` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 删除

```c
		!xrtSetRemove(pA, &Values[2]) ||
```

### `xrtSetTake`

把规范元素移交后删除；输出区间不得接触集合拥有的任何内存。

```c
bool xrtSetTake(xset* pSet, const void* pItem, ptr pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入/输出 | 非空 | 目标集合 |
| `pItem` | 输入 | 非空 | 查找键 |
| `pValue` | 输出 | 非空、不与集合重叠 | 接收移交的元素 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已移交并删除 | — |
| `false` | 不存在 | 不设错误 |

#### 错误

- 元素不存在返回 `false` 且不设置错误；参数非法 `XERR_ARGUMENT`

#### 范例

[owned](../../examples/containers/set/owned/main.c) · 移交删除

```c
	if ( !xrtSetTake(&tTags, &tPrimary, &tTaken) ) {
```

### `xrtSetVisit`

按插入顺序访问元素，并返回实际访问数量。

```c
size_t xrtSetVisit(xset* pSet, xsetvisitor pVisitor, ptr pUserData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 非空 | 目标集合 |
| `pVisitor` | 输入 | 非空 | 访问回调 |
| `pUserData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 实际访问数量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 访问全部元素

```c
	if ( (xrtSetVisit(pA, exampleVisitCount, &iSeen) != 2u) ||
		(iSeen != 2u) ) {
```

## 容量

```c
bool xrtSetReserve(xset* set, size_t capacity);
bool xrtSetTrim(xset* set);
size_t xrtSetCount(const xset* set);
size_t xrtSetCapacity(const xset* set);
```

`Capacity` 是再次扩容前可容纳的元素数，不是桶数。

`xrtSetCount(&tSet)` 返回当前元素数，`xrtSetCapacity(&tSet)` 返回已预留的元素容量。
空集合调用 `Trim` 会释放桶数组；非空集合只把桶数缩到当前元素数所需的最小值。

### `xrtSetClear`

清空全部元素并保留桶数组供后续复用。

```c
void xrtSetClear(xset* pSet)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入/输出 | 非空 | 目标集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已清空 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 清空

```c
	xrtSetClear(&tAligned);
```

### `xrtSetReserve`

确保集合无需扩容即可容纳指定数量的元素。

```c
bool xrtSetReserve(xset* pSet, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入/输出 | 非空 | 目标集合 |
| `iCapacity` | 输入 | — | 期望容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 容量已保证 | — |
| `false` | 扩容失败 | `XERR_RANGE` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_RANGE` — 容量溢出
- `XERR_MEMORY` — 桶数组分配失败

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 预留容量

```c
		!xrtSetReserve(pA, 16u) ||
```

### `xrtSetTrim`

把桶数组收缩到当前元素数需要的最小容量。

```c
bool xrtSetTrim(xset* pSet)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入/输出 | 非空 | 目标集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已收缩（或无需收缩） | — |
| `false` | 收缩失败 | `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_MEMORY` — 新桶数组分配失败

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 收缩容量

```c
		!xrtSetTrim(pA) ||
```

### `xrtSetCount`

返回当前元素数，非法集合返回零。

```c
size_t xrtSetCount(const xset* pSet)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 非空 | 目标集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 当前元素数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 元素数量

```c
		(xrtSetCount(pA) != 2u) ||
```

### `xrtSetCapacity`

返回再次扩容前可容纳的元素数。

```c
size_t xrtSetCapacity(const xset* pSet)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 非空 | 目标集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 当前容量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 容量查询

```c
		(xrtSetCapacity(pA) < 2u) ||
```

## 遍历

```c
size_t xrtSetVisit(xset* set, xsetvisitor visitor, ptr user_data);
bool xrtSetIterBegin(xset* set, xsetiter* iterator);
bool xrtSetIterRBegin(xset* set, xsetiter* iterator);
const void* xrtSetIterNext(xsetiter* iterator);
void xrtSetIterEnd(xsetiter* iterator);
```

遍历顺序确定为首次插入顺序。迭代器只借用集合，调用方必须在集合有效期内结束使用。

`Visit` 调用访问器期间允许 `Count`、`Capacity`、`Get` 和 `Has` 等查询，但拒绝增删、容量或策略修改、`Unit`、`Destroy` 以及嵌套 `Visit`，并报告 `XERR_STATE`。外置迭代器不会锁定集合；任何结构修改都会让已有迭代器在下一次推进时报告 `XERR_STATE`。

```c
/* 打印一个整数元素，返回 true 继续访问。 */
static bool printItem(const void* pItem, ptr pUserData)
{
	(void)pUserData;
	printf("%d\n", *(const int*)pItem);
	return true;
}



xsetiter tIterator;
const int* pItem;

xrtSetVisit(&tSet, printItem, NULL);
if ( xrtSetIterBegin(&tSet, &tIterator) ) {
	while ( (pItem = (const int*)xrtSetIterNext(&tIterator)) != NULL ) {
		printf("%d\n", *pItem);
	}
	xrtSetIterEnd(&tIterator);
}
if ( xrtSetIterRBegin(&tSet, &tIterator) ) {
	while ( (pItem = (const int*)xrtSetIterNext(&tIterator)) != NULL ) {
		printf("%d\n", *pItem);
	}
	xrtSetIterEnd(&tIterator);
}
```

### `xrtSetIterBegin`

启动按插入顺序的外置迭代器。

```c
bool xrtSetIterBegin(xset* pSet, xsetiter* pIterator)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 非空 | 目标集合 |
| `pIterator` | 输出 | 非空 | 接收迭代器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已启动 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零

#### 范例

[set](../../examples/containers/set/main.c) · 正向迭代

```c
	if ( !xrtSetIterBegin(pAllowed, &tIterator) ) {
```

### `xrtSetIterRBegin`

启动按插入顺序逆序遍历的外置迭代器。

```c
bool xrtSetIterRBegin(xset* pSet, xsetiter* pIterator)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 非空 | 目标集合 |
| `pIterator` | 输出 | 非空 | 接收迭代器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已启动 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 逆向迭代

```c
		if ( !xrtSetIterRBegin(pA, &Iter) ) {
```

### `xrtSetIterNext`

返回下一规范元素，结构修改后报告状态错误。

```c
const void* xrtSetIterNext(xsetiter* pIterator)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIterator` | 输入/输出 | 已启动 | 目标迭代器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 规范元素借用；遍历结束为 `NULL` | — |
| `NULL` | 遍历结束或迭代器失效 | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 集合结构在迭代期间被修改；遍历结束返回 `NULL` 不设错

#### 范例

[set](../../examples/containers/set/main.c) · 下一元素

```c
	while ( (pPort = (const int*)xrtSetIterNext(&tIterator)) != NULL ) {
```

### `xrtSetIterEnd`

提前结束迭代并清除借用状态。

```c
void xrtSetIterEnd(xsetiter* pIterator)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIterator` | 输入/输出 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已结束 | — |

#### 错误

- 无 — 结束不失败

#### 范例

[set](../../examples/containers/set/main.c) · 结束迭代

```c
	xrtSetIterEnd(&tIterator);
```

## 集合运算

```c
xset* xrtSetClone(const xset* set);
bool xrtSetMerge(xset* target, const xset* source);
xset* xrtSetUnion(const xset* left, const xset* right);
xset* xrtSetIntersection(const xset* left, const xset* right);
xset* xrtSetDifference(const xset* left, const xset* right);
xset* xrtSetSymmetricDifference(const xset* left, const xset* right);
bool xrtSetIsSubset(const xset* left, const xset* right, bool proper);
bool xrtSetIsSuperset(const xset* left, const xset* right, bool proper);
bool xrtSetIsDisjoint(const xset* left, const xset* right);
bool xrtSetEqual(const xset* left, const xset* right);
```

参与二元运算的集合必须具有相同元素大小、对齐、键策略和生命周期策略。结果集合沿用左集合策略。所有返回的集合由调用方使用 `xrtSetDestroy` 释放。

`IsDisjoint` 遍历元素较少的一侧，找到首个共同元素即停止；两个空集合互斥，同一个非空集合不互斥。关系判断不分配内存，`false` 可以表示正常的关系不成立；参数或集合状态错误通过线程错误记录区分。

克隆和集合运算是多步只读操作。实现会临时保护输入集合，阻止键策略或生命周期回调修改、结束输入集合；即使函数参数为 `const`，同一集合实例仍不得被其他线程并发访问。`Merge` 只暂存缺失元素，不复制或释放目标已有元素；仅合并重复元素不会改变结构版本，真正加入元素才会使外置迭代器失效。

```c
/* 创建全部集合运算结果，并在任意失败路径释放已经创建的结果。 */
static bool runAlgebra(xset* pLeft, const xset* pRight)
{
	xset* pClone = xrtSetClone(pLeft);
	xset* pUnion = xrtSetUnion(pLeft, pRight);
	xset* pIntersection = xrtSetIntersection(pLeft, pRight);
	xset* pDifference = xrtSetDifference(pLeft, pRight);
	xset* pSymmetric = xrtSetSymmetricDifference(pLeft, pRight);
	bool bResult = false;

	if ( (pClone == NULL) || (pUnion == NULL) ||
		(pIntersection == NULL) || (pDifference == NULL) ||
		(pSymmetric == NULL) ) {
		goto cleanup;
	}
	if ( !xrtSetMerge(pClone, pRight) ) {
		goto cleanup;
	}

	printf("equal: %d\n", xrtSetEqual(pClone, pUnion));
	printf("subset: %d\n", xrtSetIsSubset(pLeft, pUnion, false));
	printf("superset: %d\n", xrtSetIsSuperset(pUnion, pRight, false));
	printf("disjoint: %d\n", xrtSetIsDisjoint(pLeft, pRight));
	bResult = true;

cleanup:
	if ( pSymmetric != NULL ) {
		xrtSetDestroy(pSymmetric);
	}
	if ( pDifference != NULL ) {
		xrtSetDestroy(pDifference);
	}
	if ( pIntersection != NULL ) {
		xrtSetDestroy(pIntersection);
	}
	if ( pUnion != NULL ) {
		xrtSetDestroy(pUnion);
	}
	if ( pClone != NULL ) {
		xrtSetDestroy(pClone);
	}
	return bResult;
}
```

### `xrtSetClone`

深度复制集合结构，并按生命周期复制器复制元素。

```c
xset* xrtSetClone(const xset* pSet)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSet` | 输入 | 非空 | 源集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新集合 | — |
| `NULL` | 复制失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_MEMORY` — 结构或元素复制分配失败
- `XERR_STATE` — 元素复制器失败

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 深度复制

```c
			xset* pClone = xrtSetClone(pA);
```

### `xrtSetMerge`

事务合并缺失元素，失败不变且保留已有元素地址和相对顺序。

```c
bool xrtSetMerge(xset* pTarget, const xset* pSource)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTarget` | 输入/输出 | 非空 | 目标集合 |
| `pSource` | 输入 | 非空、兼容 | 源集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已合并全部缺失元素 | — |
| `false` | 失败，目标保持不变 | `XERR_STATE` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_STATE` — 集合非空、状态非法或两个集合不兼容
- `XERR_MEMORY` — 扩容或复制分配失败

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 事务合并

```c
		!xrtSetMerge(pMerged, pB) ||
```

### `xrtSetUnion`

创建两个兼容集合的并集。

```c
xset* xrtSetUnion(const xset* pLeft, const xset* pRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空、兼容 | 左集合 |
| `pRight` | 输入 | 非空、兼容 | 右集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新集合 | — |
| `NULL` | 创建失败 | `XERR_STATE` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_STATE` — 集合非空、状态非法或两个集合不兼容
- `XERR_MEMORY` — 分配失败

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 并集

```c
	pUnion = xrtSetUnion(pA, pB);
```

### `xrtSetIntersection`

创建两个兼容集合的交集。

```c
xset* xrtSetIntersection(const xset* pLeft, const xset* pRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空、兼容 | 左集合 |
| `pRight` | 输入 | 非空、兼容 | 右集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新集合 | — |
| `NULL` | 创建失败 | `XERR_STATE` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_STATE` — 集合非空、状态非法或两个集合不兼容
- `XERR_MEMORY` — 分配失败

#### 范例

[set](../../examples/containers/set/main.c) · 交集

```c
	pAllowed = xrtSetIntersection(&tRequested, &tEnabled);
```

### `xrtSetDifference`

创建左集合相对右集合的差集。

```c
xset* xrtSetDifference(const xset* pLeft, const xset* pRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空、兼容 | 左集合 |
| `pRight` | 输入 | 非空、兼容 | 右集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新集合 | — |
| `NULL` | 创建失败 | `XERR_STATE` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_STATE` — 集合非空、状态非法或两个集合不兼容
- `XERR_MEMORY` — 分配失败

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 差集

```c
	pDiff = xrtSetDifference(pUnion, pB);
```

### `xrtSetSymmetricDifference`

创建两个兼容集合的对称差集。

```c
xset* xrtSetSymmetricDifference(
	const xset* pLeft,
	const xset* pRight
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空、兼容 | 左集合 |
| `pRight` | 输入 | 非空、兼容 | 右集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新集合 | — |
| `NULL` | 创建失败 | `XERR_STATE` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_STATE` — 集合非空、状态非法或两个集合不兼容
- `XERR_MEMORY` — 分配失败

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 对称差集

```c
		pSym = xrtSetSymmetricDifference(pThree, pB);
```

### `xrtSetIsSubset`

判断左集合是否为右集合的子集，可选择严格子集。

```c
bool xrtSetIsSubset(
	const xset* pLeft,
	const xset* pRight,
	bool bProper
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空、兼容 | 左集合 |
| `pRight` | 输入 | 非空、兼容 | 右集合 |
| `bProper` | 输入 | — | 是否要求严格子集 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是子集 | — |
| `false` | 不是子集 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_STATE` — 集合非空、状态非法或两个集合不兼容

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 子集判断

```c
		if ( xrtSetIsSubset(pThree, pA, false) ||
			!xrtSetIsSubset(pA, pThree, false) ||
			!xrtSetIsSubset(pA, pThree, true) ||
			!xrtSetIsSuperset(pThree, pA, false) ||
			!xrtSetIsSuperset(pThree, pA, true) ||
			xrtSetIsSuperset(pA, pThree, false) ||
			xrtSetIsDisjoint(pA, pThree) ) {
```

### `xrtSetIsSuperset`

判断左集合是否为右集合的超集，可选择严格超集。

```c
bool xrtSetIsSuperset(
	const xset* pLeft,
	const xset* pRight,
	bool bProper
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空、兼容 | 左集合 |
| `pRight` | 输入 | 非空、兼容 | 右集合 |
| `bProper` | 输入 | — | 是否要求严格超集 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是超集 | — |
| `false` | 不是超集 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_STATE` — 集合非空、状态非法或两个集合不兼容

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 超集判断

```c
			!xrtSetIsSuperset(pThree, pA, false) ||
```

### `xrtSetIsDisjoint`

判断两个兼容集合是否没有任何共同元素。

```c
bool xrtSetIsDisjoint(
	const xset* pLeft,
	const xset* pRight
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空、兼容 | 左集合 |
| `pRight` | 输入 | 非空、兼容 | 右集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是无共同元素 | — |
| `false` | 不是无共同元素 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_STATE` — 集合非空、状态非法或两个集合不兼容

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 互斥判断

```c
		!xrtSetIsDisjoint(pA, pB) ) {
```

### `xrtSetEqual`

判断两个兼容集合是否拥有相同元素。

```c
bool xrtSetEqual(const xset* pLeft, const xset* pRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空、兼容 | 左集合 |
| `pRight` | 输入 | 非空、兼容 | 右集合 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是相同 | — |
| `false` | 不是相同 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_STATE` — 集合非空、状态非法或两个集合不兼容

#### 范例

[tour](../../examples/containers/set_tour/main.c) · 相等判断

```c
				!xrtSetEqual(pA, pClone) ) {
```

## 错误

| 错误 | 典型原因 |
| --- | --- |
| `XERR_ARGUMENT` | 空指针、无效对齐、策略不兼容、输出区间与集合内存重叠 |
| `XERR_STATE` | 非法集合状态、回调重入、访问期间修改、复制后键不一致、失效迭代器 |
| `XERR_MEMORY` | 桶数组、条目或结果集合分配失败 |
| `XERR_RANGE` | 容量、条目大小或地址区间溢出 |

完整可执行示例：

- `examples/containers/set/main.c`：整数去重、确定顺序与交集。
- `examples/containers/set/owned/main.c`：自定义键、深复制、规范元素、克隆与资源移交。
