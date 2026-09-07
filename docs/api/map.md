# Map 与 IntMap

`map.h` 提供两种边界明确的固定值映射：

- `Map`：任意二进制键、平均 `O(1)` 查找、按插入顺序遍历。
- `IntMap`：`int64` 键、`O(log n)` 查找、按整数键顺序遍历。

两者都复制键、内联保存固定大小值、保持已有值地址稳定，并提供值槽式底层 API、复制式便利 API、所有权释放器和外置迭代器。它们不内置锁；共享合同由后续同步容器或调用方选择。

## Map

### 启用与依赖

功能宏：

```c
#define XRT_FEATURE_MAP
```

依赖关系：

```text
map -> hash64 -> core
```

`Map` 不为每个对象创建尺寸类内存池。空映射没有桶数组，也不持有动态内存；第一次插入或 `Reserve` 才建立桶数组。每个条目只执行一次紧凑分配：

```text
[ bucket/order links ][ allocation/hash/key size ][ padding ][ inline value ][ copied key ][ zero byte ]
```

XRT 全局堆已经按小块尺寸类复用内存，因此这里不再重复建立每对象池。这样避免旧版 `Dict` 的树节点和键分别分配，也避免把完整变长池的固定元数据压到每个小映射上。

### 复杂度与顺序

- 查找、插入、删除平均为 `O(1)`，碰撞最坏为 `O(n)`。
- 桶负载上限为 75%，扩容使用二次幂桶数。
- 扩容和收缩只重建桶链，不移动条目，键和值地址保持稳定。
- 遍历顺序是首次插入顺序，与哈希值、架构和桶扩容无关。
- 删除后重新插入的键出现在顺序尾部。
- 替换等价键的值不会替换第一次保存的规范键字节。

### 常量与类型

#### `XRT_MAP_ALIGNMENT_DEFAULT`

默认值对齐，当前为 16 字节。

#### `XRT_MAP_BUCKETS_MIN`

非空桶数组的最小桶数，当前为 16。它是实现容量参数，不是第一次插入前的固定对象开销。

#### `xmaphash` / `xmapequal`

```c
typedef uint64 (*xmaphash)(xbytesview key, ptr user_data);
typedef bool (*xmapequal)(xbytesview left, xbytesview right, ptr user_data);
```

相等键必须产生相同哈希值。回调只借用键视图，不得释放或保存未复制的输入视图，也不得重入当前映射的任何 API。实现会在用户哈希器和相等器执行期间设置忙状态，重入调用以 `XERR_STATE` 拒绝；默认内部策略不进入这条额外门禁路径。默认策略使用稳定的 `xrtHash64` 和精确二进制比较。需要大小写不敏感、结构化键或抗碰撞键控哈希时，可以为空映射设置成对的自定义策略。

#### `xmapdrop`

```c
typedef void (*xmapdrop)(xbytesview key, ptr value, ptr user_data);
```

释放器处理值内部拥有的资源，不释放值槽和键。它在替换、`Remove`、`Clear`、`Unit` 和 `Destroy` 时调用；`Take` 与 `TakePtr` 不调用。释放回调不得重入当前映射的任何 API；实现以忙状态强制该合同，包括拒绝回调中的 `Destroy`，不会在清理尚未完成时释放映射结构。

#### `xmapinit`

```c
typedef bool (*xmapinit)(xbytesview key, ptr value, ptr user_data);
```

初始化器只用于缺失键的 `xrtMapGetOrInit`。它接收映射已经复制且末尾补零的规范键，以及已经
清零但尚未提交的值槽。返回 `false` 时必须自行清理已取得资源并设置错误；映射会释放临时条目
和预备桶，不调用正式值释放器。回调期间当前映射处于忙状态，任何 API 重入都以
`XERR_STATE` 拒绝。

#### `xmapvisitor`

```c
typedef bool (*xmapvisitor)(xbytesview key, ptr value, ptr user_data);
```

返回 `true` 继续，返回 `false` 停止。键是映射内部副本，值是可写值槽。访问回调可以查询当前映射和直接修改既有值槽，但不能插入、替换、删除、清空、调整容量、改变策略或结束生命周期；这些操作以 `XERR_STATE` 拒绝。嵌套 `Visit` 同样被拒绝。

#### `xmap`

可以放在栈上、嵌入对象或由 `xrtMapCreate` 创建。公开字段用于无隐藏对象分配和单头集成，不是直接修改接口：

| 字段组 | 含义 |
| --- | --- |
| `Buckets`、`BucketCount`、`Threshold` | 桶数组和 75% 键容量 |
| `First`、`Last` | 插入顺序链首尾 |
| `ValueSize`、`ValueOffset`、`KeyOffset`、`Alignment` | 紧凑条目布局 |
| `Count`、`Version` | 键数和结构版本 |
| `Hash`、`Equal`、`KeyUserData` | 键策略 |
| `Drop`、`DropUserData` | 值所有权策略 |
| `Flags` | 内部生命期状态 |

初始化后的结构可以整体移动，但移动期间不能存在借用的值、键或迭代器；最稳妥的做法仍是初始化后保持对象地址不变。

#### `xmapiter`

零分配外置迭代器。`Map`、下一条目、结构版本和方向都保存在迭代器自身，因此同一映射可以存在多个并行读迭代状态。

### 生命周期与容量

| API | 语义 |
| --- | --- |
| `xrtMapInit` | 默认对齐初始化空映射，不分配桶 |
| `xrtMapInitAligned` | 以二次幂对齐初始化空映射 |
| `xrtMapCreate` | 在堆上创建默认对齐映射 |
| `xrtMapCreateAligned` | 在堆上创建显式对齐映射 |
| `xrtMapSetKeyPolicy` | 为空映射设置键策略；两个空回调恢复默认策略 |
| `xrtMapSetDrop` | 为空映射设置值释放器 |
| `xrtMapClear` | 清空键值，保留桶数组复用 |
| `xrtMapReserve` | 预留指定键容量，避免热路径扩容 |
| `xrtMapTrim` | 收缩到当前键数所需的最小桶数组 |
| `xrtMapUnit` | 释放内容和桶，不释放结构 |
| `xrtMapDestroy` | 释放内容、桶和堆结构 |
| `xrtMapCount` | 当前键数 |
| `xrtMapCapacity` | 下次扩容前可容纳的键数 |

`Reserve` 和 `Trim` 不改变键集合、插入顺序或结构版本，因此不会移动键值，也不会使迭代器失效。分配失败时原桶数组保持不变。

### 键和值访问

#### `xrtMapGetOrAdd`

```c
ptr xrtMapGetOrAdd(xmap* map, xbytesview key, bool* is_new);
```

- 已有键直接返回稳定值槽，不分配，`is_new` 为 `false`。
- 缺失键复制键并创建清零值槽，`is_new` 为 `true`。
- 空键 `{ NULL, 0 }` 合法。
- 键可以包含任意零字节。
- 内部键副本额外补一个零字节，方便文本调用方，但 `Size` 才是键边界。
- 新键所需的桶和条目会在提交前全部取得；任一 OOM 都不改变映射。

#### `xrtMapGetOrInit`

```c
ptr xrtMapGetOrInit(
	xmap* map,
	xbytesview key,
	xmapinit init,
	ptr user_data,
	bool* is_new
);
```

已有键直接返回原值槽且不调用初始化器。缺失键先复制键、清零值槽并执行初始化器，只有回调
成功后才一次提交预备桶和条目。初始化失败、OOM 和回调拒绝都不会改变键集合。这是资源值、
类型容器和需要构造函数的底层扩展点；只需要零值时继续使用更短的 `GetOrAdd`。

#### `xrtMapSet`

```c
bool xrtMapSet(xmap* map, xbytesview key, const void* value);
```

复制完整固定值。缺失键会插入，已有等价键会先调用旧值释放器再替换。该 API 是字节复制语义；值内含拥有指针时，调用方应设置释放器并避免有意浅复制同一所有权到多个键。来源正是目标值槽时为无操作成功；来源区间触及映射结构、可能在扩容时释放的桶数组，或替换目标条目的任意部分时会在改变状态前拒绝。普通 `Set` 的别名检查保持 O(1)，不会扫描全部条目。

#### 查询 API

| API | 结果 |
| --- | --- |
| `xrtMapGet` | 可写值槽 |
| `xrtMapConstGet` | 只读值槽 |
| `xrtMapHas` | 键是否存在 |
| `xrtMapStoredKey` | 返回等价条目的内部规范键视图 |

缺失键是正常查询结果，不设置错误。`StoredKey` 使用布尔返回值，因此能区分“缺失键”和“已保存的空键”。返回的键和值都只借用到该键删除或映射清理之前。

### 删除与所有权

| API | 调用释放器 | 移交值字节 |
| --- | --- | --- |
| `xrtMapRemove` | 是 | 否 |
| `xrtMapTake` | 否 | 是 |
| `xrtMapTakePtr` | 否 | 是 |

`Take` 输出必须有至少 `ValueSize` 字节，且完整输出区间不能触及映射结构、桶数组或任何条目分配。缺失键返回 `false` 且不修改通用值输出。为了在公开结构和稳定值地址条件下拒绝跨条目的部分覆盖，`Take` 会扫描当前条目区间；要求严格 O(1) 且不需要移交值时应使用 `Remove`。

### 指针便利层

值大小为 `sizeof(ptr)` 时可以使用：

```c
bool xrtMapSetPtr(xmap* map, xbytesview key, ptr value);
ptr xrtMapGetPtr(xmap* map, xbytesview key);
bool xrtMapTakePtr(xmap* map, xbytesview key, ptr* value);
```

同一指针替换为无操作，不调用释放器。空指针值与缺失键使用 `xrtMapHas` 区分。值大小不匹配时报告 `XERR_STATE`。

### 遍历

`xrtMapVisit` 按插入顺序调用访问器。外置迭代器提供正反两个方向：

```c
xmapiter iterator;
xbytesview key;
myvalue* value;

if ( xrtMapIterBegin(&map, &iterator) ) {
	while ( (value = (myvalue*)xrtMapIterNext(&iterator, &key)) != NULL ) {
		/* 使用 key 和 value */
	}
	xrtMapIterEnd(&iterator);
}
```

- `xrtMapIterBegin` 按插入顺序。
- `xrtMapIterRBegin` 按插入顺序逆序。
- 修改值、替换已有值、`Reserve` 和 `Trim` 不使迭代器失效。
- 插入新键、删除键和清空映射会使已有迭代器失效；下一次 `IterNext` 报告 `XERR_STATE`。
- `Visit` 执行期间可以查询映射和修改回调取得的值槽；所有结构修改、容量修改和生命周期操作都被拒绝。

### 错误与线程合同

- 空映射指针、零值大小、非法键视图、空必需输出和非二次幂对齐报告 `XERR_ARGUMENT`。
- 布局、键长度和容量计算溢出报告 `XERR_RANGE`。
- 桶或条目分配失败报告 `XERR_MEMORY`，键集合保持不变。
- 非空映射修改键策略或释放器、错误值大小的指针便利 API 报告 `XERR_STATE`。
- 哈希、相等、释放回调期间的 API 重入，以及 `Visit` 期间的修改或嵌套访问报告 `XERR_STATE`。
- `Set` 来源或 `Take` 输出触及映射拥有内存时报告 `XERR_ARGUMENT`，操作前状态保持不变。
- 同一映射实例不支持无锁并发修改；只读并发也必须确保没有写入和生命周期结束。

跨线程组合使用的参考门禁位于 `tests/containers/test_container_external_sync.c`。测试把 Map 与其他基础容器放在同一个调用方 Mutex 下更新，避免每个容器重复携带锁和线程归属状态。

### 完整范例

可编译范例位于 `examples/containers/map/main.c`，演示字面量路径键、零初始化值槽和插入顺序遍历。

### 与旧版 Dict 的取舍

保留并强化：

- 任意二进制键和键副本。
- 固定大小内联值槽与指针便利路径。
- 旧版百万插入、千万查询所覆盖的大规模查找边界。
- 遍历、删除和独立键内存生命周期测试。

替换或退休：

- 用真正哈希桶替换“按哈希排序的 AVL 树”，平均查找从 `O(log n)` 变为 `O(1)`。
- 所有架构统一使用 64 位哈希，不再因 32/64 位改变内部顺序。
- 键和值与条目合并为一次分配，不再单独分配键。
- 迭代顺序从无业务意义的哈希树顺序改为稳定插入顺序。
- 退休 `Dict_Key`、`SetWithKey/GetWithKey` 和内部迭代宏，不让预计算哈希破坏键合同。
- 退休基础容器内的 owner/shared 锁分支；线程同步由上层明确选择。
- 调试分配位置统一由中央内存调试层处理，不再生成容器专属 Dbg API。

发布级百万/千万压力测试位于 `dev/bench/map/bench_map.c`。日常回归执行稳定地址、
20,000 键扩容和 200,000 次查询；发布基准恢复 1,000,000 次插入和
10,000,000 次命中查询，验证键数、每次查询值和最终校验和。当前同环境结果与使用边界
记录在 `dev/bench/map/MAP_BENCH_20260728.md`。

## IntMap

`IntMap` 是以 `int64` 为键、按键有序、值大小固定的稀疏映射。它继承旧版 `List` 已经验证的整数键体系，但使用准确的容器名称，并复用新版拥有式 AVL 树和固定对象池。

## 启用与依赖

功能宏：

```c
#define XRT_FEATURE_INT_MAP
```

`int_map` 依赖 `avl_tree`，因此完整依赖链为：

```text
int_map -> avl_tree -> avl + pool -> core
```

公共头文件会拒绝缺失 `XRT_FEATURE_AVL_TREE` 的非法裁剪组合。

## 存储模型

每个条目只占用一个固定池槽：

```text
[ AVL node ][ padding ][ int64 key ][ alignment padding ][ inline value ]
```

- 键支持 `INT64_MIN` 到 `INT64_MAX`。
- 键可以为负数、跳号或极度稀疏，没有最大索引造成的连续空间浪费。
- 值直接内联在树节点中，没有逐值堆分配。
- 值地址在该键被删除、`Clear` 或 `Unit` 前保持稳定。
- 遍历顺序是整数键升序或降序，不是插入顺序。
- 查找、插入和删除的复杂度是 `O(log n)`。

`IntMap` 不内置线程归属、锁或 shared 模式。这个基础容器的同一实例不能被多线程并发修改；需要共享时，由调用方或后续同步容器层选择互斥锁、读写锁或任务归属模型。

`tests/containers/test_container_external_sync.c` 覆盖 IntMap 在外部 Mutex 下的多线程写入、最终键数和组合生命周期。

## 类型与常量

### `XRT_INT_MAP_ALIGNMENT_DEFAULT`

默认值对齐，当前为 16 字节。

### `xintmap`

可以放在栈上、嵌入其他对象，或由 `xrtIntMapCreate` 在堆上创建。初始化后不应按字节复制或移动结构，因为内部 AVL 适配器保存了对映射结构的引用。

### `xintmapiter`

零堆分配的外置迭代器。同一映射可同时存在多个读迭代器。

### `xintmapdrop`

```c
typedef void (*xintmapdrop)(int64 key, ptr value, ptr user_data);
```

回调只释放值内部拥有的资源，不释放 `value` 值槽本身。它在以下路径调用：

- `Set` 替换已有值。
- `Remove` 删除值。
- `Clear`、`Unit` 和 `Destroy` 清理剩余值。

`Take` 和 `TakePtr` 是所有权移交路径，不调用释放器。释放回调执行期间，
同一映射的全部 API 都会以 `XERR_STATE` 拒绝；回调只应处理当前值内部的资源。

### `xintmapinit`

```c
typedef bool (*xintmapinit)(int64 key, ptr value, ptr user_data);
```

该回调用于 `xrtIntMapGetOrInit` 原位建立一个缺失键的值。回调失败时必须自行释放
已经取得的部分资源并设置错误；映射不会提交该条目，也不会再调用释放器。回调成功后，
如果底层提交因内部状态失败，映射会用已经配置的 `xintmapdrop` 回滚完整值。

### `xintmapvisitor`

```c
typedef bool (*xintmapvisitor)(int64 key, ptr value, ptr user_data);
```

返回 `true` 继续遍历，返回 `false` 停止。回调可以查询同一映射并直接修改当前值槽，
但不得插入、替换、删除、清空、裁剪、释放映射或嵌套调用 `Visit`。

### `xmap`

字节键映射使用哈希桶查找，并以独立条目保持键和值地址稳定。

```c
typedef struct xmap {
	xmapentry** Buckets;
	xmapentry* First;
	xmapentry* Last;
	size_t ValueSize;
	size_t ValueOffset;
	size_t KeyOffset;
	size_t Alignment;
	size_t Count;
	size_t BucketCount;
	size_t Threshold;
	uint64 Version;
	xmaphash Hash;
	xmapequal Equal;
	xmapdrop Drop;
	ptr KeyUserData;
	ptr DropUserData;
	uint32 Flags;
} xmap;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Buckets` | `xmapentry**` | Buckets |
| `First` | `xmapentry*` | First |
| `Last` | `xmapentry*` | Last |
| `ValueSize` | `size_t` | ValueSize |
| `ValueOffset` | `size_t` | ValueOffset |
| `KeyOffset` | `size_t` | KeyOffset |
| `Alignment` | `size_t` | Alignment |
| `Count` | `size_t` | Count |
| `BucketCount` | `size_t` | BucketCount |
| `Threshold` | `size_t` | Threshold |
| `Version` | `uint64` | Version |
| `Hash` | `xmaphash` | Hash |
| `Equal` | `xmapequal` | Equal |
| `Drop` | `xmapdrop` | Drop |
| `KeyUserData` | `ptr` | KeyUserData |
| `DropUserData` | `ptr` | DropUserData |
| `Flags` | `uint32` | Flags |

### `xmapiter`

外置迭代器允许同一映射存在多个独立遍历状态。

```c
typedef struct xmapiter {
	xmap* Map;
	xmapentry* Next;
	uint64 Version;
	int Direction;
} xmapiter;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Map` | `xmap*` | Map |
| `Next` | `xmapentry*` | Next |
| `Version` | `uint64` | Version |
| `Direction` | `int` | Direction |

### `xmapentry`

Map 内部条目结构（不透明，仅实现内部使用）。


```c
typedef struct xmapentry xmapentry;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xmaphash`

键哈希器必须保证相等键产生相同哈希值，且不得重入当前映射。

```c
typedef uint64 (*xmaphash)(xbytesview Key, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xmapequal`

键相等器只比较键内容，不取得所有权，也不得重入当前映射。

```c
typedef bool (*xmapequal)(xbytesview Left, xbytesview Right, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xmapdrop`

映射释放器处理值内部的拥有资源，不释放值槽、键或重入当前映射。

```c
typedef void (*xmapdrop)(xbytesview Key, ptr pValue, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xmapinit`

新值初始化器失败时自行清理部分状态并设置错误。

```c
typedef bool (*xmapinit)(xbytesview Key, ptr pValue, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xmapvisitor`

访问器返回 false 时停止遍历；回调内允许查询，不允许结构修改。

```c
typedef bool (*xmapvisitor)(xbytesview Key, ptr pValue, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

## 生命周期

| API | 语义 |
| --- | --- |
| `xrtIntMapInit` | 以默认对齐初始化自维护结构 |
| `xrtIntMapInitAligned` | 以显式值对齐初始化 |
| `xrtIntMapCreate` | 堆创建默认对齐映射 |
| `xrtIntMapCreateAligned` | 堆创建显式对齐映射 |
| `xrtIntMapSetDrop` | 为空映射设置释放器和用户数据 |
| `xrtIntMapClear` | 释放全部值，保留池页复用 |
| `xrtIntMapTrim` | 释放空闲池页，可保留指定数量 |
| `xrtIntMapUnit` | 释放值和池页，不释放结构 |
| `xrtIntMapDestroy` | 释放值、池页和堆结构 |
| `xrtIntMapCount` | 返回当前键值数量 |

`SetDrop` 只能在映射为空时调用，避免已有值的所有权合同在中途改变。

## 值访问

### `xrtIntMapGetOrAdd`

```c
ptr xrtIntMapGetOrAdd(xintmap* map, int64 key, bool* is_new);
```

这是值槽式的基础接口：

- 键存在时返回原值槽，`is_new` 为 `false`。
- 键缺失时创建值槽，值的全部字节清零，`is_new` 为 `true`。
- 重复键在任何分配前命中，因此不会因后续 OOM 而无法访问已有值。

```c
sessionstate* state;
bool is_new;

state = (sessionstate*)xrtIntMapGetOrAdd(&sessions, session_id, &is_new);
if ( state == NULL ) {
	return false;
}
state->Requests++;
```

### `xrtIntMapGetOrInit`

```c
ptr xrtIntMapGetOrInit(
	xintmap* map,
	int64 key,
	xintmapinit init,
	ptr user_data,
	bool* is_new
);
```

已有键直接返回稳定值槽且不调用 `init`。缺失键在未提交的池槽内写入键并调用
`init`；只有初始化成功后才插入 AVL 树。分配或初始化失败时，键数量、树根和已有值
均保持不变。该接口用于需要非平凡生命周期的上层容器，避免把清零字节误当成已初始化值。

### `xrtIntMapSet`

```c
bool xrtIntMapSet(xintmap* map, int64 key, const void* value);
```

复制一个完整值到映射。缺失键会插入，已有键会先调用释放器处理旧值再替换。来源
字节区间不得触及映射结构、池索引或任意池页，防止浅拷贝造成隐蔽的重复所有权和结构泄露。
来源正好是目标值槽时为无操作成功。

### `xrtIntMapGet` / `xrtIntMapConstGet`

返回可写或只读值槽。键不存在时返回空指针，这是正常查询结果，不设置错误。

### `xrtIntMapHas`

判断键是否存在。当值本身可以表达空指针时，使用该函数区分“缺失”和“已保存空值”。

## 删除与所有权

| API | 调用释放器 | 返回值字节 |
| --- | --- | --- |
| `xrtIntMapRemove` | 是 | 否 |
| `xrtIntMapTake` | 否 | 是 |
| `xrtIntMapTakePtr` | 否 | 是 |

`Take` 的输出缓冲必须至少有初始化时指定的值大小。键不存在时返回 `false`，不修改通用值输出。`TakePtr` 在失败时会将指针输出清空。

## 指针值便利接口

当映射以 `sizeof(ptr)` 初始化时，可使用：

```c
bool xrtIntMapSetPtr(xintmap* map, int64 key, ptr value);
ptr xrtIntMapGetPtr(xintmap* map, int64 key);
bool xrtIntMapTakePtr(xintmap* map, int64 key, ptr* value);
```

`SetPtr` 用相同指针替换已有值时为无操作，不会调用释放器。在非 `sizeof(ptr)` 值映射上调用这些函数会报告 `XERR_STATE`。

## 有序查询

| API | 结果 |
| --- | --- |
| `xrtIntMapFirst` | 最小键 |
| `xrtIntMapLast` | 最大键 |
| `xrtIntMapLowerBound` | 第一个 `actual_key >= key` 的条目 |
| `xrtIntMapUpperBound` | 第一个 `actual_key > key` 的条目 |

这些函数可选返回实际键。没有结果时返回空指针，并将键输出设为零。

## 遍历

### 访问器

```c
size_t xrtIntMapVisit(
	xintmap* map,
	xintmapvisitor visitor,
	ptr user_data
);
```

按键升序调用访问器，返回实际访问数量。访问期间实现复用拥有式 AVL 树的访问状态：
`Count`、`Get`、`Has`、首尾和边界查询仍可用；结构或生命周期修改会立即失败并报告
`XERR_STATE`，不会让外层遍历持有失效地址。`Visit` 必须独占同一映射实例，不能与其他线程的访问并发。

### 外置迭代器

```c
xintmapiter iterator;
int64 key;
myvalue* value;

if ( xrtIntMapIterBegin(&map, &iterator) ) {
	while ( (value = (myvalue*)xrtIntMapIterNext(&iterator, &key)) != NULL ) {
		/* 使用 key 和 value */
	}
	xrtIntMapIterEnd(&iterator);
}
```

- `xrtIntMapIterBegin` 升序。
- `xrtIntMapIterRBegin` 降序。
- `xrtIntMapIterFrom` 从第一个 `actual_key >= key` 的条目开始升序迭代。
- `xrtIntMapIterRFrom` 从第一个 `actual_key <= key` 的条目开始降序迭代。
- `xrtIntMapIterNext` 返回下一值槽和键。
- `xrtIntMapIterEnd` 允许提前结束。

修改值槽内容不会使迭代器失效。插入新键、删除键或清空映射会修改 AVL 结构版本；下一次 `IterNext` 返回空指针并报告 `XERR_STATE`。

## 错误与边界

- 空映射指针、零值大小、非 2 的幂对齐、空必需输出会报告 `XERR_ARGUMENT`。
- 条目布局加法溢出会报告 `XERR_RANGE`。
- 池页分配失败会报告 `XERR_MEMORY`，映射保持不变。
- 释放器中的 API 重入，以及访问器中的结构或生命周期修改，会报告 `XERR_STATE`。
- 不存在的键是正常结果，不会设置错误。
- 重复键 `GetOrAdd` 不分配。
- `Count` 使用 `size_t`，不保留旧版 32 位计数上限。

## 完整范例

可编译范例位于 `examples/containers/int_map/main.c`。范例演示稀疏会话 ID、零初始化值槽和升序迭代。

## 与旧版 List 的取舍

保留：

- `int64` 全范围稀疏键。
- AVL 有序查找和升序遍历。
- 固定大小内联值槽。
- 指针值友好路径。
- 旧版百万级插入和千万级查找所覆盖的平衡树与池复用路径。

改进：

- 使用 `IntMap` 准确表达“整数到值”，不再占用惯例上的 `List` 名称。
- 外置迭代器支持多个并行迭代状态，不把状态藏在容器内。
- 增加降序遍历、上下界查询、只读查询、释放回调、`Take` 所有权移交、显式对齐和池页裁剪。
- 去掉容器内部 owner/shared 分支和锁对象，使基础容器的成本、依赖和线程合同更清晰。

旧版的百万插入和千万查找不放入每次单元回归，它们作为性能基准资产保留；日常回归使用更快的稳定地址、OOM、顺序和所有权边界测试。

## API

### 字节键 Map

### `xrtMapInit`

使用默认 16 字节值对齐初始化空字节键映射。

```c
bool xrtMapInit(xmap* pMap, size_t iValueSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输出 | 非空 | 接收空映射 |
| `iValueSize` | 输入 | `> 0` | 每值字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已就绪 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法

#### 范例

[containers/map · 生命周期](../../examples/containers/map/main.c) · 观察

```c
	if ( !xrtMapInit(&tRoutes, sizeof(routestat)) ) {
```


### `xrtMapInitAligned`

使用显式值对齐初始化空字节键映射。

```c
bool xrtMapInitAligned(
	xmap* pMap,
	size_t iValueSize,
	size_t iAlignment
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输出 | 非空 | 接收空映射 |
| `iValueSize` | 输入 | `> 0` | 每值字节数 |
| `iAlignment` | 输入 | 非零二次幂 | 值对齐 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已就绪 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法

#### 范例

[containers/map_tour · 生命周期](../../examples/containers/map_tour/main.c) · 观察

```c
		if ( !xrtMapInitAligned(&Aligned, sizeof(uint64), 64u) ) {
```


### `xrtMapCreate`

创建使用默认 16 字节值对齐的空字节键映射。

```c
xmap* xrtMapCreate(size_t iValueSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iValueSize` | 输入 | `> 0` | 每值字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 堆映射；`Destroy` 释放 | — |
| `NULL` | 参数非法或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- `XERR_MEMORY` — 分配失败

#### 范例

[containers/map_tour · 生命周期](../../examples/containers/map_tour/main.c) · 观察

```c
	xmap* pMap = xrtMapCreate(sizeof(ptr));
```


### `xrtMapCreateAligned`

创建使用显式值对齐的空字节键映射。

```c
xmap* xrtMapCreateAligned(size_t iValueSize, size_t iAlignment);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iValueSize` | 输入 | `> 0` | 每值字节数 |
| `iAlignment` | 输入 | 非零二次幂 | 值对齐 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 堆映射 | — |
| `NULL` | 参数非法或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- `XERR_MEMORY` — 分配失败

#### 范例

[containers/map_tour · 生命周期](../../examples/containers/map_tour/main.c) · 观察

```c
		xmap* pAlignedHeap = xrtMapCreateAligned(sizeof(uint64), 64u);
```


### `xrtMapSetKeyPolicy`

为仍为空的映射设置成对的自定义哈希器和相等器；两个空指针恢复默认策略。

```c
bool xrtMapSetKeyPolicy(
	xmap* pMap,
	xmaphash pHash,
	xmapequal pEqual,
	ptr pUserData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空、空映射 | 目标映射 |
| `pHash` | 输入 | 允许空（成对） | 哈希器 |
| `pEqual` | 输入 | 允许空（成对） | 相等器 |
| `pUserData` | 输入 | 任意值 | 原样传给二者 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 策略已安装 | — |
| `false` | 映射非空或参数非法 | — |

#### 错误

- `XERR_STATE` — 映射非空（策略/释放器只能在空映射上设置）或迭代期间结构被修改
- `XERR_ARGUMENT` — 指针为空或大小/对齐非法

#### 范例

[containers/map_tour · 生命周期](../../examples/containers/map_tour/main.c) · 观察

```c
		if ( !xrtMapSetKeyPolicy(&Aligned, NULL, NULL, NULL) ) {
```


### `xrtMapSetDrop`

为仍为空的映射设置值资源释放器和独立用户数据。

```c
bool xrtMapSetDrop(xmap* pMap, xmapdrop pDrop, ptr pUserData);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空、空映射 | 目标映射 |
| `pDrop` | 输入 | 非空 | 值释放器 |
| `pUserData` | 输入 | 任意值 | 原样传给释放器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 释放器已安装 | — |
| `false` | 映射非空 | — |

#### 错误

- `XERR_STATE` — 映射非空（策略/释放器只能在空映射上设置）或迭代期间结构被修改

#### 范例

[containers/map_tour · 生命周期](../../examples/containers/map_tour/main.c) · 观察

```c
		, xrtMapSetDrop(&Aligned, NULL, NULL) ? 1 : 0);
```


### `xrtMapUnit`

释放全部键值和桶数组，但不释放映射结构（内嵌形态收尾）。

```c
void xrtMapUnit(xmap* pMap);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 调用全部值释放器后释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[containers/map · 生命周期](../../examples/containers/map/main.c) · 观察

```c
		xrtMapUnit(&tRoutes);
```


### `xrtMapDestroy`

释放全部键值、桶数组和映射结构。

```c
void xrtMapDestroy(xmap* pMap);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 全部资源已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[containers/map_tour · 生命周期](../../examples/containers/map_tour/main.c) · 观察

```c
			xrtMapDestroy(pMap);
```


### `xrtMapClear`

清空全部键值并保留桶数组供复用。

```c
void xrtMapClear(xmap* pMap);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | Count 归零；容量保留 | — |

#### 错误

- 无 — 清空不失败

#### 范例

[containers/map_tour · 编辑](../../examples/containers/map_tour/main.c) · 观察

```c
	xrtMapClear(pMap);
```


### `xrtMapReserve`

确保映射无需扩容即可容纳指定数量的键。

```c
bool xrtMapReserve(xmap* pMap, size_t iCapacity);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标映射 |
| `iCapacity` | 输入 | — | 最低容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 容量已保证 | — |
| `false` | 参数或 OOM | 映射不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- `XERR_MEMORY` — 分配失败

#### 范例

[containers/map_tour · 编辑](../../examples/containers/map_tour/main.c) · 观察

```c
		(void)xrtMapReserve(pMap, 8u);
```


### `xrtMapTrim`

把桶数组收缩到当前键数所需的最小容量。

```c
bool xrtMapTrim(xmap* pMap);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标映射 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已收缩 | — |
| `false` | 参数或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- `XERR_MEMORY` — 分配失败

#### 范例

[containers/map_tour · 编辑](../../examples/containers/map_tour/main.c) · 观察

```c
	(void)xrtMapTrim(pMap);
```


### `xrtMapCount`

返回当前键值数量，非法映射返回零。

```c
size_t xrtMapCount(const xmap* pMap);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 允许空 | 目标映射 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 键值数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法

#### 范例

[containers/map_tour · 查询](../../examples/containers/map_tour/main.c) · 观察

```c
	printf("count=%zu cap>=%zu ", xrtMapCount(pMap), xrtMapCapacity(pMap));
```


### `xrtMapCapacity`

返回当前桶数组在再次扩容前可容纳的键数。

```c
size_t xrtMapCapacity(const xmap* pMap);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 允许空 | 目标映射 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 容量 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法

#### 范例

[containers/map_tour · 查询](../../examples/containers/map_tour/main.c) · 观察

```c
	printf("count=%zu cap>=%zu ", xrtMapCount(pMap), xrtMapCapacity(pMap));
```


### `xrtMapGetOrAdd`

返回已有值槽，或复制键并原地创建、清零一个新值槽。

```c
ptr xrtMapGetOrAdd(xmap* pMap, xbytesview Key, bool* pNew);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标映射 |
| `Key` | 输入 | 借用 | 字节键（成功后内部持副本） |
| `pNew` | 输出 | 可空 | 是否新插入 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 值槽（新槽已清零） | — |
| `NULL` | 参数非法或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- `XERR_MEMORY` — 分配失败

#### 范例

[containers/map · 编辑](../../examples/containers/map/main.c) · 观察

```c
	pStat = (routestat*)xrtMapGetOrAdd(&tRoutes, XRT_BYTES_LITERAL("/health"), &bNew);
```


### `xrtMapGetOrInit`

返回已有值槽，或失败原子地复制键并原位初始化新值。

```c
ptr xrtMapGetOrInit(
	xmap* pMap,
	xbytesview Key,
	xmapinit pInit,
	ptr pUserData,
	bool* pNew
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标映射 |
| `Key` | 输入 | 借用 | 字节键 |
| `pInit` | 输入 | 非空 | 原位初始化回调 |
| `pUserData` | 输入 | 任意值 | 传给回调 |
| `pNew` | 输出 | 可空 | 是否新插入 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 值槽 | — |
| `NULL` | 回调失败（原子回滚）或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- `XERR_MEMORY` — 分配失败
- 回调失败 — 回滚新条目，映射不变

#### 范例

[containers/map_tour · 编辑](../../examples/containers/map_tour/main.c) · 观察

```c
		pSlot = xrtMapGetOrInit(pMap, SV("c"), initSlot, NULL, &bNew);
```


### `xrtMapSet`

复制插入或替换值；来源不得触及映射元数据或目标条目。

```c
bool xrtMapSet(xmap* pMap, xbytesview Key, const void* pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标映射 |
| `Key` | 输入 | 借用 | 字节键 |
| `pValue` | 输入 | 借用、独立 | 值来源 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已插入/替换（旧值先过释放器） | — |
| `false` | 参数或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- `XERR_MEMORY` — 分配失败

#### 范例

[containers/map_tour · 编辑](../../examples/containers/map_tour/main.c) · 观察

```c
		if ( !xrtMapSet(pMap, SV("a"), &iZero) ) {
```


### `xrtMapGet`

返回指定键的可写值槽，未找到是正常结果。

```c
ptr xrtMapGet(xmap* pMap, xbytesview Key);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `Key` | 输入 | 借用 | 字节键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 可写值槽 | — |
| `NULL` | 未找到（正常结果） | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/map_tour · 查询](../../examples/containers/map_tour/main.c) · 观察

```c
		, xrtMapGet(pMap, SV("b")) != NULL ? 1 : 0);
```


### `xrtMapConstGet`

返回指定键的只读值槽，未找到是正常结果。

```c
const void* xrtMapConstGet(const xmap* pMap, xbytesview Key);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `Key` | 输入 | 借用 | 字节键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 只读值槽 | — |
| `NULL` | 未找到 | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/map_tour · 查询](../../examples/containers/map_tour/main.c) · 观察

```c
		xrtMapConstGet(pMap, SV("b")) != NULL ? 1 : 0);
```


### `xrtMapHas`

判断指定字节键是否存在。

```c
bool xrtMapHas(const xmap* pMap, xbytesview Key);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `Key` | 输入 | 借用 | 字节键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 存在 | — |
| `false` | 不存在 | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/map_tour · 查询](../../examples/containers/map_tour/main.c) · 观察

```c
			xrtMapHas(pMap, SV("a")) ? 1 : 0);
```


### `xrtMapStoredKey`

返回与查询键等价的内部键副本，缺失时清空输出。

```c
bool xrtMapStoredKey(
	const xmap* pMap,
	xbytesview Key,
	xbytesview* pStoredKey
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `Key` | 输入 | 借用 | 查询键 |
| `pStoredKey` | 输出 | 非空 | 接收内部键视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 视图已发布（存活到键被删除） | — |
| `false` | 未找到 | `*pStoredKey` 清空 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/map_tour · 查询](../../examples/containers/map_tour/main.c) · 观察

```c
	if ( xrtMapStoredKey(pMap, SV("a"), &Key) ) {
```


### `xrtMapRemove`

删除指定键并调用值释放器。

```c
bool xrtMapRemove(xmap* pMap, xbytesview Key);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标映射 |
| `Key` | 输入 | 借用 | 要删除的键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已删除 | — |
| `false` | 未找到（正常结果） | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/map_tour · 编辑](../../examples/containers/map_tour/main.c) · 观察

```c
		, xrtMapRemove(pMap, SV("c")) ? 1 : 0);
```


### `xrtMapTake`

将值移交给映射外缓冲后删除；输出不得触及映射拥有的内存。

```c
bool xrtMapTake(xmap* pMap, xbytesview Key, ptr pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标映射 |
| `Key` | 输入 | 借用 | 键 |
| `pValue` | 输出 | 非空、独立 | 接收值字节 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 值已移交且条目已删除（不调释放器） | — |
| `false` | 未找到 | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/map_tour · 编辑](../../examples/containers/map_tour/main.c) · 观察

```c
			(void)xrtMapTake(&Aligned, SV("k"), &iValue);
```


### `xrtMapSetPtr`

对 `sizeof(ptr)` 值映射执行指针类型友好的插入或替换。

```c
bool xrtMapSetPtr(xmap* pMap, xbytesview Key, ptr pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 值大小为 `sizeof(ptr)` 的映射 |
| `Key` | 输入 | 借用 | 字节键 |
| `pValue` | 输入 | — | 指针值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已插入/替换 | — |
| `false` | 参数或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- `XERR_MEMORY` — 分配失败

#### 范例

[containers/map_tour · 编辑](../../examples/containers/map_tour/main.c) · 观察

```c
	(void)xrtMapSetPtr(pMap, SV("a"), (ptr)0x10u);
```


### `xrtMapGetPtr`

返回 `sizeof(ptr)` 值映射中保存的指针；空值与缺失键用 `Has` 区分。

```c
ptr xrtMapGetPtr(xmap* pMap, xbytesview Key);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `Key` | 输入 | 借用 | 字节键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空或空 | 保存的指针（可为 NULL 值） | — |
| 区分缺失 | 用 `xrtMapHas` | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/map_tour · 查询](../../examples/containers/map_tour/main.c) · 观察

```c
		xrtMapGetPtr(pMap, SV("a")) == (ptr)0x10u ? "0x10" : "?");
```


### `xrtMapTakePtr`

从 `sizeof(ptr)` 值映射中移交指针，不调用值释放器。

```c
bool xrtMapTakePtr(xmap* pMap, xbytesview Key, ptr* pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标映射 |
| `Key` | 输入 | 借用 | 键 |
| `pValue` | 输出 | 非空、独立 | 接收指针 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 指针已移交且条目已删除 | — |
| `false` | 未找到 | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/map_tour · 编辑](../../examples/containers/map_tour/main.c) · 观察

```c
		bool bTook = xrtMapTakePtr(pMap, SV("a"), &pTaken);
```


### `xrtMapVisit`

按插入顺序访问键值，并返回实际访问数量。

```c
size_t xrtMapVisit(xmap* pMap, xmapvisitor pVisitor, ptr pUserData);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `pVisitor` | 输入 | 非空 | 访问器 |
| `pUserData` | 输入 | 任意值 | 传给访问器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 实际访问数（提前停止时小于总数） | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法

#### 范例

[containers/map_tour · 迭代](../../examples/containers/map_tour/main.c) · 观察

```c
	(void)xrtMapVisit(pMap, visitPair, &iVisited);
```


### `xrtMapIterBegin`

启动按插入顺序的外置迭代器。

```c
bool xrtMapIterBegin(xmap* pMap, xmapiter* pIterator);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射（空映射可启动） |
| `pIterator` | 输出 | 非空 | 迭代器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已就绪 | — |
| `false` | 参数错误 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法

#### 范例

[containers/map · 迭代](../../examples/containers/map/main.c) · 观察

```c
	if ( !xrtMapIterBegin(&tRoutes, &tIterator) ) {
```


### `xrtMapIterRBegin`

启动按插入顺序逆序遍历的外置迭代器。

```c
bool xrtMapIterRBegin(xmap* pMap, xmapiter* pIterator);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `pIterator` | 输出 | 非空 | 迭代器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已就绪 | — |
| `false` | 参数错误 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法

#### 范例

[containers/map_tour · 迭代](../../examples/containers/map_tour/main.c) · 观察

```c
	(void)xrtMapIterRBegin(pMap, &Iter);
```


### `xrtMapIterNext`

返回下一值槽并可选返回内部键视图；结构修改后报告状态错误。

```c
ptr xrtMapIterNext(xmapiter* pIterator, xbytesview* pKey);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIterator` | 输入/输出 | 非空、活动 | 迭代器 |
| `pKey` | 输出 | 可空 | 接收内部键视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 下一值槽 | — |
| `NULL` | 自然耗尽（不设错）或结构被修改 | 结构修改时 `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- `XERR_STATE` — 迭代期间结构被修改

#### 范例

[containers/map · 迭代](../../examples/containers/map/main.c) · 观察

```c
	while ( (pStat = (routestat*)xrtMapIterNext(&tIterator, &Path)) != NULL ) {
```


### `xrtMapIterEnd`

提前结束迭代并清除借用状态。

```c
void xrtMapIterEnd(xmapiter* pIterator);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIterator` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯清理 | — |

#### 错误

- 无 — 清理不失败

#### 范例

[containers/map · 迭代](../../examples/containers/map/main.c) · 观察

```c
	xrtMapIterEnd(&tIterator);
```


### `xrtIntMapInit`

使用默认 16 字节值对齐初始化空整数映射（按键有序存储）。

```c
bool xrtIntMapInit(xintmap* pMap, size_t iValueSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输出 | 非空 | 接收空映射 |
| `iValueSize` | 输入 | `> 0` | 每值字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已就绪 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法

#### 范例

[containers/int_map · 生命周期](../../examples/containers/int_map/main.c) · 观察

```c
	if ( !xrtIntMapInit(&tSessions, sizeof(sessionstate)) ) {
```



### 整数键 IntMap

### `xrtIntMapInitAligned`

使用显式值对齐初始化空整数映射。

```c
bool xrtIntMapInitAligned(
	xintmap* pMap,
	size_t iValueSize,
	size_t iAlignment
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输出 | 非空 | 接收空映射 |
| `iValueSize` | 输入 | `> 0` | 每值字节数 |
| `iAlignment` | 输入 | 非零二次幂 | 值对齐 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已就绪 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法

#### 范例

[containers/int_map_tour · 生命周期](../../examples/containers/int_map_tour/main.c) · 观察

```c
		if ( !xrtIntMapInitAligned(&Aligned, sizeof(ptr), 64u) ) {
```


### `xrtIntMapCreate`

创建使用默认 16 字节值对齐的空整数映射。

```c
xintmap* xrtIntMapCreate(size_t iValueSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iValueSize` | 输入 | `> 0` | 每值字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 堆映射；`Destroy` 释放 | — |
| `NULL` | 参数非法或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- `XERR_MEMORY` — 分配失败

#### 范例

[containers/int_map_tour · 生命周期](../../examples/containers/int_map_tour/main.c) · 观察

```c
	xintmap* pMap = xrtIntMapCreate(sizeof(int64));
```


### `xrtIntMapCreateAligned`

创建使用显式值对齐的空整数映射。

```c
xintmap* xrtIntMapCreateAligned(size_t iValueSize, size_t iAlignment);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iValueSize` | 输入 | `> 0` | 每值字节数 |
| `iAlignment` | 输入 | 非零二次幂 | 值对齐 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 堆映射 | — |
| `NULL` | 参数非法或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- `XERR_MEMORY` — 分配失败

#### 范例

[containers/int_map_tour · 生命周期](../../examples/containers/int_map_tour/main.c) · 观察

```c
		xintmap* pHeap = xrtIntMapCreateAligned(sizeof(ptr), 64u);
```


### `xrtIntMapSetDrop`

为仍为空的映射设置值资源释放器和用户数据。

```c
bool xrtIntMapSetDrop(
	xintmap* pMap,
	xintmapdrop pDrop,
	ptr pUserData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空、空映射 | 目标映射 |
| `pDrop` | 输入 | 非空 | 值释放器 |
| `pUserData` | 输入 | 任意值 | 传给释放器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已安装 | — |
| `false` | 映射非空 | — |

#### 错误

- `XERR_STATE` — 映射非空（策略/释放器只能在空映射上设置）或迭代期间结构被修改

#### 范例

[containers/int_map_tour · 生命周期](../../examples/containers/int_map_tour/main.c) · 观察

```c
		printf("setdrop=%d\n", xrtIntMapSetDrop(&Aligned, NULL, NULL) ? 1 : 0);
```


### `xrtIntMapUnit`

释放全部值和池页，但不释放映射结构。

```c
void xrtIntMapUnit(xintmap* pMap);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 值释放器先执行 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[containers/int_map · 生命周期](../../examples/containers/int_map/main.c) · 观察

```c
		xrtIntMapUnit(&tSessions);
```


### `xrtIntMapDestroy`

释放全部值、池页和映射结构。

```c
void xrtIntMapDestroy(xintmap* pMap);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 全部资源已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[containers/int_map_tour · 生命周期](../../examples/containers/int_map_tour/main.c) · 观察

```c
			xrtIntMapDestroy(pMap);
```


### `xrtIntMapClear`

清空全部值并保留固定池的复用能力。

```c
void xrtIntMapClear(xintmap* pMap);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | Count 归零；池页保留 | — |

#### 错误

- 无 — 清空不失败

#### 范例

[containers/int_map_tour · 编辑](../../examples/containers/int_map_tour/main.c) · 观察

```c
	xrtIntMapClear(pMap);
```


### `xrtIntMapTrim`

释放空闲池页，并返回实际释放的页数。

```c
size_t xrtIntMapTrim(xintmap* pMap, size_t iRetainEmpty);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标映射 |
| `iRetainEmpty` | 输入 | — | 至少保留的空页数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 实际释放页数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法

#### 范例

[containers/int_map_tour · 编辑](../../examples/containers/int_map_tour/main.c) · 观察

```c
	(void)xrtIntMapTrim(pMap, 0);
```


### `xrtIntMapCount`

返回当前键值数量，非法映射返回零。

```c
size_t xrtIntMapCount(const xintmap* pMap);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 允许空 | 目标映射 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 键值数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法

#### 范例

[containers/int_map_tour · 查询](../../examples/containers/int_map_tour/main.c) · 观察

```c
		xrtIntMapCount(pMap));
```


### `xrtIntMapGetOrAdd`

返回已有值槽，或原地创建并清零一个新值槽。

```c
ptr xrtIntMapGetOrAdd(xintmap* pMap, int64 iKey, bool* pNew);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标映射 |
| `iKey` | 输入 | — | 整数键 |
| `pNew` | 输出 | 可空 | 是否新插入 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 值槽（新槽已清零） | — |
| `NULL` | 参数非法或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- `XERR_MEMORY` — 分配失败

#### 范例

[containers/int_map · 编辑](../../examples/containers/int_map/main.c) · 观察

```c
	pState = (sessionstate*)xrtIntMapGetOrAdd(&tSessions, 1000001, &bNew);
```


### `xrtIntMapGetOrInit`

返回已有值槽，或失败原子地原位初始化一个新值。

```c
ptr xrtIntMapGetOrInit(
	xintmap* pMap,
	int64 iKey,
	xintmapinit pInit,
	ptr pUserData,
	bool* pNew
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标映射 |
| `iKey` | 输入 | — | 整数键 |
| `pInit` | 输入 | 非空 | 原位初始化回调 |
| `pUserData` | 输入 | 任意值 | 传给回调 |
| `pNew` | 输出 | 可空 | 是否新插入 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 值槽 | — |
| `NULL` | 回调失败（原子回滚）或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- `XERR_MEMORY` — 分配失败
- 回调失败 — 回滚新条目

#### 范例

[containers/int_map_tour · 编辑](../../examples/containers/int_map_tour/main.c) · 观察

```c
	pSlot = xrtIntMapGetOrInit(pMap, 60, initZero, NULL, NULL);
```


### `xrtIntMapSet`

复制插入或替换值；替换时先调用旧值释放器。

```c
bool xrtIntMapSet(xintmap* pMap, int64 iKey, const void* pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标映射 |
| `iKey` | 输入 | — | 整数键 |
| `pValue` | 输入 | 借用、独立 | 值来源 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已插入/替换 | — |
| `false` | 参数或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- `XERR_MEMORY` — 分配失败

#### 范例

[containers/int_map_tour · 编辑](../../examples/containers/int_map_tour/main.c) · 观察

```c
		if ( !xrtIntMapSet(pMap, Keys[i], &iValue) ) {
```


### `xrtIntMapGet`

返回指定键的可写值槽，未找到是正常结果。

```c
ptr xrtIntMapGet(xintmap* pMap, int64 iKey);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `iKey` | 输入 | — | 整数键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 可写值槽 | — |
| `NULL` | 未找到 | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/int_map_tour · 查询](../../examples/containers/int_map_tour/main.c) · 观察

```c
	if ( (pSlot = xrtIntMapGet(pMap, 20)) != NULL ) {
```


### `xrtIntMapConstGet`

返回指定键的只读值槽，未找到是正常结果。

```c
const void* xrtIntMapConstGet(const xintmap* pMap, int64 iKey);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `iKey` | 输入 | — | 整数键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 只读值槽 | — |
| `NULL` | 未找到 | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/int_map_tour · 查询](../../examples/containers/int_map_tour/main.c) · 观察

```c
		(long long)*(const int64*)xrtIntMapConstGet(pMap, 10));
```


### `xrtIntMapHas`

判断指定整数键是否存在。

```c
bool xrtIntMapHas(const xintmap* pMap, int64 iKey);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `iKey` | 输入 | — | 整数键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 存在 | — |
| `false` | 不存在 | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/int_map_tour · 查询](../../examples/containers/int_map_tour/main.c) · 观察

```c
		pSlot != NULL ? 1 : 0, xrtIntMapHas(pMap, 60) ? 1 : 0,
```


### `xrtIntMapRemove`

删除指定键并调用值释放器。

```c
bool xrtIntMapRemove(xintmap* pMap, int64 iKey);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标映射 |
| `iKey` | 输入 | — | 要删除的键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已删除 | — |
| `false` | 未找到 | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/int_map_tour · 编辑](../../examples/containers/int_map_tour/main.c) · 观察

```c
	printf("removed=%d ", xrtIntMapRemove(pMap, 30) ? 1 : 0);
```


### `xrtIntMapTake`

将指定键的值字节移交给调用方后删除，不调用值释放器。

```c
bool xrtIntMapTake(xintmap* pMap, int64 iKey, ptr pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标映射 |
| `iKey` | 输入 | — | 键 |
| `pValue` | 输出 | 非空、独立 | 接收值字节 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 值已移交且条目已删除 | — |
| `false` | 未找到 | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/int_map_tour · 编辑](../../examples/containers/int_map_tour/main.c) · 观察

```c
	(void)xrtIntMapTake(pMap, 10, &iValue);
```


### `xrtIntMapSetPtr`

对 `sizeof(ptr)` 值映射执行指针类型友好的插入或替换。

```c
bool xrtIntMapSetPtr(xintmap* pMap, int64 iKey, ptr pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 值大小为 `sizeof(ptr)` 的映射 |
| `iKey` | 输入 | — | 整数键 |
| `pValue` | 输入 | — | 指针值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已插入/替换 | — |
| `false` | 参数或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- `XERR_MEMORY` — 分配失败

#### 范例

[containers/int_map_tour · 编辑](../../examples/containers/int_map_tour/main.c) · 观察

```c
	(void)xrtIntMapSetPtr(pMap, 50, (ptr)0xABu);
```


### `xrtIntMapGetPtr`

返回 `sizeof(ptr)` 值映射中保存的指针；空指针值与缺失键用 `Has` 区分。

```c
ptr xrtIntMapGetPtr(xintmap* pMap, int64 iKey);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `iKey` | 输入 | — | 整数键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空或空 | 保存的指针（可为 NULL 值） | — |
| 区分缺失 | 用 `xrtIntMapHas` | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/int_map_tour · 查询](../../examples/containers/int_map_tour/main.c) · 观察

```c
	printf("getptr=%p ", (void*)xrtIntMapGetPtr(pMap, 50));
```


### `xrtIntMapTakePtr`

从 `sizeof(ptr)` 值映射中移交指针，不调用值释放器。

```c
bool xrtIntMapTakePtr(xintmap* pMap, int64 iKey, ptr* pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入/输出 | 非空 | 目标映射 |
| `iKey` | 输入 | — | 键 |
| `pValue` | 输出 | 非空、独立 | 接收指针 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 指针已移交且条目已删除 | — |
| `false` | 未找到 | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/int_map_tour · 编辑](../../examples/containers/int_map_tour/main.c) · 观察

```c
			(void)xrtIntMapTakePtr(&Aligned, 1, &pTaken);
```


### `xrtIntMapFirst`

返回顺序第一项的值槽，并可选返回键。

```c
ptr xrtIntMapFirst(xintmap* pMap, int64* pKey);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `pKey` | 输出 | 可空 | 接收键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 首项值槽（键最小） | — |
| `NULL` | 空映射 | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/int_map_tour · 查询](../../examples/containers/int_map_tour/main.c) · 观察

```c
	(void)xrtIntMapFirst(pMap, &iKey);
```


### `xrtIntMapLast`

返回顺序最后一项的值槽，并可选返回键。

```c
ptr xrtIntMapLast(xintmap* pMap, int64* pKey);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `pKey` | 输出 | 可空 | 接收键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 末项值槽（键最大） | — |
| `NULL` | 空映射 | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/int_map_tour · 查询](../../examples/containers/int_map_tour/main.c) · 观察

```c
	(void)xrtIntMapLast(pMap, &iKey);
```


### `xrtIntMapLowerBound`

返回第一个不小于指定键的值槽和实际键。

```c
ptr xrtIntMapLowerBound(xintmap* pMap, int64 iKey, int64* pActualKey);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `iKey` | 输入 | — | 边界键 |
| `pActualKey` | 输出 | 可空 | 接收实际键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 首个 `>= iKey` 项 | — |
| `NULL` | 全部小于 iKey | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/int_map_tour · 查询](../../examples/containers/int_map_tour/main.c) · 观察

```c
	(void)xrtIntMapLowerBound(pMap, 25, &iKey);
```


### `xrtIntMapUpperBound`

返回第一个严格大于指定键的值槽和实际键。

```c
ptr xrtIntMapUpperBound(xintmap* pMap, int64 iKey, int64* pActualKey);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `iKey` | 输入 | — | 边界键 |
| `pActualKey` | 输出 | 可空 | 接收实际键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 首个 `> iKey` 项 | — |
| `NULL` | 无严格大于项 | 不设错 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- 未找到不是错误

#### 范例

[containers/int_map_tour · 查询](../../examples/containers/int_map_tour/main.c) · 观察

```c
	(void)xrtIntMapUpperBound(pMap, 25, &iKey);
```


### `xrtIntMapVisit`

按键升序访问值；回调期间查询可用，结构和生命周期修改被拒绝。

```c
size_t xrtIntMapVisit(
	xintmap* pMap,
	xintmapvisitor pVisitor,
	ptr pUserData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `pVisitor` | 输入 | 非空 | 访问器 |
| `pUserData` | 输入 | 任意值 | 传给访问器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 实际访问数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法

#### 范例

[containers/int_map_tour · 迭代](../../examples/containers/int_map_tour/main.c) · 观察

```c
	(void)xrtIntMapVisit(pMap, doubleValue, &iHit);
```


### `xrtIntMapIterBegin`

启动按键升序的外置迭代器。

```c
bool xrtIntMapIterBegin(xintmap* pMap, xintmapiter* pIterator);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `pIterator` | 输出 | 非空 | 迭代器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已就绪 | — |
| `false` | 参数错误 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法

#### 范例

[containers/int_map · 迭代](../../examples/containers/int_map/main.c) · 观察

```c
	if ( !xrtIntMapIterBegin(&tSessions, &tIterator) ) {
```


### `xrtIntMapIterRBegin`

启动按键降序的外置迭代器。

```c
bool xrtIntMapIterRBegin(xintmap* pMap, xintmapiter* pIterator);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `pIterator` | 输出 | 非空 | 迭代器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已就绪 | — |
| `false` | 参数错误 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法

#### 范例

[containers/int_map_tour · 迭代](../../examples/containers/int_map_tour/main.c) · 观察

```c
	(void)xrtIntMapIterRBegin(pMap, &Iter);
```


### `xrtIntMapIterFrom`

从第一个不小于指定键的项开始升序迭代。

```c
bool xrtIntMapIterFrom(
	xintmap* pMap,
	int64 iKey,
	xintmapiter* pIterator
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `iKey` | 输入 | — | 起点键 |
| `pIterator` | 输出 | 非空 | 迭代器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已就绪（O(log n) 起点） | — |
| `false` | 参数错误 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法

#### 范例

[containers/int_map_tour · 迭代](../../examples/containers/int_map_tour/main.c) · 观察

```c
	(void)xrtIntMapIterFrom(pMap, 20, &Iter);
```


### `xrtIntMapIterRFrom`

从第一个不大于指定键的项开始降序迭代。

```c
bool xrtIntMapIterRFrom(
	xintmap* pMap,
	int64 iKey,
	xintmapiter* pIterator
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMap` | 输入 | 非空 | 目标映射 |
| `iKey` | 输入 | — | 起点键 |
| `pIterator` | 输出 | 非空 | 迭代器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已就绪 | — |
| `false` | 参数错误 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法

#### 范例

[containers/int_map_tour · 迭代](../../examples/containers/int_map_tour/main.c) · 观察

```c
	(void)xrtIntMapIterRFrom(pMap, 30, &Iter);
```


### `xrtIntMapIterNext`

返回下一值槽并可选返回键；自然耗尽不设错。

```c
ptr xrtIntMapIterNext(xintmapiter* pIterator, int64* pKey);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIterator` | 输入/输出 | 非空、活动 | 迭代器 |
| `pKey` | 输出 | 可空 | 接收键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 下一值槽 | — |
| `NULL` | 耗尽或结构被修改 | 结构修改时 `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或大小/对齐非法
- `XERR_STATE` — 迭代期间结构被修改

#### 范例

[containers/int_map · 迭代](../../examples/containers/int_map/main.c) · 观察

```c
	while ( (pState = (sessionstate*)xrtIntMapIterNext(&tIterator, &iSessionId)) != NULL ) {
```


### `xrtIntMapIterEnd`

提前结束迭代并清除借用状态。

```c
void xrtIntMapIterEnd(xintmapiter* pIterator);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIterator` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯清理 | — |

#### 错误

- 无 — 清理不失败

#### 范例

[containers/int_map · 迭代](../../examples/containers/int_map/main.c) · 观察

```c
	xrtIntMapIterEnd(&tIterator);
```


