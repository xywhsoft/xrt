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

### `xslotmapiter`

```c
typedef struct xslotmapiter {
	const xslotmap* Map;
	size_t Next;
	uint64 Version;
} xslotmapiter;
```

迭代器由 `IterBegin` 初始化并由 `IterEnd` 清理，不应手工修改字段。

## 句柄诊断

### `xrtSlotIndex`

```c
uint32 xrtSlotIndex(xslot Slot);
```

返回零基槽索引。无效句柄返回 `XRT_SLOT_INDEX_INVALID`，不设置错误。

### `xrtSlotGeneration`

```c
uint32 xrtSlotGeneration(xslot Slot);
```

返回非零代际。无效句柄返回零，不设置错误。

索引只适合日志和诊断。对象身份必须使用完整 `xslot`，不能只保存索引。

## 生命周期与容量

### `xrtSlotMapInit` / `xrtSlotMapCreate`

```c
bool xrtSlotMapInit(xslotmap* pMap);
xslotmap* xrtSlotMapCreate(void);
```

`Init` 初始化调用方持有的结构，成功后使用 `Unit`。`Create` 分配槽表结构，
成功后使用 `Destroy`。

### `xrtSlotMapUnit` / `xrtSlotMapDestroy`

```c
void xrtSlotMapUnit(xslotmap* pMap);
void xrtSlotMapDestroy(xslotmap* pMap);
```

两者允许空指针，只释放槽存储，不释放槽内对象。`Unit` 后结构归零。

### `xrtSlotMapClear`

```c
void xrtSlotMapClear(xslotmap* pMap);
```

删除全部活动槽，推进它们的代际并保留容量。调用前应先遍历和释放由调用方
拥有的对象。

### `xrtSlotMapReserve`

```c
bool xrtSlotMapReserve(xslotmap* pMap, size_t iCapacity);
```

保证内部存储至少可容纳指定数量的槽。最大请求为 `UINT32_MAX`；实际容量可能
因几何增长更大。预留容量不会创建槽、改变 `Count` 或使句柄和迭代器失效。

## 基本操作

### `xrtSlotMapInsert`

```c
xslot xrtSlotMapInsert(xslotmap* pMap, ptr pValue);
```

插入非空借用指针并返回句柄。优先 O(1) 复用空闲槽，没有空闲槽时在末尾建立
新槽。失败返回 `XRT_SLOT_INVALID`。

### `xrtSlotMapGet`

```c
ptr xrtSlotMapGet(const xslotmap* pMap, xslot Slot);
```

返回当前句柄对应的对象指针。零句柄、越界句柄、已删除句柄和旧代际句柄均
返回 `NULL` 并设置 `XERR_RANGE`。

### `xrtSlotMapContains`

```c
bool xrtSlotMapContains(const xslotmap* pMap, xslot Slot);
```

判断句柄是否活动。句柄无效只是查询结果，不设置新错误；槽表状态无效仍会
报告错误。

### `xrtSlotMapSet`

```c
bool xrtSlotMapSet(xslotmap* pMap, xslot Slot, ptr pValue);
```

用非空指针替换现有值。句柄、代际和结构版本不变。函数不释放旧对象；需要
旧值时先调用 `Get`。

### `xrtSlotMapRemove`

```c
bool xrtSlotMapRemove(xslotmap* pMap, xslot Slot, ptr* pValue);
```

删除有效槽并推进代际。`pValue` 可为空；非空时返回被移除的指针。输出地址
不得位于槽表内部存储区。失败时槽表和输出保持不变。

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
