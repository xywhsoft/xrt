# Set

`set.h` 提供固定大小元素的通用哈希集合。空集合不分配内存，每个元素使用一个紧凑独立条目，平均查找复杂度为 `O(1)`，并保持元素地址和首次插入顺序稳定。

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
