# 内存池 API

Pool 提供三种内存池：单页池、固定对象池与变长池；统一支持标记-清扫式回收，适合每帧/每请求的成批分配。

## 分层模型

内存池由浅入深分为三层：

| 层 | 类型 | 适用场景 |
|---|---|---|
| 单页 | `xpoolpage` | 精确控制一个 1 到 256 槽固定对象页，或搭建更高层分配器 |
| 固定池 | `xpool` | 同一种结构或节点的长期高频分配，按约 64 KiB 目标自适应页容量并自动跨页增长 |
| 变长池 | `xmempool` | 同一生命周期域内的多尺寸内存，小块池化，大块独立登记 |

旧版 `MemUnit` 的 256 槽默认布局、空闲槽复用、满页边界和显式标记回收被保留；重复实现多页固定块管理的 `BSMM` 与 `FSMemPool` 合并为 `xpool`。多页固定池不再把每种对象都机械放大为 256 个槽：小对象仍保持 256 槽吞吐，大对象按目标页字节数降低槽数。`xmempool` 继续采用 16 字节尺寸类和默认 1024 字节分界，但删除了未参与当前分配路径的二叉树、LUT 和对象前 4 字节头。

三个层次都可独立裁剪。`XRT_FEATURE_POOL` 依赖 `XRT_FEATURE_POOL_PAGE`，`XRT_FEATURE_MEMORY_POOL` 依赖前两层。

## 共同契约

### 所有权与安全

池只接受自己当前持有的活动指针。释放外部指针、槽内部指针、另一个池的指针或已经释放的指针会返回 `false`，不会读取用户指针前方内存。释放后地址可以立即复用，旧指针随即失效。

`Init` / `Unit` 用于栈上或嵌入结构，`Create` / `Destroy` 用于堆上池对象。同一结构再次 `Init` 前必须先 `Unit`；直接重复初始化会丢失原资源。`Unit` 和 `Destroy` 不调用用户对象析构器；需要逐对象清理时，应先使用 `Visit` 或在上层保存生命周期信息。

公共结构用于栈上初始化和诊断，调用方不得直接修改字段。池对象和其返回的地址在 `Unit` / `Destroy` 后全部失效。

### 对齐

默认对齐是 16 字节。显式对齐必须是非零二次幂。对象大小会向对齐值取整，但 `xpoolpageinfo.ItemSize` 和 `xpoolinfo.ItemSize` 始终保留用户对象大小。

### 线程

三层池都不包含锁，同一时间只能由一个线程或执行上下文操作。不同池可以并行使用。需要跨线程共享时，应由上层同步或使用并发模块提供的共享包装，不在热路径中隐式加锁。

旧版 `XRT_OBJMODE_LOCAL` 会检查线程归属，`XRT_OBJMODE_SHARED` 会在每次池操作中隐式加锁。这项能力没有被遗漏，而是从通用池热路径中移除：线程归属检查不能覆盖协程迁移，共享模式又让所有调用永久承担同步分支和锁开销。新版本将“无锁线程内池”和“显式同步共享包装”分层，调用成本和并发边界都更清晰。

`tests/containers/test_container_external_sync.c` 让四个线程通过同一外部 Mutex 组合使用固定池和四类基础容器，并验证分配/释放计数相等、活动对象归零。该测试直接承接旧 Phase 2 的 shared allocator/container 场景。

### 标记回收

池只提供显式标记与扫描，不查找根，也不遍历对象图：

1. 调用方从自己的根集合出发，对全部可达块调用 `Mark`。
2. `Sweep` 释放未标记块，并清除幸存块的标记。
3. 下一轮必须重新标记。

`FreeMarked` 是独立的选择性批量释放操作，只释放已标记块。旧版含义不清的 `GC(pool, boolean)` 不再存在。

### 错误

池错误使用稳定域 `xrt.pool`：

| 代码 | 常量 | 含义 |
|---|---|---|
| 1 | `XPOOL_ERROR_INVALID_POINTER` | 指针不是该池的精确槽起点或活动大块 |
| 2 | `XPOOL_ERROR_NOT_ALLOCATED` | 槽已经空闲 |
| 3 | `XPOOL_ERROR_PAGE_FULL` | 单页配置的槽全部使用，错误种类为 `XERR_AGAIN` |
| 4 | `XPOOL_ERROR_INVALID_ALIGNMENT` | 对齐不是有效二次幂 |
| 5 | `XPOOL_ERROR_INVALID_SIZE` | 固定槽大小为零 |
| 6 | `XPOOL_ERROR_INDEX_OUT_OF_RANGE` | 单页槽索引超出已建立范围 |
| 7 | `XPOOL_ERROR_VISIT_ACTIVE` | 活动访问器期间尝试改变分配集合或嵌套访问 |
| 8 | `XPOOL_ERROR_INVALID_CAPACITY` | 显式页槽数不在 1 到 256 范围内 |

参数错误使用 `XERR_ARGUMENT`，大小计算溢出使用 `XERR_RANGE`，底层分配失败使用 `XERR_MEMORY`。`Owns`、`Size`、`GetInfo`、`Get` 和合法的空遍历是查询接口，不因“未找到”设置错误。

## 常量

### `XRT_POOL_PAGE_CAPACITY`

单页支持的最大槽数，也是快捷单页入口的默认槽数，值为 256。

### `XRT_POOL_PAGE_BYTES_DEFAULT`

多页固定池的默认目标页字节数，值为 65536。自动布局使用
`clamp(65536 / stride, 1, 256)` 选择每页槽数，其中 `stride` 是对齐后的实际槽步长。
因此小对象保持 256 槽，大对象不会仅因进入固定池就预留 256 倍对象大小的首批内存。
当单个槽已经超过 64 KiB 时，一页只包含一个槽。

### `XRT_POOL_ALIGNMENT_DEFAULT`

默认对象对齐，值为 16。

### `XRT_MEMPOOL_CLASS_STEP`

变长池的小块尺寸类步长，值为 16。

### `XRT_MEMPOOL_CUTOFF_DEFAULT`

变长池默认小块分界，值为 1024。

### `xpoolerror`

内存池错误代码在 xrt.pool 域内稳定。

```c
typedef enum xpoolerror {
	XPOOL_ERROR_INVALID_POINTER = 1,
	XPOOL_ERROR_NOT_ALLOCATED,
	XPOOL_ERROR_PAGE_FULL,
	XPOOL_ERROR_INVALID_ALIGNMENT,
	XPOOL_ERROR_INVALID_SIZE,
	XPOOL_ERROR_INDEX_OUT_OF_RANGE,
	XPOOL_ERROR_VISIT_ACTIVE,
	XPOOL_ERROR_INVALID_CAPACITY
} xpoolerror;
```

| 值 | 语义 |
|---|---|
| `XPOOL_ERROR_INVALID_POINTER` | 无效POINTER |
| `XPOOL_ERROR_NOT_ALLOCATED` | 失败 |
| `XPOOL_ERROR_PAGE_FULL` | PAGE已满 |
| `XPOOL_ERROR_INVALID_ALIGNMENT` | 无效ALIGNMENT |
| `XPOOL_ERROR_INVALID_SIZE` | 无效尺寸 |
| `XPOOL_ERROR_INDEX_OUT_OF_RANGE` | 索引OUTOF范围越界 |
| `XPOOL_ERROR_VISIT_ACTIVE` | 失败 |

### `xmempoolbucket`

变长池的 16 字节尺寸类内部桶结构（不透明，仅实现内部使用）。


```c
typedef struct xmempoolbucket xmempoolbucket;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xmempoollarge`

超过池化上限的独立大块登记结构（不透明，仅实现内部使用）。


```c
typedef struct xmempoollarge xmempoollarge;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

## 单页池

### `xpoolpage`

`Allocation` 和 `Memory` 分别是底层分配地址与对齐后的槽区；`ItemSize`、`Stride`、`Alignment`、`MemorySize` 和 `Capacity` 描述布局。`Used` 与 `Marked` 是外置位图，因而不占用户对象空间。`FreeList`、`LiveCount`、`NextIndex` 和 `FreeCount` 管理分配状态。四个链指针、`Parent` 和 `Flags` 供上层固定池维护页关系，不得由调用方修改。

### `xpoolpageinfo`

`ItemSize` 是用户对象大小，`Stride` 是实际槽步长，`Alignment` 是对齐；`LiveCount`、`FreeCount` 和 `Capacity` 分别表示活动槽、可直接复用槽和总槽数。

### `xrtPoolPageInit`

使用默认 16 字节对齐初始化一个空的 256 槽页。

```c
bool xrtPoolPageInit(xpoolpage* pPage, size_t iItemSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPage` | 输出 | 非空 | 接收页 |
| `iItemSize` | 输入 | > 0 | 每槽字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 布局参数非法 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 项大小或容量为零、溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[page](../../examples/memory/pool_page/main.c) · 默认初始化

```c
		if ( !xrtPoolPageInit(&DefaultPage, sizeof(uint64)) ) {
```

### `xrtPoolPageInitAligned`

使用指定的二次幂对齐初始化一个空的 256 槽页。

```c
bool xrtPoolPageInitAligned(xpoolpage* pPage, size_t iItemSize, size_t iAlignment)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPage` | 输出 | 非空 | 接收页 |
| `iItemSize` | 输入 | > 0 | 每槽字节数 |
| `iAlignment` | 输入 | 二次幂 | 槽对齐 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 布局参数非法 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 项大小或容量为零、溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[page](../../examples/memory/pool_page/main.c) · 对齐初始化

```c
	if ( !xrtPoolPageInitAligned(&tPage, sizeof(int), 32) ) {
```

### `xrtPoolPageInitLayout`

使用显式对齐和槽数初始化一个空页。

```c
bool xrtPoolPageInitLayout(
	xpoolpage* pPage,
	size_t iItemSize,
	size_t iAlignment,
	size_t iCapacity
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPage` | 输出 | 非空 | 接收页 |
| `iItemSize` | 输入 | > 0 | 每槽字节数 |
| `iAlignment` | 输入 | 二次幂 | 槽对齐 |
| `iCapacity` | 输入 | > 0 | 槽数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 布局参数非法 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 项大小或容量为零、溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[page](../../examples/memory/pool_page/main.c) · 完整布局初始化

```c
		if ( !xrtPoolPageInitLayout(&LayoutPage, sizeof(uint64),
				32u, 8u) ||
			(LayoutPage.Capacity != 8u) ) {
```

### `xrtPoolPageCreate`

创建一个使用默认 16 字节对齐的 256 槽页。

```c
xpoolpage* xrtPoolPageCreate(size_t iItemSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iItemSize` | 输入 | > 0 | 每槽字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 页（含槽内存） | — |
| `NULL` | 创建失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 项大小或容量为零、溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[page](../../examples/memory/pool_page/main.c) · 创建页

```c
	pCreated = xrtPoolPageCreate(sizeof(uint64));
```

### `xrtPoolPageCreateAligned`

创建一个使用指定对齐的 256 槽页。

```c
xpoolpage* xrtPoolPageCreateAligned(size_t iItemSize, size_t iAlignment)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iItemSize` | 输入 | > 0 | 每槽字节数 |
| `iAlignment` | 输入 | 二次幂 | 槽对齐 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 页（含槽内存） | — |
| `NULL` | 创建失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 项大小或容量为零、溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[page](../../examples/memory/pool_page/main.c) · 创建页（对齐）

```c
		pAligned = xrtPoolPageCreateAligned(sizeof(uint64), 32u);
```

### `xrtPoolPageCreateLayout`

创建一个使用显式对齐和槽数的页。

```c
xpoolpage* xrtPoolPageCreateLayout(
	size_t iItemSize,
	size_t iAlignment,
	size_t iCapacity
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iItemSize` | 输入 | > 0 | 每槽字节数 |
| `iAlignment` | 输入 | 二次幂 | 槽对齐 |
| `iCapacity` | 输入 | > 0 | 槽数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 页（含槽内存） | — |
| `NULL` | 创建失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 项大小或容量为零、溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[page](../../examples/memory/pool_page/main.c) · 创建页（完整布局）

```c
	pCompact = xrtPoolPageCreateLayout(8192, 64, 4);
```

### `xrtPoolPageUnit`

释放页持有的槽内存，但不释放页结构。

```c
void xrtPoolPageUnit(xpoolpage* pPage)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPage` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 槽内存已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[page](../../examples/memory/pool_page/main.c) · 释放槽内存

```c
		xrtPoolPageUnit(&tPage);
```

### `xrtPoolPageDestroy`

释放页持有的全部资源和页结构。

```c
void xrtPoolPageDestroy(xpoolpage* pPage)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPage` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 页已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[page](../../examples/memory/pool_page/main.c) · 销毁页

```c
		xrtPoolPageDestroy(pCompact);
```

### `xrtPoolPageAlloc`

分配一个未初始化槽，页满时返回空指针并设置 `XERR_AGAIN`。

```c
ptr xrtPoolPageAlloc(xpoolpage* pPage)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPage` | 输入 | 非空 | 目标页 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 槽内存，至少按 16 字节对齐 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 页已满，无空闲槽

#### 范例

[page](../../examples/memory/pool_page/main.c) · 分配槽

```c
	pDrop = (int*)xrtPoolPageAlloc(&tPage);
```

### `xrtPoolPageCalloc`

分配并清零一个槽，页满时返回空指针并设置 `XERR_AGAIN`。

```c
ptr xrtPoolPageCalloc(xpoolpage* pPage)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPage` | 输入 | 非空 | 目标页 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 槽内存，至少按 16 字节对齐 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_AGAIN` — 页已满，无空闲槽

#### 范例

[page](../../examples/memory/pool_page/main.c) · 清零分配槽

```c
	pKeep = (int*)xrtPoolPageCalloc(&tPage);
```

### `xrtPoolPageFree`

安全释放一个活动槽，非法、跨页或重复释放均返回 `false`。

```c
bool xrtPoolPageFree(xpoolpage* pPage, ptr pMemory)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPage` | 输入 | 非空 | 目标页 |
| `pMemory` | 输入 | 本页活动槽 | 要释放的槽 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已释放 | — |
| `false` | 非法、跨页或重复释放 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 重复释放、指针不属于本对象或内部状态非法

#### 范例

[page](../../examples/memory/pool_page/main.c) · 释放槽

```c
	if ( (pValue == NULL) || !xrtPoolPageFree(&tPage, pValue) ) {
```

### `xrtPoolPageFreeAt`

按槽索引释放活动对象。

```c
bool xrtPoolPageFreeAt(xpoolpage* pPage, size_t iIndex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPage` | 输入 | 非空 | 目标页 |
| `iIndex` | 输入 | < 槽数 | 槽索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已释放 | — |
| `false` | 空闲或越界 | 不设错 |

#### 错误

- 空闲槽或索引越界返回 `false` 且不设置错误；空句柄 `XERR_ARGUMENT`

#### 范例

[page](../../examples/memory/pool_page/main.c) · 按索引释放

```c
		!xrtPoolPageFreeAt(&tPage, iIndex)
```

### `xrtPoolPageGet`

返回指定索引处的活动对象，空闲或越界时返回空指针。

```c
ptr xrtPoolPageGet(const xpoolpage* pPage, size_t iIndex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPage` | 输入 | 非空 | 目标页 |
| `iIndex` | 输入 | < 槽数 | 槽索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 活动对象 | — |
| `NULL` | 空闲或越界 | 不设错 |

#### 错误

- 空闲槽或索引越界返回 `NULL` 且不设置错误；空句柄 `XERR_ARGUMENT`

#### 范例

[page](../../examples/memory/pool_page/main.c) · 按索引取对象

```c
		(xrtPoolPageGet(&tPage, iIndex) != pKeep)
```

### `xrtPoolPageIndex`

获取活动对象的槽索引。

```c
bool xrtPoolPageIndex(const xpoolpage* pPage, const void* pMemory, size_t* pIndex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPage` | 输入 | 非空 | 目标页 |
| `pMemory` | 输入 | 本页活动槽 | 目标对象 |
| `pIndex` | 输出 | 非空 | 接收槽索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出索引 | — |
| `false` | 非本页活动对象 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 重复释放、指针不属于本对象或内部状态非法

#### 范例

[page](../../examples/memory/pool_page/main.c) · 对象索引

```c
		!xrtPoolPageIndex(&tPage, pKeep, &iIndex) ||
```

### `xrtPoolPageOwns`

判断指针当前是否属于该页的活动槽。

```c
bool xrtPoolPageOwns(const xpoolpage* pPage, const void* pMemory)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPage` | 输入 | 非空 | 目标页 |
| `pMemory` | 输入 | 任意 | 待判断指针 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否本页活动槽 | 不设错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[page](../../examples/memory/pool_page/main.c) · 归属判断

```c
		!xrtPoolPageOwns(&tPage, pKeep) ||
```

### `xrtPoolPageMark`

将一个活动槽标记为本轮可达对象。

```c
bool xrtPoolPageMark(xpoolpage* pPage, ptr pMemory)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPage` | 输入/输出 | 非空 | 目标页 |
| `pMemory` | 输入 | 本页活动槽 | 目标对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已标记 | — |
| `false` | 非本页/池活动对象 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 重复释放、指针不属于本对象或内部状态非法

#### 范例

[page](../../examples/memory/pool_page/main.c) · 标记

```c
	if ( !xrtPoolPageMark(&tPage, pKeep) || (xrtPoolPageSweep(&tPage) != 1) ) {
```

### `xrtPoolPageSweep`

释放未标记槽，并清除幸存槽的标记。

```c
size_t xrtPoolPageSweep(xpoolpage* pPage)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPage` | 输入/输出 | 非空 | 目标页 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 释放的未标记槽数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[page](../../examples/memory/pool_page/main.c) · 清扫

```c
	if ( !xrtPoolPageMark(&tPage, pKeep) || (xrtPoolPageSweep(&tPage) != 1) ) {
```

### `xrtPoolPageFreeMarked`

释放已标记槽，适合显式批量选择释放。

```c
size_t xrtPoolPageFreeMarked(xpoolpage* pPage)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPage` | 输入/输出 | 非空 | 目标页 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 释放的已标记槽数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[page](../../examples/memory/pool_page/main.c) · 释放已标记

```c
	if ( !xrtPoolPageMark(&tPage, pKeep) || (xrtPoolPageFreeMarked(&tPage) != 1) ) {
```

### `xrtPoolPageReset`

将页内全部槽恢复为空闲状态并返回释放的活动槽数。

```c
size_t xrtPoolPageReset(xpoolpage* pPage)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPage` | 输入/输出 | 非空 | 目标页 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 释放的活动槽数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[page](../../examples/memory/pool_page/main.c) · 全部重置

```c
		(xrtPoolPageReset(&tPage) != 2)
```

### `xrtPoolPageGetInfo`

获取单页当前状态。

```c
void xrtPoolPageGetInfo(const xpoolpage* pPage, xpoolpageinfo* pInfo)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPage` | 输入 | 非空 | 目标页 |
| `pInfo` | 输出 | 非空 | 接收状态快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 快照已写出 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[page](../../examples/memory/pool_page/main.c) · 状态查询

```c
	xrtPoolPageGetInfo(&tPage, &tInfo);
```

## 固定对象池

### `xpool`

`Pages` 是全部页链，`Available` 是可分配页链，`Index` 是按槽区地址排序的安全查找索引。`ItemSize`、`Alignment`、`PageCapacity`、`PageCount`、`EmptyPages`、`LiveCount`、`PeakCount`、`AllocCount`、`FreeCount` 和 `RetainEmpty` 提供布局、统计与保留策略。`IndexCapacity` 和 `Flags` 是内部状态。

### `xpoolinfo`

包含对象大小、步长、对齐、每页槽数、页数、空页数、实时/峰值对象数、总容量和累计分配/释放次数。理论容量无法用 `size_t` 表示时，`Capacity` 饱和为 `SIZE_MAX`。

### `xpoolvisitor`

签名为 `bool visitor(ptr object, size_t index, ptr userData)`。`index` 是本次遍历从零开始的连续序号，不是可持久化句柄。返回 `false` 提前停止。

### `xrtPoolInit`

使用默认 16 字节对齐初始化固定对象池。

```c
bool xrtPoolInit(xpool* pPool, size_t iItemSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输出 | 非空 | 接收池 |
| `iItemSize` | 输入 | > 0 | 每对象字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 布局参数非法 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 项大小或容量为零、溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[pool](../../examples/memory/pool/main.c) · 默认初始化

```c
		if ( !xrtPoolInit(&DefaultPool, sizeof(examplejob)) ) {
```

### `xrtPoolInitAligned`

使用指定的二次幂对齐初始化固定对象池。

```c
bool xrtPoolInitAligned(xpool* pPool, size_t iItemSize, size_t iAlignment)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输出 | 非空 | 接收池 |
| `iItemSize` | 输入 | > 0 | 每对象字节数 |
| `iAlignment` | 输入 | 二次幂 | 对象对齐 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 布局参数非法 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 项大小或容量为零、溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[pool](../../examples/memory/pool/main.c) · 对齐初始化

```c
	if ( !xrtPoolInitAligned(&tPool, sizeof(examplejob), 32) ) {
```

### `xrtPoolInitLayout`

使用显式对齐和每页槽数初始化固定对象池。

```c
bool xrtPoolInitLayout(
	xpool* pPool,
	size_t iItemSize,
	size_t iAlignment,
	size_t iPageCapacity
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输出 | 非空 | 接收池 |
| `iItemSize` | 输入 | > 0 | 每对象字节数 |
| `iAlignment` | 输入 | 二次幂 | 对象对齐 |
| `iPageCapacity` | 输入 | > 0 | 每页槽数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 布局参数非法 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 项大小或容量为零、溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[pool](../../examples/memory/pool/main.c) · 完整布局初始化

```c
		if ( !xrtPoolInitLayout(&LayoutPool, sizeof(examplejob),
				32u, 64u) ||
			(LayoutPool.PageCapacity != 64u) ) {
```

### `xrtPoolCreate`

创建使用默认 16 字节对齐的固定对象池。

```c
xpool* xrtPoolCreate(size_t iItemSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iItemSize` | 输入 | > 0 | 每对象字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 池 | — |
| `NULL` | 创建失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 项大小或容量为零、溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[pool](../../examples/memory/pool/main.c) · 创建池

```c
	pCompact = xrtPoolCreate(8192);
```

### `xrtPoolCreateAligned`

创建使用指定对齐的固定对象池。

```c
xpool* xrtPoolCreateAligned(size_t iItemSize, size_t iAlignment)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iItemSize` | 输入 | > 0 | 每对象字节数 |
| `iAlignment` | 输入 | 二次幂 | 对象对齐 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 池 | — |
| `NULL` | 创建失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 项大小或容量为零、溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[pool](../../examples/memory/pool/main.c) · 创建池（对齐）

```c
		pAligned = xrtPoolCreateAligned(sizeof(examplejob), 32u);
```

### `xrtPoolCreateLayout`

创建使用显式对齐和每页槽数的固定对象池。

```c
xpool* xrtPoolCreateLayout(
	size_t iItemSize,
	size_t iAlignment,
	size_t iPageCapacity
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iItemSize` | 输入 | > 0 | 每对象字节数 |
| `iAlignment` | 输入 | 二次幂 | 对象对齐 |
| `iPageCapacity` | 输入 | > 0 | 每页槽数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 池 | — |
| `NULL` | 创建失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 项大小或容量为零、溢出
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[pool](../../examples/memory/pool/main.c) · 创建池（完整布局）

```c
	pCreated = xrtPoolCreateLayout(sizeof(examplejob), 32, 128);
```

### `xrtPoolUnit`

释放池持有的全部页，但不释放池结构。

```c
void xrtPoolUnit(xpool* pPool)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 全部页已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[pool](../../examples/memory/pool/main.c) · 释放页

```c
			xrtPoolUnit(&tPool);
```

### `xrtPoolDestroy`

释放池持有的全部资源和池结构。

```c
void xrtPoolDestroy(xpool* pPool)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 池已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[pool](../../examples/memory/pool/main.c) · 销毁池

```c
		xrtPoolDestroy(pCompact);
```

### `xrtPoolAlloc`

分配一个未初始化对象；池需要新页时可能分配内存。

```c
ptr xrtPoolAlloc(xpool* pPool)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标池 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 槽内存，至少按 16 字节对齐 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[pool](../../examples/memory/pool/main.c) · 分配对象

```c
	pReused = (examplejob*)xrtPoolAlloc(&tPool);
```

### `xrtPoolCalloc`

分配并清零一个对象；池需要新页时可能分配内存。

```c
ptr xrtPoolCalloc(xpool* pPool)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标池 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 槽内存，至少按 16 字节对齐 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[pool](../../examples/memory/pool/main.c) · 清零分配对象

```c
		arrJob[i] = (examplejob*)xrtPoolCalloc(&tPool);
```

### `xrtPoolFree`

安全释放活动对象，非法、跨池或重复释放均返回 `false`。

```c
bool xrtPoolFree(xpool* pPool, ptr pObject)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标池 |
| `pObject` | 输入 | 本池活动对象 | 要释放的对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已释放 | — |
| `false` | 非法、跨页或重复释放 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 重复释放、指针不属于本对象或内部状态非法

#### 范例

[pool](../../examples/memory/pool/main.c) · 释放对象

```c
	if ( !xrtPoolFree(&tPool, pReleased) ) {
```

### `xrtPoolOwns`

判断指针当前是否属于该池的活动对象。

```c
bool xrtPoolOwns(const xpool* pPool, const void* pObject)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标池 |
| `pObject` | 输入 | 任意 | 待判断指针 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否本池活动对象 | 不设错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[pool](../../examples/memory/pool/main.c) · 归属判断

```c
	if ( !xrtPoolOwns(&tPool, pReused) ) {
```

### `xrtPoolMark`

标记一个活动对象为本轮可达。

```c
bool xrtPoolMark(xpool* pPool, ptr pObject)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入/输出 | 非空 | 目标池 |
| `pObject` | 输入 | 本池活动对象 | 目标对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已标记 | — |
| `false` | 非本页/池活动对象 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 重复释放、指针不属于本对象或内部状态非法

#### 范例

[pool](../../examples/memory/pool/main.c) · 标记

```c
	if ( !xrtPoolMark(&tPool, arrJob[0]) || (xrtPoolSweep(&tPool) != 299) ) {
```

### `xrtPoolSweep`

释放全部未标记对象，并清除幸存对象标记。

```c
size_t xrtPoolSweep(xpool* pPool)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入/输出 | 非空 | 目标池 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 释放的未标记对象数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pool](../../examples/memory/pool/main.c) · 清扫

```c
	if ( !xrtPoolMark(&tPool, arrJob[0]) || (xrtPoolSweep(&tPool) != 299) ) {
```

### `xrtPoolFreeMarked`

释放全部已标记对象。

```c
size_t xrtPoolFreeMarked(xpool* pPool)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入/输出 | 非空 | 目标池 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 释放的已标记对象数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pool](../../examples/memory/pool/main.c) · 释放已标记

```c
	if ( !xrtPoolMark(&tPool, arrJob[0]) || (xrtPoolFreeMarked(&tPool) != 1) ) {
```

### `xrtPoolReset`

释放全部活动对象，并按保留策略回收空页。

```c
size_t xrtPoolReset(xpool* pPool)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入/输出 | 非空 | 目标池 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 释放的活动对象数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pool](../../examples/memory/pool/main.c) · 全部重置

```c
	if ( xrtPoolReset(&tPool) != 300 ) {
```

### `xrtPoolTrim`

回收多余空页，返回真正释放的页数。

```c
size_t xrtPoolTrim(xpool* pPool, size_t iRetainEmpty)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入/输出 | 非空 | 目标池 |
| `iRetainEmpty` | 输入 | — | 保留的空页数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 释放的页数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pool](../../examples/memory/pool/main.c) · 裁剪空页

```c
	if ( xrtPoolTrim(&tPool, 0) == 0 ) {
```

### `xrtPoolSetRetain`

设置自动保留的空页数，并立即执行一次裁剪。

```c
void xrtPoolSetRetain(xpool* pPool, size_t iRetainEmpty)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入/输出 | 非空 | 目标池 |
| `iRetainEmpty` | 输入 | — | 保留的空页数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已设置并裁剪 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pool](../../examples/memory/pool/main.c) · 设置保留策略

```c
	xrtPoolSetRetain(&tPool, 2);
```

### `xrtPoolGet`

获取固定对象池当前状态。

```c
void xrtPoolGet(const xpool* pPool, xpoolinfo* pInfo)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标池 |
| `pInfo` | 输出 | 非空 | 接收状态快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 快照已写出 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pool](../../examples/memory/pool/main.c) · 状态查询

```c
	xrtPoolGet(&tPool, &tInfo);
```

### `xrtPoolVisit`

访问活动对象；遍历期间只允许查询和标记，不得改变池的分配集合。

```c
size_t xrtPoolVisit(xpool* pPool, xpoolvisitor pVisitor, ptr pUserData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标池 |
| `pVisitor` | 输入 | 非空 | 访问回调 |
| `pUserData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 实际访问的对象数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[pool](../../examples/memory/pool/main.c) · 遍历对象

```c
	if ( xrtPoolVisit(&tPool, exampleVisitJob, &iVisited) != 300 ) {
```

## 变长内存池

### `xmempool`

`Buckets` 是 16 字节尺寸类数组，`Pages` 是所有小块页的有序查找索引，`Large` 是独立大块哈希登记表。`Cutoff`、`ClassCount`、`PageCount`、`LargeCount`、`LiveCount`、`PeakCount`、`LiveBytes`、`PeakBytes`、`AllocCount` 和 `FreeCount` 是配置与统计。其余容量、删除计数和 `Flags` 是内部状态。

### `xmempoolinfo`

包含分界、尺寸类步长/数量、页数、小块/大块数量、实时/峰值块数、实时/峰值可用字节及累计操作次数。小块字节按尺寸类可用大小统计，不是原请求大小。

### `xmempoolvisitor`

签名为 `bool visitor(ptr memory, size_t size, size_t alignment, ptr userData)`。`size` 是 `xrtMemPoolSize` 可查询的安全可用大小，返回 `false` 提前停止。

### `xrtMemPoolInit`

初始化变长池，`iCutoff` 为零时使用默认值 1024。

```c
bool xrtMemPoolInit(xmempool* pPool, size_t iCutoff)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输出 | 非空 | 接收池 |
| `iCutoff` | 输入 | 0 = 1024 | 池化小块上限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[mempool](../../examples/memory/memory_pool/main.c) · 初始化

```c
	if ( !xrtMemPoolInit(&tPool, 128) ) {
```

### `xrtMemPoolCreate`

创建变长池，`iCutoff` 为零时使用默认值 1024。

```c
xmempool* xrtMemPoolCreate(size_t iCutoff)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iCutoff` | 输入 | 0 = 1024 | 池化小块上限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 变长池 | — |
| `NULL` | 创建失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[mempool](../../examples/memory/memory_pool/main.c) · 创建池

```c
	pCreated = xrtMemPoolCreate(0);
```

### `xrtMemPoolUnit`

释放池持有的全部资源，但不释放池结构；不调用块内对象析构器。

```c
void xrtMemPoolUnit(xmempool* pPool)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 全部资源已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[mempool](../../examples/memory/memory_pool/main.c) · 释放资源

```c
		xrtMemPoolUnit(&tPool);
```

### `xrtMemPoolDestroy`

释放池持有的全部资源和池结构；不调用块内对象析构器。

```c
void xrtMemPoolDestroy(xmempool* pPool)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 池已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[mempool](../../examples/memory/memory_pool/main.c) · 销毁池

```c
	xrtMemPoolDestroy(pCreated);
```

### `xrtMemPoolAlloc`

按 16 字节对齐分配内存，大小为零时仍返回至少一个可用字节。

```c
ptr xrtMemPoolAlloc(xmempool* pPool, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标池 |
| `iSize` | 输入 | — | 请求字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 槽内存，至少按 16 字节对齐 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[mempool](../../examples/memory/memory_pool/main.c) · 分配块

```c
	pPacket = (bytes)xrtMemPoolAlloc(&tPool, 4096);
```

### `xrtMemPoolCalloc`

分配 `iCount * iSize` 字节并清零；乘法溢出时失败，总大小为零时仍分配至少一个字节。

```c
ptr xrtMemPoolCalloc(xmempool* pPool, size_t iCount, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标池 |
| `iCount` | 输入 | — | 元素数量 |
| `iSize` | 输入 | — | 每元素字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 槽内存，至少按 16 字节对齐 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — `iCount * iSize` 乘法溢出
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[mempool](../../examples/memory/memory_pool/main.c) · 清零分配块

```c
	sText = (char*)xrtMemPoolCalloc(&tPool, 1, 24);
```

### `xrtMemPoolAllocAligned`

按指定二次幂对齐分配，零大小仍有效，超过 16 字节对齐时走独立大块。

```c
ptr xrtMemPoolAllocAligned(xmempool* pPool, size_t iSize, size_t iAlignment)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标池 |
| `iSize` | 输入 | — | 请求字节数 |
| `iAlignment` | 输入 | 二次幂 | 对齐 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 槽内存，至少按 16 字节对齐 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_VALUE` — 对齐不是二次幂
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[mempool](../../examples/memory/memory_pool/main.c) · 对齐分配块

```c
	pAligned = (bytes)xrtMemPoolAllocAligned(&tPool, 7, 64);
```

### `xrtMemPoolRealloc`

调整池内块大小并保留已有内容，大小为零时释放。

```c
ptr xrtMemPoolRealloc(xmempool* pPool, ptr pMemory, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标池 |
| `pMemory` | 输入 | 本池活动块或空 | 要调整的块 |
| `iSize` | 输入 | — | 新字节数，0 = 释放 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 槽内存，至少按 16 字节对齐 | — |
| `NULL` | 失败 | 见错误 |
（`pMemory` 为空且大小非零时等价分配）

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 重复释放、指针不属于本对象或内部状态非法
- `XERR_MEMORY` — 槽内存分配失败

#### 范例

[mempool](../../examples/memory/memory_pool/main.c) · 调整块大小

```c
	sText = (char*)xrtMemPoolRealloc(&tPool, sText, 80);
```

### `xrtMemPoolFree`

安全释放池内活动块，非法、跨池或重复释放均返回 `false`。

```c
bool xrtMemPoolFree(xmempool* pPool, ptr pMemory)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标池 |
| `pMemory` | 输入 | 本池活动块 | 要释放的块 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已释放 | — |
| `false` | 非法、跨页或重复释放 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 重复释放、指针不属于本对象或内部状态非法

#### 范例

[mempool](../../examples/memory/memory_pool/main.c) · 释放块

```c
		!xrtMemPoolFree(&tPool, sText)
```

### `xrtMemPoolSize`

返回活动块可安全使用的字节数，不属于该池时返回零。

```c
size_t xrtMemPoolSize(const xmempool* pPool, const void* pMemory)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标池 |
| `pMemory` | 输入 | 任意 | 待查询指针 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 可用字节数；0 = 不属于本池 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[mempool](../../examples/memory/memory_pool/main.c) · 块大小查询

```c
		(xrtMemPoolSize(&tPool, sText) < 80) ||
```

### `xrtMemPoolOwns`

判断指针当前是否属于该池的活动块。

```c
bool xrtMemPoolOwns(const xmempool* pPool, const void* pMemory)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标池 |
| `pMemory` | 输入 | 任意 | 待判断指针 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否本池活动块 | 不设错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[mempool](../../examples/memory/memory_pool/main.c) · 归属判断

```c
		!xrtMemPoolOwns(&tPool, sText) ||
```

### `xrtMemPoolMark`

标记一个活动块为本轮可达。

```c
bool xrtMemPoolMark(xmempool* pPool, ptr pMemory)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入/输出 | 非空 | 目标池 |
| `pMemory` | 输入 | 本池活动块 | 目标块 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已标记 | — |
| `false` | 非本页/池活动对象 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 重复释放、指针不属于本对象或内部状态非法

#### 范例

[mempool](../../examples/memory/memory_pool/main.c) · 标记

```c
		!xrtMemPoolMark(&tPool, sText) ||
```

### `xrtMemPoolSweep`

释放全部未标记块，并清除幸存块标记。

```c
size_t xrtMemPoolSweep(xmempool* pPool)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入/输出 | 非空 | 目标池 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 释放的未标记块数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[mempool](../../examples/memory/memory_pool/main.c) · 清扫

```c
		(xrtMemPoolSweep(&tPool) != 1)
```

### `xrtMemPoolFreeMarked`

释放全部已标记块。

```c
size_t xrtMemPoolFreeMarked(xmempool* pPool)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入/输出 | 非空 | 目标池 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 释放的已标记块数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[mempool](../../examples/memory/memory_pool/main.c) · 释放已标记

```c
		(xrtMemPoolFreeMarked(&tPool) != 1) ||
```

### `xrtMemPoolReset`

释放全部活动块，并保留每个尺寸类的一个空页。

```c
size_t xrtMemPoolReset(xmempool* pPool)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入/输出 | 非空 | 目标池 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 释放的活动块数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[mempool](../../examples/memory/memory_pool/main.c) · 全部重置

```c
		(xrtMemPoolReset(&tPool) != 2)
```

### `xrtMemPoolTrim`

将每个尺寸类的空页裁剪到指定数量。

```c
size_t xrtMemPoolTrim(xmempool* pPool, size_t iRetainEmptyPerClass)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入/输出 | 非空 | 目标池 |
| `iRetainEmptyPerClass` | 输入 | — | 每尺寸类保留空页数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 释放的页数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[mempool](../../examples/memory/memory_pool/main.c) · 裁剪空页

```c
	(void)xrtMemPoolTrim(&tPool, 0);
```

### `xrtMemPoolGet`

获取变长池当前状态。

```c
void xrtMemPoolGet(const xmempool* pPool, xmempoolinfo* pInfo)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标池 |
| `pInfo` | 输出 | 非空 | 接收状态快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 快照已写出 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[mempool](../../examples/memory/memory_pool/main.c) · 状态查询

```c
	xrtMemPoolGet(&tPool, &tInfo);
```

### `xrtMemPoolVisit`

访问活动块；遍历期间只允许查询和标记，不得改变池的分配集合。

```c
size_t xrtMemPoolVisit(
	xmempool* pPool,
	xmempoolvisitor pVisitor,
	ptr pUserData
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 目标池 |
| `pVisitor` | 输入 | 非空 | 访问回调 |
| `pUserData` | 输入 | 任意值 | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 实际访问的块数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[mempool](../../examples/memory/memory_pool/main.c) · 遍历块

```c
	if ( xrtMemPoolVisit(&tPool, exampleVisitMemory, &iVisited) != 3 ) {
```

## 旧版资产复用

这三层实现逐项审计了旧版 `memunit.h`、`bsmm.h`、`mempool_fs.h`、`mempool.h` 及其测试、范例和中英文文档。

保留并加强的资产：

- `MemUnit` 的 256 槽默认容量、释放槽复用、满页失败和“释放标记/释放未标记”两种回收语义。
- `FSMemPool` 的跨页增长、满页与可用页迁移、空页缓存策略。
- `MemPool` 的小块分级、可配置分界和大块回退路径。
- 旧测试中的尺寸边界、256 槽边界、跨页增长、复用与 GC 场景，已改为无需人工查看输出的自动断言。

新增的自适应页容量解决了旧实现的大对象放大问题：显式单页仍可选择旧版 256 槽布局，通用多页池则不会为一个 8 KiB 对象立即申请约 2 MiB，也不会为一个 1 MiB 对象立即申请约 256 MiB。上层 AVLTree 等稳定地址容器直接复用这一能力，无需重复实现大对象旁路。

明确淘汰的实现：

- 用户内存前方的 4 字节管理头。新实现用外置位图、页地址索引和大块哈希登记，避免读取未知指针前方内存，并完整支持显式对齐。
- `BSMM` 和 `FSMemPool` 两套重叠的多页管理器。其有效能力统一进入 `xpool`，不保留重复 API 和实现。
- 旧变长池未参与最终热路径的树字段与重复 LUT 状态。新尺寸类可直接通过除法定位。
- 打印内部字段、依赖人工判断结果的旧测试形式。有效场景保留，测试方式升级为自动失败。

## 范例与回归

完整范例位于：

- `examples/memory/pool_page/main.c`
- `examples/memory/pool/main.c`
- `examples/memory/memory_pool/main.c`

三个裁剪层可分别执行：

```text
python tools/build.py --suite pool_page --compiler gcc --arch native
python tools/build.py --suite pool --compiler gcc --arch native
python tools/build.py --suite memory_pool --compiler gcc --arch native
```

构建脚本同时运行普通实现、范例和对应单头文件测试。
