# AVL 与 AVLTree

`avl` 是不拥有节点内存的侵入式有序索引；`avl_tree` 在它上面增加固定对象池、按值复制和资源释放回调。两层使用同一套 AVL 平衡核心，但面向不同成本模型，调用方不需要为了使用底层能力而承担拥有型容器的分配和所有权规则。

## 类型与常量

### `xavlnode`

侵入式节点只保存平衡树链接，业务结构可将它嵌入任意位置。

```c
typedef struct xavlnode {
	struct xavlnode* Left;
	struct xavlnode* Right;
	uint8 Height;
} xavlnode;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Left` | `struct xavlnode*` | 左操作数 |
| `Right` | `struct xavlnode*` | 右操作数 |
| `Height` | `uint8` | Height |

### `xavl`

侵入式树不拥有节点内存，版本号用于检测遍历期结构修改。

```c
typedef struct xavl {
	xavlnode* Root;
	size_t Count;
	uint64 Version;
} xavl;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Root` | `xavlnode*` | Root |
| `Count` | `size_t` | 数量 |
| `Version` | `uint64` | 结构版本 |

### `xavliter`

外置迭代器允许同一棵树存在多个并行读迭代，不发生堆分配。

```c
typedef struct xavliter {
	const xavl* Tree;
	xavlnode* Path[XRT_AVL_HEIGHT_MAX];
	size_t Depth;
	uint64 Version;
	bool Reverse;
	bool Active;
} xavliter;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Tree` | `const xavl*` | Tree |
| `Depth` | `size_t` | Depth |
| `Version` | `uint64` | 结构版本 |
| `Reverse` | `bool` | Reverse |
| `Active` | `bool` | Active |

### `xavltree`

拥有式树使用固定对象池，节点地址在删除前保持稳定。

```c
typedef struct xavltree {
	xavl Base;
	xpool Pool;
	size_t ItemSize;
	size_t ItemOffset;
	size_t Alignment;
	xavltreecompare Compare;
	xavltreedrop Drop;
	ptr UserData;
	uint32 Flags;
} xavltree;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Base` | `xavl` | Base |
| `Pool` | `xpool` | Pool |
| `ItemSize` | `size_t` | 单元素字节数 |
| `ItemOffset` | `size_t` | 元素偏移 |
| `Alignment` | `size_t` | 对齐（二次幂） |
| `Compare` | `xavltreecompare` | Compare |
| `Drop` | `xavltreedrop` | Drop |
| `UserData` | `ptr` | 用户数据 |
| `Flags` | `uint32` | 标志位 |

### `xavltreeiter`

拥有式迭代器复用零分配侵入式路径栈。

```c
typedef struct xavltreeiter {
	xavltree* Tree;
	xavliter Base;
} xavltreeiter;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Tree` | `xavltree*` | Tree |
| `Base` | `xavliter` | Base |

### `xavlcompare`

比较器返回 key 与节点的顺序关系，并可通过用户数据恢复业务结构。

```c
typedef int (*xavlcompare)(const void* pKey, const xavlnode* pNode, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xavlvisitor`

访问器返回 false 时停止遍历。

```c
typedef bool (*xavlvisitor)(xavlnode* pNode, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xavltreecompare`

拥有式树比较器返回 key 与对象的顺序关系，不得重入同一棵树。

```c
typedef int (*xavltreecompare)(const void* pKey, const void* pItem, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xavltreedrop`

对象释放器只处理内部资源，回调期间不得调用同一棵树的 API。

```c
typedef void (*xavltreedrop)(ptr pItem, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xavltreevisitor`

访问器可查询树和修改非键字段，不得修改结构或生命周期。

```c
typedef bool (*xavltreevisitor)(ptr pItem, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

## 裁剪与依赖

| 能力 | 宏 | 依赖 |
|---|---|---|
| 侵入式 AVL | `XRT_FEATURE_AVL` | `core` |
| 拥有型 AVLTree | `XRT_FEATURE_AVL_TREE` | `avl`、`pool` |

只启用 `XRT_FEATURE_AVL` 时不会带入内存池。启用 `XRT_FEATURE_AVL_TREE` 必须同时启用 `XRT_FEATURE_AVL`、`XRT_FEATURE_POOL` 及其 `XRT_FEATURE_POOL_PAGE` 依赖；公共头会拒绝不完整的宏组合。

## 共同契约

- 比较器统一返回 `key` 与节点或对象的顺序关系：负数表示 key 更小，零表示相等，正数表示 key 更大。
- 比较器必须只观察键、节点或对象和用户数据，不得调用同一棵树的 API，也不得修改参与当前操作的树状态。
- 树中不允许存在比较结果相等的两个节点。重复添加返回已有节点或对象，不覆盖原值。
- 查找、边界查询和未命中的删除是正常结果，返回 `NULL` 或 `false`，不会主动清除调用前已有错误。
- 节点挂树期间不得修改比较器可见的排序键。需要修改键时，先删除，修改后再重新插入。
- 两层容器都不隐式加锁。普通查询和外置迭代可由调用方保证生命周期后并发只读；`Visit` 会维护访问状态，必须独占同一实例。任何并发写入都需要外部同步。
- 插入、删除和清空会推进结构版本，使已经开始的迭代器失效。失效后的 `IterNext` 返回 `NULL` 并设置 `XERR_STATE`。
- 查找、插入、删除和边界查询平均及最坏时间复杂度均为 O(log n)，首尾查询和迭代每项摊销为 O(1)。

`tests/containers/test_container_external_sync.c` 验证拥有式 AVLTree 与其他基础容器共用调用方 Mutex 时的多线程写入和最终结构完整性。

## 侵入式 AVL

### 类型

```c
typedef struct xavlnode {
	struct xavlnode* Left;
	struct xavlnode* Right;
	uint8 Height;
} xavlnode;

typedef struct xavl {
	xavlnode* Root;
	size_t Count;
	uint64 Version;
} xavl;
```

业务对象把 `xavlnode` 嵌入任意位置，通过 `offsetof` 或自己的容器宏恢复完整对象。树只借用节点，不分配、不复制、不释放业务对象。

`xavliter` 是外置迭代状态，内部保存最多 `XRT_AVL_HEIGHT_MAX` 个路径节点。同一棵树可以同时存在多个迭代器，不再使用旧版树内单例迭代器，也不会在开始遍历时临时分配内存。

### 生命周期

### `xrtAVLNodeInit`

将节点恢复为未挂入任何树的状态。每个新节点第一次插入前必须调用一次。

```c
void xrtAVLNodeInit(xavlnode* pNode);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pNode` | 输入 | 建议非空 | 要重置的嵌入节点；空指针是空操作 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 纯重置，不失败 |

#### 范例

[containers/avl_tour · 侵入式](../../examples/containers/avl_tour/main.c) · 每次插入前逐节点初始化

```c
for ( i = 0; i < 5; ++i ) {
	xrtAVLNodeInit(&Items[i].Node);
	Items[i].Key = Keys[i];
	if ( xrtAVLInsert(&Tree, &Items[i].Node, &Keys[i],
		exampleCompareIntrusive, NULL, NULL) ==
		NULL ) {
		return 1;
	}
}
```

### `xrtAVLInit`

初始化一棵不拥有节点内存的空树。

```c
bool xrtAVLInit(xavl* pTree);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输出 | 非空 | 接收空树（`Root=NULL`、`Count=0`、`Version=0`） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 树已就绪 | — |
| `false` | `pTree` 为空 | `*pTree` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pTree` 为空

#### 范例

[containers/avl_tour · 侵入式](../../examples/containers/avl_tour/main.c) · 初始化后乱序插入制造旋转

```c
(void)xrtAVLInit(&Tree);
for ( i = 0; i < 5; ++i ) {
	xrtAVLNodeInit(&Items[i].Node);
```

### `xrtAVLClear`

忘记全部节点但不释放或逐个重置节点。适合外部对象整体失效或即将统一释放的场景；仍要复用旧节点时，调用方必须重新执行 `xrtAVLNodeInit()`。

```c
void xrtAVLClear(xavl* pTree);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 建议非空 | 目标树；清空后 `Count == 0` |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 只清根指针与计数，不触碰节点本身 |

#### 范例

[containers/avl_tour · 侵入式](../../examples/containers/avl_tour/main.c) · 清空后计数归零

```c
xrtAVLClear(&Tree);
if ( Tree.Count != 0u ) {
	return 14;
}
```

### 插入、删除与查找

### `xrtAVLInsert`

插入已初始化的独立节点；重复键返回已有节点并通过 `pNew` 返回 `false`。

```c
xavlnode* xrtAVLInsert(
	xavl* pTree,
	xavlnode* pNode,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData,
	bool* pNew
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入/输出 | 非空 | 目标树；成功插入后 `Count` 与 `Version` 前进 |
| `pNode` | 输入 | 已 `NodeInit` | 候选节点；重复键时完全不变 |
| `pKey` | 输入 | 借用 | 比较器理解的键表示；XRT 不解释 |
| `pCompare` | 输入 | 非空 | 比较器 |
| `pUserData` | 输入 | 任意值 | 原样传给比较器 |
| `pNew` | 输出 | 可空 | 接收"是否新插入"；重复时为 `false` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 树内节点 | 新插入时即 `pNode`；重复键时为已有节点 | — |
| `NULL` | 参数非法或节点不是独立状态 | 树与候选节点均不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 树/节点/键/比较器为空
- `XERR_STATE` — 候选节点仍挂在树上（非 `NodeInit` 后的独立状态）
- `XERR_RANGE` — 计数溢出（`size_t` 已满）

#### 范例

[containers/avl · 会话索引](../../examples/containers/avl/main.c) · 返回值用于检测重复键

```c
xrtAVLNodeInit(&pSessions[i].Index);
if (
	xrtAVLInsert(
```

### `xrtAVLRemove`

删除指定键并返回已经恢复为独立状态的原节点。

```c
xavlnode* xrtAVLRemove(
	xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入/输出 | 非空 | 目标树 |
| `pKey` | 输入 | 借用 | 要删除的键 |
| `pCompare` | 输入 | 非空 | 比较器 |
| `pUserData` | 输入 | 任意值 | 原样传给比较器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 原节点 | 调用方当初插入的节点，已恢复独立状态，可再次插入 | — |
| `NULL` | 未找到（正常结果）或参数非法 | 未找到不设置错误；参数错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 树/键/比较器为空

#### 范例

[containers/avl_tour · 侵入式](../../examples/containers/avl_tour/main.c) · 删除后 Find 不再命中

```c
Probe = 5;
if ( xrtAVLRemove(&Tree, &Probe, exampleCompareIntrusive,
	NULL) != &Items[0].Node ) {
	return 12;
}
```

### `xrtAVLFind`

查找与 key 相等的节点，未找到是正常结果。

```c
xavlnode* xrtAVLFind(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |
| `pKey` | 输入 | 借用 | 查找键 |
| `pCompare` | 输入 | 非空 | 比较器 |
| `pUserData` | 输入 | 任意值 | 原样传给比较器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 节点 | 相等节点（借用，树期间有效） | — |
| `NULL` | 未找到（正常结果）或参数非法 | 未找到不设置错误 |

#### 错误

- `XERR_ARGUMENT` — 树/键/比较器为空

#### 范例

[containers/avl_tour · 侵入式](../../examples/containers/avl_tour/main.c) · 命中返回插入时的原节点

```c
if ( (xrtAVLFind(&Tree, &Probe,
		exampleCompareIntrusive, NULL) !=
		&Items[0].Node) ) {
	return 2;
}
```

### 边界与首尾

### `xrtAVLLowerBound`

返回第一项不小于 key 的节点。

```c
xavlnode* xrtAVLLowerBound(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |
| `pKey` | 输入 | 借用 | 边界键 |
| `pCompare` | 输入 | 非空 | 比较器 |
| `pUserData` | 输入 | 任意值 | 原样传给比较器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 节点 | 第一项 `>= key`（借用） | — |
| `NULL` | 全部小于 key（正常结果）或参数非法 | 未找到不设置错误 |

#### 错误

- `XERR_ARGUMENT` — 树/键/比较器为空

#### 范例

[containers/avl_tour · 侵入式](../../examples/containers/avl_tour/main.c) · 键 4 不存在：下界与上界同为 5

```c
if ( (xrtAVLLowerBound(&Tree, &Probe,
		exampleCompareIntrusive, NULL) !=
		&Items[0].Node) ||
```

### `xrtAVLUpperBound`

返回第一项严格大于 key 的节点。

```c
xavlnode* xrtAVLUpperBound(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |
| `pKey` | 输入 | 借用 | 边界键 |
| `pCompare` | 输入 | 非空 | 比较器 |
| `pUserData` | 输入 | 任意值 | 原样传给比较器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 节点 | 第一项 `> key`（借用） | — |
| `NULL` | 没有严格大于项（正常结果）或参数非法 | 未找到不设置错误 |

#### 错误

- `XERR_ARGUMENT` — 树/键/比较器为空

#### 范例

[containers/avl_tour · 侵入式](../../examples/containers/avl_tour/main.c) · 键 5 存在：上界严格大于 → 8

```c
Probe = 5;  /* 存在：UpperBound → 8（严格大于）。 */
if ( xrtAVLUpperBound(&Tree, &Probe,
		exampleCompareIntrusive, NULL) !=
	&Items[2].Node ) {
	return 4;
}
```

### `xrtAVLFirst`

返回按比较器顺序排列的第一项。

```c
xavlnode* xrtAVLFirst(const xavl* pTree);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 节点 | 升序首项（借用） | — |
| `NULL` | 空树（正常结果）或参数非法 | 空树不设置错误 |

#### 错误

- `XERR_ARGUMENT` — `pTree` 为空

#### 范例

[containers/avl_tour · 侵入式](../../examples/containers/avl_tour/main.c) · 乱序插入 5,2,8,1,9 后首=1 末=9

```c
if ( (xrtAVLFirst(&Tree) != &Items[3].Node) ||
	(xrtAVLLast(&Tree) != &Items[4].Node) ) {
	return 5;
}
```

### `xrtAVLLast`

返回按比较器顺序排列的最后一项。

```c
xavlnode* xrtAVLLast(const xavl* pTree);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 节点 | 升序末项（借用） | — |
| `NULL` | 空树（正常结果）或参数非法 | 空树不设置错误 |

#### 错误

- `XERR_ARGUMENT` — `pTree` 为空

#### 范例

[containers/avl_tour · 侵入式](../../examples/containers/avl_tour/main.c) · 与 First 配对核对

```c
if ( (xrtAVLFirst(&Tree) != &Items[3].Node) ||
	(xrtAVLLast(&Tree) != &Items[4].Node) ) {
	return 5;
}
```

### 访问与迭代

### `xrtAVLVisit`

按升序访问节点并返回实际访问数量。

```c
size_t xrtAVLVisit(const xavl* pTree, xavlvisitor pVisitor, ptr pUserData);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树；访问期间不得修改结构 |
| `pVisitor` | 输入 | 非空 | 访问器；返回 `false` 提前结束 |
| `pUserData` | 输入 | 任意值 | 原样传给访问器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 实际访问数量（提前停止时小于总数） | — |
| `0` | 参数非法或空树 | 参数非法时设置错误 |

#### 错误

- `XERR_ARGUMENT` — 树/访问器为空

#### 范例

[containers/avl_tour · 侵入式](../../examples/containers/avl_tour/main.c) · 第三个返回 false → 恰好访问 3 个

```c
iSeen = 0;
if ( (xrtAVLVisit(&Tree, exampleVisitCount, &iSeen) !=
	3u) || (iSeen != 3u) ) {
	return 6;
}
```

### `xrtAVLIterBegin`

启动升序外置迭代器。

```c
bool xrtAVLIterBegin(const xavl* pTree, xavliter* pIterator);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树（空树也可启动，首次 `IterNext` 即 `NULL`） |
| `pIterator` | 输出 | 非空 | 接收迭代状态，零分配 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 迭代器已就绪 | — |
| `false` | 参数非法 | `*pIterator` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 树/迭代器为空

#### 范例

[containers/avl · 中序迭代](../../examples/containers/avl/main.c) · 插入顺序与有序视图解耦

```c
xrtAVLIterBegin(&tSessions, &tIterator);
while ( true ) {
	xavlnode* pNode = xrtAVLIterNext(&tIterator);
```

### `xrtAVLIterRBegin`

启动降序外置迭代器。

```c
bool xrtAVLIterRBegin(const xavl* pTree, xavliter* pIterator);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |
| `pIterator` | 输出 | 非空 | 接收降序迭代状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 迭代器已就绪 | — |
| `false` | 参数非法 | `*pIterator` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 树/迭代器为空

#### 范例

[containers/avl_tour · 侵入式](../../examples/containers/avl_tour/main.c) · 降序首项是最大键 9

```c
if ( !xrtAVLIterRBegin(&Tree, &Iter) ||
	((pNode = xrtAVLIterNext(&Iter)) == NULL) ||
```

### `xrtAVLIterFrom`

从第一项不小于 key 的节点开始升序迭代。

```c
bool xrtAVLIterFrom(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData,
	xavliter* pIterator
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |
| `pKey` | 输入 | 借用 | 范围起点键 |
| `pCompare` | 输入 | 非空 | 比较器 |
| `pUserData` | 输入 | 任意值 | 原样传给比较器 |
| `pIterator` | 输出 | 非空 | 接收迭代状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 迭代器已就绪（O(log n) 构造起始路径） | — |
| `false` | 参数非法 | `*pIterator` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 任一指针为空

#### 范例

[containers/avl_tour · 侵入式](../../examples/containers/avl_tour/main.c) · 从 5 起升序共 3 项

```c
if ( !xrtAVLIterFrom(&Tree, &Probe,
		exampleCompareIntrusive, NULL, &Iter) ) {
	return 8;
}
while ( (pNode = xrtAVLIterNext(&Iter)) != NULL ) {
	++iSeen;
}
```

### `xrtAVLIterRFrom`

从第一项不大于 key 的节点开始降序迭代。

```c
bool xrtAVLIterRFrom(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData,
	xavliter* pIterator
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |
| `pKey` | 输入 | 借用 | 范围起点键 |
| `pCompare` | 输入 | 非空 | 比较器 |
| `pUserData` | 输入 | 任意值 | 原样传给比较器 |
| `pIterator` | 输出 | 非空 | 接收降序迭代状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 迭代器已就绪 | — |
| `false` | 参数非法 | `*pIterator` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 任一指针为空

#### 范例

[containers/avl_tour · 侵入式](../../examples/containers/avl_tour/main.c) · 从 8 起降序共 4 项

```c
if ( !xrtAVLIterRFrom(&Tree, &Probe,
		exampleCompareIntrusive, NULL, &Iter) ) {
	return 10;
}
while ( (pNode = xrtAVLIterNext(&Iter)) != NULL ) {
	++iSeen;
}
```

### `xrtAVLIterNext`

返回下一节点；正常结束或结构已修改时返回空指针。

```c
xavlnode* xrtAVLIterNext(xavliter* pIterator);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIterator` | 输入/输出 | 非空、活动 | 步进；耗尽后自动转为结束状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 节点 | 下一节点（借用） | — |
| `NULL` | 正常耗尽（不设错）、结构已修改或参数非法 | 结构修改设置 `XERR_STATE`；参数非法设置 `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — `pIterator` 为空或未处于活动状态
- `XERR_STATE` — 迭代期间树结构被修改（版本变化）

#### 范例

[containers/avl_tour · 侵入式](../../examples/containers/avl_tour/main.c) · while 循环驱动到耗尽

```c
while ( (pNode = xrtAVLIterNext(&Iter)) != NULL ) {
	++iSeen;
}
xrtAVLIterEnd(&Iter);
```

### `xrtAVLIterEnd`

提前结束迭代并清除它持有的借用状态。正常耗尽的迭代器已自动结束，再调用是无害的。

```c
void xrtAVLIterEnd(xavliter* pIterator);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIterator` | 输入 | 允许空 | 要结束的迭代器 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 纯清理，不失败 |

#### 范例

[containers/avl_tour · 侵入式](../../examples/containers/avl_tour/main.c) · 提前退出后显式结束

```c
xrtAVLIterEnd(&Iter);
if ( iSeen != 4u ) {
	return 11;
}
```

## 拥有型 AVLTree

### 所有权与存储

```c
typedef struct xavltree {
	xavl Base;
	xpool Pool;
	size_t ItemSize;
	size_t ItemOffset;
	size_t Alignment;
	xavltreecompare Compare;
	xavltreedrop Drop;
	ptr UserData;
	uint32 Flags;
} xavltree;
```

对象按固定大小存入 `xpool`。添加后对象地址在删除、清空或销毁前保持稳定，后续插入和池扩页不会移动已有对象。结构公开用于底层诊断和与侵入式层组合，不允许调用方修改字段不变量。

底层池按约 64 KiB 的目标页大小自适应槽数。小节点仍使用 256 槽页以保持吞吐，
大对象则自动降低每页槽数，避免旧版固定页容量造成 `256 × 节点大小` 的首次分配放大。
当业务需要显式控制该权衡时，可以直接使用公开的侵入式 `xavl` 与自定义存储组合。

对象使用浅字节复制。若对象含有指针、句柄或其他资源，成功添加后这些资源应视为已移交给树，并通过 `xrtAVLTreeSetDrop()` 安装释放器。重复添加不会接管候选对象内的资源；候选对象仍由调用方处理。

### 生命周期与对齐

### `xrtAVLTreeInit`

使用默认 16 字节对象对齐初始化拥有式树。

```c
bool xrtAVLTreeInit(
	xavltree* pTree,
	size_t iItemSize,
	xavltreecompare pCompare,
	ptr pUserData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输出 | 非空 | 接收树；失败时内容不变 |
| `iItemSize` | 输入 | `> 0` | 对象字节数（池槽大小） |
| `pCompare` | 输入 | 非空 | 拥有式比较器 |
| `pUserData` | 输入 | 任意值 | 存入树，供比较器与释放器使用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 空树已就绪（池页惰性分配） | — |
| `false` | 参数非法或大小溢出 | `*pTree` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 任一指针为空、`iItemSize` 为零或对齐非法
- `XERR_RANGE` — 对象大小或对齐计算溢出

#### 范例

[containers/avl_tree · 配置表](../../examples/containers/avl_tree/main.c) · 初始化即绑定大小与比较器

```c
if ( !xrtAVLTreeInit(&tConfigs, sizeof(exampleconfig), exampleCompare, NULL) ) {
	return 1;
}
```

### `xrtAVLTreeInitAligned`

使用显式对象对齐初始化拥有式树。对齐必须是非零二次幂；对象大小不必是对齐的倍数（每个对象位于独立池槽内）。

```c
bool xrtAVLTreeInitAligned(
	xavltree* pTree,
	size_t iItemSize,
	size_t iAlignment,
	xavltreecompare pCompare,
	ptr pUserData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输出 | 非空 | 接收树 |
| `iItemSize` | 输入 | `> 0` | 对象字节数 |
| `iAlignment` | 输入 | 非零二次幂 | 对象对齐 |
| `pCompare` | 输入 | 非空 | 拥有式比较器 |
| `pUserData` | 输入 | 任意值 | 存入树 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 空树已就绪 | — |
| `false` | 参数非法或计算溢出 | `*pTree` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空、大小为零或对齐不是二次幂
- `XERR_RANGE` — 大小/对齐计算溢出

#### 范例

[containers/avl_tour · 对齐变体](../../examples/containers/avl_tour/main.c) · int 元素配 int 对齐

```c
if ( !xrtAVLTreeInitAligned(&Embedded, sizeof(int),
	sizeof(int), exampleCompareOwned, NULL) ) {
	return 30;
}
```

### `xrtAVLTreeCreate`

创建使用默认 16 字节对象对齐的拥有式树。

```c
xavltree* xrtAVLTreeCreate(
	size_t iItemSize,
	xavltreecompare pCompare,
	ptr pUserData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iItemSize` | 输入 | `> 0` | 对象字节数 |
| `pCompare` | 输入 | 非空 | 拥有式比较器 |
| `pUserData` | 输入 | 任意值 | 存入树，Drop 回调经它取计数器等 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 堆分配的空树，调用方负责 `Destroy` | — |
| `NULL` | 参数非法、溢出或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 比较器为空或 `iItemSize` 为零
- `XERR_RANGE` — 大小/对齐计算溢出
- `XERR_MEMORY` — 树结构分配失败

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · Create 时传入 Drop 的用户数据

```c
pTree = xrtAVLTreeCreate(sizeof(int),
	exampleCompareOwned, &iDropped);
if ( pTree == NULL ) {
	return 15;
}
```

### `xrtAVLTreeCreateAligned`

创建使用显式对象对齐的拥有式树。

```c
xavltree* xrtAVLTreeCreateAligned(
	size_t iItemSize,
	size_t iAlignment,
	xavltreecompare pCompare,
	ptr pUserData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iItemSize` | 输入 | `> 0` | 对象字节数 |
| `iAlignment` | 输入 | 非零二次幂 | 对象对齐 |
| `pCompare` | 输入 | 非空 | 拥有式比较器 |
| `pUserData` | 输入 | 任意值 | 存入树 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 堆分配的空树 | — |
| `NULL` | 参数非法、溢出或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtAVLTreeCreate`（`ARGUMENT`/`RANGE`/`MEMORY`）

#### 范例

[containers/avl_tour · 对齐变体](../../examples/containers/avl_tour/main.c) · 对齐堆形态与 Destroy 配对

```c
xavltree* pAligned = xrtAVLTreeCreateAligned(
	sizeof(int), sizeof(int),
	exampleCompareOwned, NULL);

if ( pAligned == NULL ) {
	return 31;
}
xrtAVLTreeDestroy(pAligned);
```

### `xrtAVLTreeSetDrop`

为仍为空的树设置对象资源释放器。

```c
bool xrtAVLTreeSetDrop(xavltree* pTree, xavltreedrop pDrop);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入/输出 | 非空、空树 | 目标树 |
| `pDrop` | 输入 | 可空 | 释放器；空表示撤销（仍要求空树） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 释放器已安装 | — |
| `false` | 树非空或参数非法 | 树不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pTree` 为空
- `XERR_STATE` — 树中已有对象（避免漏调已有对象的释放器）

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · Create 后、Add 前安装

```c
if ( !xrtAVLTreeSetDrop(pTree, exampleDrop) ) {
	xrtAVLTreeDestroy(pTree);
	return 16;
}
```

### `xrtAVLTreeUnit`

释放全部对象和池页，但不释放树结构。内嵌形态的收尾。

```c
void xrtAVLTreeUnit(xavltree* pTree);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 允许空 | 目标树；先调用全部释放器再释放池页 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作；释放器内重入同一棵树会被忙状态拒绝 |

#### 范例

[containers/avl_tour · 对齐变体](../../examples/containers/avl_tour/main.c) · 内嵌树用后归位

```c
xrtAVLTreeUnit(&Embedded);
```

### `xrtAVLTreeDestroy`

释放全部对象、池页和树结构。`Create`/`CreateAligned` 产物的收尾。

```c
void xrtAVLTreeDestroy(xavltree* pTree);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 允许空 | 目标树；流程为释放器 → 池页 → 树结构 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作；释放器重入 `Destroy` 的 `Unit` 被拒绝后不会继续释放树结构 |

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · Drop 之后 Destroy 不再触发 Drop

```c
xrtAVLTreeDestroy(pTree);
if ( iDropped != 2u ) {
	return 29;
}
```

### `xrtAVLTreeClear`

清空全部对象并保留固定池的复用能力。

```c
void xrtAVLTreeClear(xavltree* pTree);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 允许空 | 目标树；逐对象调用释放器后清索引 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 池页保留，后续 `Add` 不再分配 |

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · Clear 触发剩余对象的 Drop

```c
xrtAVLTreeClear(pTree);
printf(" after-clear drop=%zu", iDropped);
if ( (iDropped != 2u) || (xrtAVLTreeCount(pTree) != 0u) ) {
```

### `xrtAVLTreeCount`

返回当前对象数量，非法树返回零。

```c
size_t xrtAVLTreeCount(const xavltree* pTree);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 允许空 | 目标树 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `>= 0` | 当前对象数量（并发快照口径由外部同步决定） |
| `0` | 空树或非法树；纯查询不设置错误 |

#### 错误

- 无 — 空指针返回零是查询结果

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · 添加 3 项后核对

```c
if ( (xrtAVLTreeCount(pTree) != 3u) ) {
	xrtAVLTreeDestroy(pTree);
	return 18;
}
```

### 添加与所有权移交

### `xrtAVLTreeAdd`

复制添加对象；`pKey` 必须等价于对象内排序键，重复时不覆盖已有对象。

```c
ptr xrtAVLTreeAdd(
	xavltree* pTree,
	const void* pKey,
	const void* pItem,
	bool* pNew
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入/输出 | 非空 | 目标树 |
| `pKey` | 输入 | 借用 | 复制前验证与 `pItem` 比较为零 |
| `pItem` | 输入 | 借用、独立 | 复制来源；不得是树结构、池元数据或同树对象 |
| `pNew` | 输出 | 可空 | 接收"是否新插入" |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 池内新对象（地址稳定到删除）；或重复键时已有对象 | — |
| `NULL` | 参数非法、键不等价、别名或 OOM | 树、池计数与已有对象均不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空、`pKey` 与 `pItem` 比较结果非零、或复制来源与树/池别名
- `XERR_MEMORY` — 池页分配失败
- `XERR_RANGE` — 计数溢出

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · 键与对象同源（int 树）

```c
if ( xrtAVLTreeAdd(pTree, &Keys[i], &Keys[i], NULL) ==
	NULL ) {
	xrtAVLTreeDestroy(pTree);
	return 17;
}
```

### 查找、边界与删除

### `xrtAVLTreeFind`

查找可修改对象，未找到是正常结果。

```c
ptr xrtAVLTreeFind(xavltree* pTree, const void* pKey);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |
| `pKey` | 输入 | 借用 | 查找键（树创建时的比较器解释） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 池内对象借用地址；可修改非排序字段 | — |
| `NULL` | 未找到（正常结果）或参数非法 | 未找到不设置错误 |

#### 错误

- `XERR_ARGUMENT` — 树/键为空

#### 范例

[containers/avl_tree · 配置表](../../examples/containers/avl_tree/main.c) · 命中后直改非键字段

```c
exampleconfig* pConfig = (exampleconfig*)xrtAVLTreeFind(&tConfigs, &iSearch);
```

### `xrtAVLTreeConstFind`

查找只读对象，未找到是正常结果。

```c
const void* xrtAVLTreeConstFind(const xavltree* pTree, const void* pKey);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |
| `pKey` | 输入 | 借用 | 查找键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 只读对象借用地址 | — |
| `NULL` | 未找到（正常结果）或参数非法 | 未找到不设置错误 |

#### 错误

- `XERR_ARGUMENT` — 树/键为空

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · 只读上下文取值

```c
(*(const int*)xrtAVLTreeConstFind(pTree,
	&Probe) != 20) ||
```

### `xrtAVLTreeHas`

判断指定键是否存在。

```c
bool xrtAVLTreeHas(const xavltree* pTree, const void* pKey);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |
| `pKey` | 输入 | 借用 | 查找键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 键存在 | — |
| `false` | 不存在（正常结果）或参数非法 | 不存在不设置错误 |

#### 错误

- `XERR_ARGUMENT` — 树/键为空

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · Take 之后 Has 应为假

```c
if ( !xrtAVLTreeTake(pTree, &Probe, &Value) ||
	(Value != 20) || xrtAVLTreeHas(pTree, &Probe) ) {
```

### `xrtAVLTreeRemove`

删除对象并调用资源释放器。

```c
bool xrtAVLTreeRemove(xavltree* pTree, const void* pKey);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入/输出 | 非空 | 目标树 |
| `pKey` | 输入 | 借用 | 要删除的键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已删除：先释放器、后归还池槽 | — |
| `false` | 未找到（正常结果）或参数非法 | 未找到不设置错误 |

#### 错误

- `XERR_ARGUMENT` — 树/键为空

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · 删 10 后计数减一

```c
Probe = 10;
if ( !xrtAVLTreeRemove(pTree, &Probe) ||
	(xrtAVLTreeCount(pTree) != 1u) ) {
```

### `xrtAVLTreeTake`

将对象字节移交给调用方后删除，不调用资源释放器。

```c
bool xrtAVLTreeTake(xavltree* pTree, const void* pKey, ptr pItem);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入/输出 | 非空 | 目标树 |
| `pKey` | 输入 | 借用 | 要移出的键 |
| `pItem` | 输出 | 非空、独立缓冲 | 接收完整对象字节；资源随输出移交 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已复制到 `*pItem` 并删除池槽 | — |
| `false` | 未找到（正常结果）、参数非法或输出别名 | 未找到不设置错误；树不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或 `pItem` 触及树结构/池元数据/同树对象池（完整区间别名拒绝）

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · 移出 20，资源归调用方（不计 Drop）

```c
Probe = 20;
if ( !xrtAVLTreeTake(pTree, &Probe, &Value) ||
	(Value != 20) || xrtAVLTreeHas(pTree, &Probe) ) {
```

### `xrtAVLTreeFirst`

返回顺序第一项。

```c
ptr xrtAVLTreeFirst(xavltree* pTree);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 升序首对象（借用） | — |
| `NULL` | 空树（正常结果）或参数非法 | 空树不设置错误 |

#### 错误

- `XERR_ARGUMENT` — `pTree` 为空

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · 首尾与边界一次核对

```c
if ( (*(int*)xrtAVLTreeFirst(pTree) != 10) ||
	(*(int*)xrtAVLTreeLast(pTree) != 30) ||
```

### `xrtAVLTreeLast`

返回顺序最后一项。

```c
ptr xrtAVLTreeLast(xavltree* pTree);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 升序末对象（借用） | — |
| `NULL` | 空树（正常结果）或参数非法 | 空树不设置错误 |

#### 错误

- `XERR_ARGUMENT` — `pTree` 为空

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · 与 First 配对

```c
(*(int*)xrtAVLTreeLast(pTree) != 30) ||
(*(int*)xrtAVLTreeLowerBound(pTree, &Probe) != 20) ||
```

### `xrtAVLTreeLowerBound`

返回第一项不小于 key 的对象。

```c
ptr xrtAVLTreeLowerBound(xavltree* pTree, const void* pKey);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |
| `pKey` | 输入 | 借用 | 边界键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 第一项 `>= key` 的对象（借用） | — |
| `NULL` | 全部小于 key（正常结果）或参数非法 | 未找到不设置错误 |

#### 错误

- `XERR_ARGUMENT` — 树/键为空

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · LowerBound(20)=20

```c
(*(int*)xrtAVLTreeLowerBound(pTree, &Probe) != 20) ||
(*(int*)xrtAVLTreeUpperBound(pTree, &Probe) != 30) ) {
```

### `xrtAVLTreeUpperBound`

返回第一项严格大于 key 的对象。

```c
ptr xrtAVLTreeUpperBound(xavltree* pTree, const void* pKey);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |
| `pKey` | 输入 | 借用 | 边界键 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 第一项 `> key` 的对象（借用） | — |
| `NULL` | 没有严格大于项（正常结果）或参数非法 | 未找到不设置错误 |

#### 错误

- `XERR_ARGUMENT` — 树/键为空

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · UpperBound(20)=30

```c
(*(int*)xrtAVLTreeUpperBound(pTree, &Probe) != 30) ) {
	xrtAVLTreeDestroy(pTree);
	return 20;
}
```

### 访问与迭代

### `xrtAVLTreeVisit`

按升序访问对象；回调期间查询可用，结构和生命周期修改被拒绝。

```c
size_t xrtAVLTreeVisit(
	xavltree* pTree,
	xavltreevisitor pVisitor,
	ptr pUserData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树；独占访问 |
| `pVisitor` | 输入 | 非空 | 访问器；返回 `false` 提前结束 |
| `pUserData` | 输入 | 任意值 | 原样传给访问器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 实际访问数量 | — |
| `0` | 参数非法或空树 | 参数非法时设置错误 |

#### 错误

- `XERR_ARGUMENT` — 树/访问器为空
- `XERR_STATE` — 回调内重入 `Add`/`Remove`/`Take`/`Clear`/`Unit`/`Destroy`/`SetDrop`/嵌套 `Visit`；外层访问继续有效

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · 全量访问计数核对

```c
iSeen = 0;
if ( (xrtAVLTreeVisit(pTree, exampleVisitOwned, &iSeen) !=
	3u) || (iSeen != 3u) ) {
```

### `xrtAVLTreeIterBegin`

启动拥有式树升序迭代。

```c
bool xrtAVLTreeIterBegin(xavltree* pTree, xavltreeiter* pIterator);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |
| `pIterator` | 输出 | 非空 | 接收迭代状态，零分配 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 迭代器已就绪 | — |
| `false` | 参数非法 | `*pIterator` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 树/迭代器为空

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · 升序走完全部 3 项

```c
if ( !xrtAVLTreeIterBegin(pTree, &Iter) ) {
	xrtAVLTreeDestroy(pTree);
	return 22;
}
while ( xrtAVLTreeIterNext(&Iter) != NULL ) {
	++iSeen;
}
```

### `xrtAVLTreeIterRBegin`

启动拥有式树降序迭代。

```c
bool xrtAVLTreeIterRBegin(xavltree* pTree, xavltreeiter* pIterator);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |
| `pIterator` | 输出 | 非空 | 接收降序迭代状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 迭代器已就绪 | — |
| `false` | 参数非法 | `*pIterator` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 树/迭代器为空

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · 启动后即结束覆盖接口

```c
if ( xrtAVLTreeIterRBegin(pTree, &Iter) ) {
	xrtAVLTreeIterEnd(&Iter);
}
```

### `xrtAVLTreeIterFrom`

从第一项不小于 key 的对象开始升序迭代。

```c
bool xrtAVLTreeIterFrom(
	xavltree* pTree,
	const void* pKey,
	xavltreeiter* pIterator
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |
| `pKey` | 输入 | 借用 | 范围起点键（树创建时的比较器解释） |
| `pIterator` | 输出 | 非空 | 接收迭代状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 迭代器已就绪（O(log n) 起点） | — |
| `false` | 参数非法 | `*pIterator` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 任一指针为空

#### 范例

[containers/avl_tree · 范围迭代](../../examples/containers/avl_tree/main.c) · 等价 `WHERE id >= 20 ORDER BY id`

```c
if ( xrtAVLTreeIterFrom(&tConfigs, &iSearch, &tIterator) ) {
	exampleconfig* pConfig;

	while ( (pConfig = (exampleconfig*)xrtAVLTreeIterNext(&tIterator)) != NULL ) {
```

### `xrtAVLTreeIterRFrom`

从第一项不大于 key 的对象开始降序迭代。

```c
bool xrtAVLTreeIterRFrom(
	xavltree* pTree,
	const void* pKey,
	xavltreeiter* pIterator
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTree` | 输入 | 非空 | 目标树 |
| `pKey` | 输入 | 借用 | 范围起点键 |
| `pIterator` | 输出 | 非空 | 接收降序迭代状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 迭代器已就绪 | — |
| `false` | 参数非法 | `*pIterator` 不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 任一指针为空

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · RFrom(20) 降序共 2 项

```c
if ( !xrtAVLTreeIterRFrom(pTree, &Probe, &Iter) ) {
	xrtAVLTreeDestroy(pTree);
	return 24;
}
while ( (pItem = xrtAVLTreeIterNext(&Iter)) != NULL ) {
	++iSeen;
}
```

### `xrtAVLTreeIterNext`

返回下一对象；正常结束或结构已修改时返回空指针。

```c
ptr xrtAVLTreeIterNext(xavltreeiter* pIterator);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIterator` | 输入/输出 | 非空、活动 | 步进；耗尽后自动结束 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 下一对象（池内借用地址） | — |
| `NULL` | 正常耗尽（不设错）、结构已修改或参数非法 | 结构修改设置 `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — `pIterator` 为空或未活动
- `XERR_STATE` — 迭代期间树结构被修改（版本变化），迭代自动结束

#### 范例

[containers/avl_tree · 范围迭代](../../examples/containers/avl_tree/main.c) · while 驱动到范围末尾

```c
while ( (pConfig = (exampleconfig*)xrtAVLTreeIterNext(&tIterator)) != NULL ) {
	printf("range id=%d timeout=%d\n", pConfig->ID, pConfig->Timeout);
}
```

### `xrtAVLTreeIterEnd`

提前结束拥有式树迭代。

```c
void xrtAVLTreeIterEnd(xavltreeiter* pIterator);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIterator` | 输入 | 允许空 | 要结束的迭代器 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 纯清理，不失败 |

#### 范例

[containers/avl_tour · 拥有式](../../examples/containers/avl_tour/main.c) · 每轮迭代结束后的收尾

```c
xrtAVLTreeIterEnd(&Iter);
if ( iSeen != 3u ) {
	xrtAVLTreeDestroy(pTree);
	return 23;
}
```

## 错误

| 场景 | 错误种类 |
|---|---|
| 空树、空节点、空比较器、空对象、键不等价或空输出参数 | `XERR_ARGUMENT` |
| 新节点已挂树、访问回调重入修改、迭代期间结构改变、公开摘要损坏 | `XERR_STATE` |
| 计数、对象大小或对齐计算溢出 | `XERR_RANGE` |
| 节点池或树结构分配失败 | `XERR_MEMORY` |

未找到不是错误。和 XRT 其他查询 API 一样，成功和未找到不会清除较早的错误；需要区分时，在操作前调用 `xrtClearError()`。

## 示例

### 侵入式会话索引

```c
typedef struct session {
	int ID;
	xavlnode Index;
} session;

xavl Sessions;
session Value = { 7, { 0 } };

xrtAVLInit(&Sessions);
xrtAVLNodeInit(&Value.Index);
xrtAVLInsert(&Sessions, &Value.Index, &Value.ID, compare_session, NULL, NULL);
```

### 拥有型对象树

```c
xavltree Configs;
config Value = { 7, 3000 };

if ( xrtAVLTreeInit(&Configs, sizeof(config), compare_config, NULL) ) {
	xrtAVLTreeAdd(&Configs, &Value.ID, &Value, NULL);
	config* pValue = (config*)xrtAVLTreeFind(&Configs, &Value.ID);
	/* 使用 pValue；不要修改 ID。 */
	xrtAVLTreeUnit(&Configs);
}
```

完整可编译示例位于 `examples/containers/avl` 和 `examples/containers/avl_tree`。
