# AVL 与 AVLTree

`avl` 是不拥有节点内存的侵入式有序索引；`avl_tree` 在它上面增加固定对象池、按值复制和资源释放回调。两层使用同一套 AVL 平衡核心，但面向不同成本模型，调用方不需要为了使用底层能力而承担拥有型容器的分配和所有权规则。

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

```c
void xrtAVLNodeInit(xavlnode* pNode);
bool xrtAVLInit(xavl* pTree);
void xrtAVLClear(xavl* pTree);
```

每个新节点第一次插入前必须调用 `xrtAVLNodeInit()`。成功删除的节点会自动恢复为相同的独立状态，可以再次插入。

`xrtAVLClear()` 只让树忘记所有节点，不遍历、不释放，也不逐个重置旧节点。它适合外部对象整体失效或即将统一释放的场景；仍要复用旧节点时，调用方必须重新执行 `xrtAVLNodeInit()`。

### 插入、删除与查找

```c
xavlnode* xrtAVLInsert(
	xavl* pTree,
	xavlnode* pNode,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData,
	bool* pNew
);

xavlnode* xrtAVLRemove(
	xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData
);

xavlnode* xrtAVLFind(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData
);
```

`Insert` 成功插入时返回 `pNode` 并把 `*pNew` 设为 `true`；重复键返回已有节点并设为 `false`，候选节点完全不变。新键对应的候选节点若不是初始化后的独立状态，操作失败并设置 `XERR_STATE`。

`Remove` 返回调用方最初插入的原节点，即使删除双子节点时内部需要移动前驱节点也不会改变返回身份。未找到返回 `NULL`。

`pKey` 可以是任何比较器理解的表示形式，包括整数地址、字符串视图或复合查找条件；XRT 不解释也不保存它。

### 边界与首尾

```c
xavlnode* xrtAVLLowerBound(const xavl* pTree, const void* pKey, xavlcompare pCompare, ptr pUserData);
xavlnode* xrtAVLUpperBound(const xavl* pTree, const void* pKey, xavlcompare pCompare, ptr pUserData);
xavlnode* xrtAVLFirst(const xavl* pTree);
xavlnode* xrtAVLLast(const xavl* pTree);
```

`LowerBound` 返回第一项 `>= key` 的节点，`UpperBound` 返回第一项 `> key` 的节点。不存在符合项时返回 `NULL`。

### 访问与迭代

```c
size_t xrtAVLVisit(const xavl* pTree, xavlvisitor pVisitor, ptr pUserData);
bool xrtAVLIterBegin(const xavl* pTree, xavliter* pIterator);
bool xrtAVLIterRBegin(const xavl* pTree, xavliter* pIterator);
bool xrtAVLIterFrom(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData,
	xavliter* pIterator
);
bool xrtAVLIterRFrom(
	const xavl* pTree,
	const void* pKey,
	xavlcompare pCompare,
	ptr pUserData,
	xavliter* pIterator
);
xavlnode* xrtAVLIterNext(xavliter* pIterator);
void xrtAVLIterEnd(xavliter* pIterator);
```

`Visit` 按升序访问，回调返回 `false` 时提前结束，函数返回实际访问数量。`IterBegin` 和 `IterRBegin` 分别启动升序和降序迭代。正常耗尽后迭代器会自动结束；提前退出时调用 `IterEnd`。

`IterFrom` 从第一项 `>= key` 的节点开始升序迭代，`IterRFrom` 从第一项
`<= key` 的节点开始降序迭代。两者都以 O(log n) 构造起始路径，
不需要先从树首或树尾扫描。

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

```c
bool xrtAVLTreeInit(xavltree* pTree, size_t iItemSize, xavltreecompare pCompare, ptr pUserData);
bool xrtAVLTreeInitAligned(xavltree* pTree, size_t iItemSize, size_t iAlignment, xavltreecompare pCompare, ptr pUserData);
xavltree* xrtAVLTreeCreate(size_t iItemSize, xavltreecompare pCompare, ptr pUserData);
xavltree* xrtAVLTreeCreateAligned(size_t iItemSize, size_t iAlignment, xavltreecompare pCompare, ptr pUserData);
bool xrtAVLTreeSetDrop(xavltree* pTree, xavltreedrop pDrop);
void xrtAVLTreeUnit(xavltree* pTree);
void xrtAVLTreeDestroy(xavltree* pTree);
void xrtAVLTreeClear(xavltree* pTree);
size_t xrtAVLTreeCount(const xavltree* pTree);
```

默认对象对齐为 16 字节。显式对齐必须是非零二次幂；对象大小不必是对齐的倍数，因为每个对象位于独立池槽内。

释放器只能在空树上设置。`Clear` 调用全部对象的释放器，清空索引并保留池的复用能力；
`Unit` 还释放池页并重置结构；`Destroy` 进一步释放树结构。
释放器中不得调用同一棵树的任何 API；实现会以忙状态拒绝这种重入，
尤其不会在重入 `Destroy` 的 `Unit` 被拒绝后继续释放树结构。

### 添加与所有权移交

```c
ptr xrtAVLTreeAdd(
	xavltree* pTree,
	const void* pKey,
	const void* pItem,
	bool* pNew
);
```

函数先复制完整对象，再把已经初始化的内部节点挂入树，不存在旧版“先插入未初始化槽、再由调用方填写排序字段”的瞬时错误状态。

`pKey` 必须与 `pItem` 内比较器可见的排序键等价，实现会在复制前验证比较结果为零。
成功插入返回新对象并把 `*pNew` 设为 `true`；重复键不分配、不覆盖已有对象，
返回旧对象并设为 `false`。不能把树结构、池元数据或同一树池内的对象作为复制来源，
以免破坏容器状态或浅拷贝同一份资源所有权。
OOM、键不等价、别名或参数错误时树、池计数和已有对象保持不变。

### 查找、边界与删除

```c
ptr xrtAVLTreeFind(xavltree* pTree, const void* pKey);
const void* xrtAVLTreeConstFind(const xavltree* pTree, const void* pKey);
bool xrtAVLTreeHas(const xavltree* pTree, const void* pKey);
bool xrtAVLTreeRemove(xavltree* pTree, const void* pKey);
bool xrtAVLTreeTake(xavltree* pTree, const void* pKey, ptr pItem);
ptr xrtAVLTreeFirst(xavltree* pTree);
ptr xrtAVLTreeLast(xavltree* pTree);
ptr xrtAVLTreeLowerBound(xavltree* pTree, const void* pKey);
ptr xrtAVLTreeUpperBound(xavltree* pTree, const void* pKey);
```

`Find`、首尾和边界函数返回池内借用地址。可以修改非排序字段；不得修改比较器可见的键。

`Remove` 先调用释放器再归还池槽。`Take` 把完整对象字节复制到调用方缓冲区，
再删除池槽且不调用释放器，因此对象内部资源随输出一起移交给调用方。
输出缓冲不得触及树结构、池元数据或同一树的对象池；
实现会在删除前拒绝这种可能覆盖容器状态的完整区间别名。

### 访问与迭代

```c
size_t xrtAVLTreeVisit(xavltree* pTree, xavltreevisitor pVisitor, ptr pUserData);
bool xrtAVLTreeIterBegin(xavltree* pTree, xavltreeiter* pIterator);
bool xrtAVLTreeIterRBegin(xavltree* pTree, xavltreeiter* pIterator);
bool xrtAVLTreeIterFrom(
	xavltree* pTree,
	const void* pKey,
	xavltreeiter* pIterator
);
bool xrtAVLTreeIterRFrom(
	xavltree* pTree,
	const void* pKey,
	xavltreeiter* pIterator
);
ptr xrtAVLTreeIterNext(xavltreeiter* pIterator);
void xrtAVLTreeIterEnd(xavltreeiter* pIterator);
```

语义与侵入式迭代一致，返回的是对象借用地址。
`IterFrom` 和 `IterRFrom` 提供 O(log n) 的升序、降序范围起点。
`xrtAVLTreeVisit` 的回调可以调用 `Count`、`Find`、`Has`、首尾、边界查询和外置迭代 API，
也可以直接修改不参与比较的对象字段。`Add`、`Remove`、`Take`、`Clear`、`Unit`、
`Destroy`、释放器替换和嵌套 `Visit` 会立即失败并报告 `XERR_STATE`，外层访问继续保持有效。

外置迭代器本身不占用树状态。调用方仍不得在它的有效期内改变树结构；
若结构被其他路径修改，下一次 `IterNext` 会检测版本变化、结束迭代并报告 `XERR_STATE`。

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
