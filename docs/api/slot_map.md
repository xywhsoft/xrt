# SlotMap

`slot_map` 管理非空对象指针，并返回带代际的稳定句柄。它承接旧版
`xrtPtrArrayAddAlt()` 的空槽复用场景，但不会把“稠密指针序列”和“稳定句柄表”
混进同一个容器。

## 裁剪与依赖

| 能力 | 宏 | 依赖 |
|---|---|---|
| 代际槽表 | `XRT_FEATURE_SLOT_MAP` | `array` |

启用 `XRT_FEATURE_SLOT_MAP` 时必须同时启用 `XRT_FEATURE_ARRAY`。只需要连续指针
序列时，应裁掉本模块并使用 `ptr_array`。

## 适用边界

适合：

- 连接、session、worker 和任务对象的稳定句柄；
- 对象移除后复用槽位，但不能让旧句柄误命中新对象；
- 需要 O(1) 插入、查询、替换和删除的本地对象表。

不适合：

- 需要按连续索引排序、批量插入或压缩删除的序列，使用 `ptr_array`；
- 需要自动管理对象生命周期的所有权容器；
- 多执行流无锁并发访问。共享时由调用方提供外部同步。

## 稳定契约

- `xslot` 的零值永远无效；句柄包含 32 位零基索引和 32 位非零代际。
- 插入和替换拒绝 `NULL`，因此 `Get` 返回 `NULL` 可以无歧义地表示句柄无效。
- 删除槽时推进代际。复用同一索引会得到不同句柄，旧句柄永久失效。
- 单个槽的代际耗尽后会永久退役，不回绕到旧代际，因此不会产生 ABA 命中。
- 空闲槽由链表以 O(1) 复用；普通删除优先复用最近释放的槽。
- `Clear` 使全部活动句柄失效、保留容量，并从最低索引开始重建空闲链。
- 槽表没有 `Trim`。释放尾部槽后再次创建同索引会丢失代际历史，破坏旧句柄
  永久失效的保证；需要回收全部存储时使用 `Unit` 或 `Destroy`。
- 槽表只借用指针，不释放对象。`Remove` 可取回原指针，便于调用方完成析构。
- 分配失败时，已有句柄、值、数量、槽跨度、容量和结构版本保持不变。
- `Reserve` 和 `Set` 不改变结构版本；`Insert`、`Remove` 和非空 `Clear` 会使
  已开始的迭代器失效。

## 常量与类型

### `XRT_SLOT_INVALID`

```c
#define XRT_SLOT_INVALID ((xslot)0)
```

所有失败的插入返回该值。

### `XRT_SLOT_INDEX_INVALID`

```c
#define XRT_SLOT_INDEX_INVALID UINT32_MAX
```

`xrtSlotIndex()` 无法解码句柄时返回该值。有效内部索引最大为
`UINT32_MAX - 1`。

### `xslot`

```c
typedef uint64 xslot;
```

句柄可复制、比较和作为语言层整数保存，但不得自行拆位或构造。使用
`xrtSlotIndex()` 和 `xrtSlotGeneration()` 做诊断。

### `xslotmap`

```c
typedef struct xslotmap {
	xarray Storage;
	size_t Count;
	uint64 Version;
	uint32 FreeSlot;
	uint32 Reserved;
} xslotmap;
```

| 字段 | 含义 |
|---|---|
| `Storage` | 内部槽记录数组；只允许读取 `Count`、`Capacity` 等诊断信息 |
| `Count` | 当前活动对象数 |
| `Version` | 结构版本，仅供诊断 |
| `FreeSlot` | 内部空闲链表头，不得修改 |
| `Reserved` | 保留字段，必须保持为零 |

`Storage.Count` 是已经建立代际历史的槽跨度，可能大于活动 `Count`。
公开结构支持栈上和嵌入式生命周期，不表示调用方可以修改其不变量。

| 字段 | 类型 | 语义 |
|---|---|---|
| `Storage` | `xarray` | 槽存储（内联+外部） |
| `Count` | `size_t` | 活动句柄数 |
| `Version` | `uint64` | 结构版本号（代际） |
| `FreeSlot` | `uint32` | 空闲链头 |
| `Reserved` | `uint32` | 保留字段，必须保持为零 |

### `xslotmapiter`

```c
typedef struct xslotmapiter {
	const xslotmap* Map;
	size_t Next;
	uint64 Version;
} xslotmapiter;
```

迭代器由 `IterBegin` 初始化并由 `IterEnd` 清理，不应手工修改字段。

| 字段 | 类型 | 语义 |
|---|---|---|
| `Map` | `const xslotmap*` | 目标槽表借用 |
| `Next` | `size_t` | 下一扫描槽位 |
| `Version` | `uint64` | 启动时代际 |

## 句柄诊断

### `xrtSlotIndex`

返回句柄中的零基槽索引，无效句柄返回 `XRT_SLOT_INDEX_INVALID`。

```c
uint32 xrtSlotIndex(xslot Slot)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Slot` | 输入 | — | 槽句柄 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 零基槽索引 | — |
| `XRT_SLOT_INDEX_INVALID` | 无效句柄 | — |

#### 错误

- 无 — 纯位域解码，不设置错误

#### 范例

[slot_map](../../examples/containers/slot_map/main.c) · 槽索引

```c
		xrtSlotIndex(First) == xrtSlotIndex(Replacement) ? "yes" : "no",
```

### `xrtSlotGeneration`

返回句柄中的代际，无效句柄返回零。

```c
uint32 xrtSlotGeneration(xslot Slot)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Slot` | 输入 | — | 槽句柄 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 句柄代际 | — |
| `0` | 无效句柄 | — |

#### 错误

- 无 — 纯位域解码，不设置错误

#### 范例

[slot_map_tour](../../examples/containers/slot_map_tour/main.c) · 代际

```c
		(xrtSlotGeneration(SlotA) != 1u) ||
```

## 生命周期与容量

### `xrtSlotMapInit`

初始化调用方持有的空槽表。

```c
bool xrtSlotMapInit(xslotmap* pMap)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输出 | 非空 | 接收槽表 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[slot_map](../../examples/containers/slot_map/main.c) · 内嵌初始化

```c
	if ( !xrtSlotMapInit(&tConnections) ) {
```

### `xrtSlotMapCreate`

创建堆上的空槽表。

```c
xslotmap* xrtSlotMapCreate(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 槽表 | — |
| `NULL` | 分配失败 | `XERR_MEMORY` |

#### 错误

- `XERR_MEMORY` — 结构分配失败

#### 范例

[slot_map_tour](../../examples/containers/slot_map_tour/main.c) · 堆创建

```c
	pMap = xrtSlotMapCreate();
```

### `xrtSlotMapUnit`

释放槽表存储，但不释放槽内指针指向的对象。

```c
void xrtSlotMapUnit(xslotmap* pMap)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 存储已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[slot_map](../../examples/containers/slot_map/main.c) · 释放存储

```c
		xrtSlotMapUnit(&tConnections);
```

### `xrtSlotMapDestroy`

释放槽表存储和槽表结构，但不释放槽内对象。

```c
void xrtSlotMapDestroy(xslotmap* pMap)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 槽表已销毁 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[slot_map_tour](../../examples/containers/slot_map_tour/main.c) · 销毁槽表

```c
	xrtSlotMapDestroy(pMap);
```

### `xrtSlotMapClear`

清空全部活动槽并使已有句柄失效，同时保留已分配容量。

```c
void xrtSlotMapClear(xslotmap* pMap)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标槽表 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已清空，全部句柄失效 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[slot_map_tour](../../examples/containers/slot_map_tour/main.c) · 清空

```c
	xrtSlotMapClear(pMap);
```

### `xrtSlotMapReserve`

保证槽表至少具有指定存储容量。

```c
bool xrtSlotMapReserve(xslotmap* pMap, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标槽表 |
| `iCapacity` | 输入 | — | 期望容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 容量已保证 | — |
| `false` | 扩容失败 | `XERR_RANGE` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量或尺寸计算溢出
- `XERR_MEMORY` — 存储分配失败

#### 范例

[slot_map_tour](../../examples/containers/slot_map_tour/main.c) · 预留容量

```c
		!xrtSlotMapReserve(pMap, 8u) ) {
```

## 基本操作

### `xrtSlotMapInsert`

插入非空指针并返回稳定代际句柄，失败返回 `XRT_SLOT_INVALID`。

```c
xslot xrtSlotMapInsert(xslotmap* pMap, ptr pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标槽表 |
| `pValue` | 输入 | 非空 | 要存储的指针 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非零 | 稳定代际句柄 | — |
| `XRT_SLOT_INVALID` | 插入失败 | `XERR_ARGUMENT` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 空闲链状态破坏
- `XERR_RANGE` — 槽位空间耗尽或尺寸溢出

#### 范例

[slot_map](../../examples/containers/slot_map/main.c) · 插入

```c
	First = xrtSlotMapInsert(&tConnections, &tFirst);
```

### `xrtSlotMapGet`

返回有效句柄对应的指针，陈旧或不存在的句柄返回空指针。

```c
ptr xrtSlotMapGet(const xslotmap* pMap, xslot Slot)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标槽表 |
| `Slot` | 输入 | — | 槽句柄 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 槽内指针 | — |
| `NULL` | 句柄陈旧或不存在 | 不设错误 |

#### 错误

- 陈旧句柄返回 `NULL` 且不设置错误；句柄为空 `XERR_ARGUMENT`

#### 范例

[slot_map_tour](../../examples/containers/slot_map_tour/main.c) · 取指针

```c
		(xrtSlotMapGet(pMap, SlotA) != (ptr)1) ||
```

### `xrtSlotMapContains`

判断句柄当前是否仍指向活动槽，句柄失效不是错误。

```c
bool xrtSlotMapContains(const xslotmap* pMap, xslot Slot)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标槽表 |
| `Slot` | 输入 | — | 槽句柄 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是活动 | — |
| `false` | 不是活动 | 不设错误 |

#### 错误

- 无 — 句柄失效是正常结果，不设置错误

#### 范例

[slot_map](../../examples/containers/slot_map/main.c) · 句柄有效性

```c
		xrtSlotMapContains(&tConnections, First) ? "yes" : "no"
```

### `xrtSlotMapSet`

替换有效槽中的非空指针，句柄和迭代顺序保持不变。

```c
bool xrtSlotMapSet(xslotmap* pMap, xslot Slot, ptr pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标槽表 |
| `Slot` | 输入 | — | 有效槽句柄 |
| `pValue` | 输入 | 非空 | 新指针 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已替换 | — |
| `false` | 句柄陈旧或参数非法 | 不设错误 / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 陈旧句柄返回 `false` 且不设置错误

#### 范例

[slot_map_tour](../../examples/containers/slot_map_tour/main.c) · 替换指针

```c
		!xrtSlotMapSet(pMap, SlotA, (ptr)11) ||
```

### `xrtSlotMapRemove`

删除有效槽并可返回原指针，删除后旧句柄永久失效。

```c
bool xrtSlotMapRemove(xslotmap* pMap, xslot Slot, ptr* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标槽表 |
| `Slot` | 输入 | — | 有效槽句柄 |
| `pValue` | 输出 | 允许空；不得与槽表存储重叠 | 接收原指针 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已删除 | — |
| `false` | 句柄陈旧 | 不设错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 陈旧句柄返回 `false` 且不设置错误
- `XERR_STATE` — 计数与槽状态不一致（内部损坏）

#### 范例

[slot_map](../../examples/containers/slot_map/main.c) · 删除槽

```c
	if ( !xrtSlotMapRemove(&tConnections, First, NULL) ) {
```

## 迭代

```c
bool xrtSlotMapIterBegin(const xslotmap* pMap, xslotmapiter* pIterator);
ptr xrtSlotMapIterNext(xslotmapiter* pIterator, xslot* pSlot);
void xrtSlotMapIterEnd(xslotmapiter* pIterator);
```

迭代按零基槽索引递增，跳过空闲和退役槽。`IterNext` 返回非空对象指针，
`pSlot` 可选；遍历结束返回 `NULL` 且不设置新错误。

开始迭代后执行 `Insert`、`Remove` 或非空 `Clear`，下一次 `IterNext` 返回
`NULL` 并设置 `XERR_STATE`。`Set` 和 `Reserve` 不改变槽结构，允许继续迭代。

### `xrtSlotMapIterBegin`

启动按槽索引递增的外置迭代器。

```c
bool xrtSlotMapIterBegin(const xslotmap* pMap, xslotmapiter* pIterator)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标槽表 |
| `pIterator` | 输出 | 非空 | 接收迭代器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已启动 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[slot_map_tour](../../examples/containers/slot_map_tour/main.c) · 启动迭代

```c
	if ( !xrtSlotMapIterBegin(pMap, &Iter) ) {
```

### `xrtSlotMapIterNext`

返回下一个活动指针，并可返回与其匹配的稳定句柄。

```c
ptr xrtSlotMapIterNext(xslotmapiter* pIterator, xslot* pSlot)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIterator` | 输入/输出 | 已启动 | 目标迭代器 |
| `pSlot` | 输出 | 允许空 | 接收匹配句柄 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 活动指针借用；遍历结束为 `NULL` | — |
| `NULL` | 遍历结束或迭代器失效 | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 槽表结构在迭代期间被修改；遍历结束返回 `NULL` 不设错

#### 范例

[slot_map_tour](../../examples/containers/slot_map_tour/main.c) · 下一活动槽

```c
	while ( (pValue = xrtSlotMapIterNext(&Iter, &SlotA)) !=
		NULL ) {
```

### `xrtSlotMapIterEnd`

提前结束迭代并清除借用状态。

```c
void xrtSlotMapIterEnd(xslotmapiter* pIterator)
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

[slot_map_tour](../../examples/containers/slot_map_tour/main.c) · 结束迭代

```c
	xrtSlotMapIterEnd(&Iter);
```

## 示例

```c
xslotmap Connections;
connection* pConnection = create_connection();
xslot Handle;
ptr pRemoved = NULL;

if ( xrtSlotMapInit(&Connections) ) {
	Handle = xrtSlotMapInsert(&Connections, pConnection);
	if ( Handle == XRT_SLOT_INVALID ) {
		destroy_connection(pConnection);
	} else {
		connection* pCurrent = xrtSlotMapGet(&Connections, Handle);
		/* 使用 pCurrent。 */

		if ( xrtSlotMapRemove(&Connections, Handle, &pRemoved) ) {
			destroy_connection((connection*)pRemoved);
		}
	}
	xrtSlotMapUnit(&Connections);
}
```

同一索引稍后被复用时，新句柄的代际不同，旧 `Handle` 仍会被拒绝。
