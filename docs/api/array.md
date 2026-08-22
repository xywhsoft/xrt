# Array 与 PtrArray

`array` 提供固定元素大小的连续动态数组；`ptr_array` 在同一存储核心上提供指针类型友好的接口。两者都是本地容器，不隐式加锁，不接管元素内部资源。

## 裁剪与依赖

| 能力 | 宏 | 依赖 |
|---|---|---|
| 连续动态数组 | `XRT_FEATURE_ARRAY` | `core` |
| 指针数组 | `XRT_FEATURE_PTR_ARRAY` | `array` |

只启用 `XRT_FEATURE_ARRAY` 不会带入指针便利层。启用 `XRT_FEATURE_PTR_ARRAY` 时必须同时启用 `XRT_FEATURE_ARRAY`，公共头会在依赖不完整时拒绝编译。

## 稳定契约

- 所有索引和插入位点统一从 `0` 开始，有效索引范围是 `[0, Count)`。
- `xrtArrayGet()` 返回借用地址，任何可能改变容量、位置或顺序的操作都可能使旧地址失效。
- 数组只拥有连续存储区，不会释放结构体字段或指针元素指向的对象。
- 容器不带隐式锁。一个数组同时只能由一个执行流修改；共享时由调用方使用外部同步，或使用后续专门的并发容器。
- 扩容采用几何增长。小数组不会像旧版一样首次固定预留 256 个元素。
- 分配失败时，原来的 `Data`、`Count`、`Capacity` 和已有元素保持不变。
- `Reserve` 只增长容量，`Resize` 改变元素数量，`Trim` 才主动缩减容量。
- `Remove` 要求删除区间完整有效，不会静默截断到数组末尾。
- 排序使用 C `qsort`，不保证相等元素的原顺序。

`tests/containers/test_container_external_sync.c` 使用一把外部 `xmutex`，让主线程和三个工作线程共同更新同一个 Array、Map、IntMap、AVLTree 和固定池。该门禁验证显式同步路径能够替代旧版容器内置的 owner/shared 分支。

## 常量

### `XRT_ARRAY_ALIGNMENT_DEFAULT`

值为 `16`。普通数组的数据起始地址至少按 16 字节对齐。对常规 C 类型，`sizeof(T)` 会保持后续元素的自然对齐。

需要每个元素都按更大边界对齐时，使用 `xrtArrayInitAligned()` 或 `xrtArrayCreateAligned()`；此时元素大小必须是对齐值的倍数。

## 类型

### `xarraycompare`

```c
typedef int (*xarraycompare)(const void* pLeft, const void* pRight);
```

返回负数、零、正数分别表示左值小于、等于、大于右值。用于 `Sort` 时两个参数都是元素地址；用于 `FindBy` 和 `BSearch` 时第一个参数是 key，第二个参数是元素。

### `xarray`

```c
typedef struct xarray {
	bytes Data;
	ptr Allocation;
	size_t ItemSize;
	size_t Count;
	size_t Capacity;
	size_t Alignment;
} xarray;
```

| 字段 | 含义 |
|---|---|
| `Data` | 第一个元素的借用地址，可用于连续遍历 |
| `Allocation` | XRT 持有的原始分配，仅供诊断，不得修改或释放 |
| `ItemSize` | 单个元素字节数 |
| `Count` | 当前有效元素数 |
| `Capacity` | 无需再次分配即可容纳的元素数 |
| `Alignment` | 数据起始地址的对齐值 |

结构公开是为了底层代码能直接读取连续视图和计数，不表示调用方可以修改结构不变量。

### `xptrarray`

```c
typedef xarray xptrarray;
```

指针数组与普通数组共享完全相同的存储结构，其 `ItemSize` 固定为 `sizeof(ptr)`。需要底层通用操作时，可以直接使用 `xrtArray*` API；常见代码应优先使用 `xrtPtrArray*` API，避免手工传递指针地址。

## 生命周期

### `xrtArrayInit` / `xrtArrayInitAligned`

```c
bool xrtArrayInit(xarray* pArray, size_t iItemSize);
bool xrtArrayInitAligned(xarray* pArray, size_t iItemSize, size_t iAlignment);
```

初始化调用方持有的数组结构。元素大小必须大于零；显式对齐必须是二次幂，且 `iItemSize % iAlignment == 0`。成功后使用 `xrtArrayUnit()` 释放。

### `xrtArrayCreate` / `xrtArrayCreateAligned`

```c
xarray* xrtArrayCreate(size_t iItemSize);
xarray* xrtArrayCreateAligned(size_t iItemSize, size_t iAlignment);
```

在堆上创建数组结构。失败返回 `NULL`。成功后使用 `xrtArrayDestroy()`。

### `xrtArrayUnit` / `xrtArrayDestroy`

```c
void xrtArrayUnit(xarray* pArray);
void xrtArrayDestroy(xarray* pArray);
```

`Unit` 释放内部存储并把结构重置为零；`Destroy` 还会释放数组结构。两者允许空指针。它们不处理元素内部资源。

### `xrtArrayClear`

```c
void xrtArrayClear(xarray* pArray);
```

把 `Count` 设为零并保留容量。元素内原有字节不再属于活动区，也不会被析构或清零。

## 容量

### `xrtArrayReserve`

```c
bool xrtArrayReserve(xarray* pArray, size_t iCapacity);
```

保证容量不少于参数。已有容量足够时不分配；实际容量可能因几何增长大于请求值。

### `xrtArrayResize`

```c
bool xrtArrayResize(xarray* pArray, size_t iCount);
```

改变有效元素数量。增长时新增元素全部清零；缩小时只减少 `Count`，不释放容量。

### `xrtArrayTrim`

```c
bool xrtArrayTrim(xarray* pArray);
```

把容量精确裁剪到 `Count`。空数组会释放整个存储区。调用可能改变 `Data`。

## 访问与底层槽

### `xrtArrayGet` / `xrtArrayConstGet`

```c
ptr xrtArrayGet(xarray* pArray, size_t iIndex);
const void* xrtArrayConstGet(const xarray* pArray, size_t iIndex);
```

返回指定元素的借用地址。越界返回 `NULL` 并设置 `XERR_RANGE`。热路径可直接缓存 `Data` 和 `Count` 后连续遍历，不需要一组无边界检查的重复 API。

### `xrtArrayAdd`

```c
ptr xrtArrayAdd(xarray* pArray, size_t iCount);
```

在末尾增加 `iCount` 个未初始化元素，返回第一个新元素。适合取得槽后立即写满的底层路径。`iCount` 为零是参数错误。

### `xrtArrayInsertSpace`

```c
ptr xrtArrayInsertSpace(xarray* pArray, size_t iIndex, size_t iCount);
```

在 `[0, Count]` 位点插入未初始化槽。后续元素会移动，所有旧元素地址都应视为失效。

## 复制写入

### `xrtArrayPush`

```c
bool xrtArrayPush(xarray* pArray, const void* pItem);
```

复制一个元素到末尾，是最常见的单元素追加入口。

### `xrtArrayAppend`

```c
bool xrtArrayAppend(xarray* pArray, const void* pItems, size_t iCount);
```

复制追加连续元素。来源可以是当前数组的完整活动元素区间；即使扩容移动了 `Data`，复制仍然有效。数量为零时是成功的空操作，允许 `pItems == NULL`。

### `xrtArrayInsert`

```c
bool xrtArrayInsert(xarray* pArray, size_t iIndex, const void* pItems, size_t iCount);
```

在 `[0, Count]` 位点复制插入元素。支持来源来自数组自身，包括来源跨过插入位点的情况。实现按插入移动后的来源偏移直接复制，自引用路径不会额外分配临时副本；只有容量不足时才会分配新存储。

### `xrtArraySet`

```c
bool xrtArraySet(xarray* pArray, size_t iIndex, const void* pItem);
```

覆盖一个已有元素。允许来源是数组内另一个完整元素。

## 删除与重排

### `xrtArrayRemove`

```c
bool xrtArrayRemove(xarray* pArray, size_t iIndex, size_t iCount);
```

删除精确区间 `[iIndex, iIndex + iCount)` 并保持剩余顺序。零数量、起点越界或区间超出末尾均失败并设置 `XERR_RANGE`。

### `xrtArrayRemoveSwap`

```c
bool xrtArrayRemoveSwap(xarray* pArray, size_t iIndex);
```

用最后一个元素覆盖目标后减少数量，时间复杂度为 O(1)，但不保持顺序。

### `xrtArrayPop`

```c
bool xrtArrayPop(xarray* pArray, ptr pItem);
```

删除最后一个元素。`pItem` 非空时先复制元素，空指针表示只删除。输出缓冲不得与数组自身完整分配区重叠，避免覆盖仍然活动的其他元素；别名失败时数组不变。空数组失败并设置 `XERR_RANGE`。

### `xrtArraySwap` / `xrtArrayReverse`

```c
bool xrtArraySwap(xarray* pArray, size_t iLeft, size_t iRight);
bool xrtArrayReverse(xarray* pArray);
```

交换或反转元素。交换不分配堆内存，也不限制元素大小。

### `xrtArraySort`

```c
bool xrtArraySort(xarray* pArray, xarraycompare pCompare);
```

原地执行不稳定排序。比较器不能为空。排序会改变元素地址所代表的对象。

## 查找

### `xrtArrayFind`

```c
size_t xrtArrayFind(const xarray* pArray, const void* pItem);
```

按 `ItemSize` 字节完全相等查找，返回第一个索引；未找到返回 `XRT_NPOS`。结构体填充字节不稳定时应改用 `FindBy`。

### `xrtArrayFindBy`

```c
size_t xrtArrayFindBy(const xarray* pArray, const void* pKey, xarraycompare pCompare);
```

线性调用 `pCompare(pKey, pElement)`，返回第一个比较结果为零的索引；未找到返回 `XRT_NPOS`。

### `xrtArrayBSearch`

```c
size_t xrtArrayBSearch(const xarray* pArray, const void* pKey, xarraycompare pCompare);
```

在已按同一比较器排序的数组中二分查找。数组未排序时结果未定义；未找到返回 `XRT_NPOS`。

## 指针便利层

### 生命周期与容量

```c
bool xrtPtrArrayInit(xptrarray* pArray);
xptrarray* xrtPtrArrayCreate(void);
void xrtPtrArrayUnit(xptrarray* pArray);
void xrtPtrArrayDestroy(xptrarray* pArray);
void xrtPtrArrayClear(xptrarray* pArray);
bool xrtPtrArrayReserve(xptrarray* pArray, size_t iCapacity);
bool xrtPtrArrayResize(xptrarray* pArray, size_t iCount);
bool xrtPtrArrayTrim(xptrarray* pArray);
```

语义与对应的普通数组函数一致。`Resize` 新增的槽全部为 `NULL`。销毁、清空、删除都不会释放指针目标。

### 连续视图

```c
ptr* xrtPtrArrayData(xptrarray* pArray);
ptr const* xrtPtrArrayConstData(const xptrarray* pArray);
```

返回可直接遍历的 `ptr*` 视图。扩容、插入、删除、裁剪和排序后，旧视图不再可靠。

### 访问与写入

```c
ptr xrtPtrArrayGet(const xptrarray* pArray, size_t iIndex);
bool xrtPtrArraySet(xptrarray* pArray, size_t iIndex, ptr pValue);
bool xrtPtrArrayPush(xptrarray* pArray, ptr pValue);
bool xrtPtrArrayAppend(xptrarray* pArray, ptr const* pValues, size_t iCount);
bool xrtPtrArrayInsert(xptrarray* pArray, size_t iIndex, ptr pValue);
bool xrtPtrArrayInsertMany(xptrarray* pArray, size_t iIndex, ptr const* pValues, size_t iCount);
```

`Get` 越界和合法的 `NULL` 元素都会返回 `NULL`；区别是越界会设置 `XERR_RANGE`，合法空元素不会设置新错误。需要无歧义读取时，可先检查 `iIndex < Count` 或直接使用连续视图。

### 删除、排序与查找

```c
bool xrtPtrArrayRemove(xptrarray* pArray, size_t iIndex, size_t iCount);
bool xrtPtrArrayRemoveSwap(xptrarray* pArray, size_t iIndex);
bool xrtPtrArrayPop(xptrarray* pArray, ptr* pValue);
bool xrtPtrArraySwap(xptrarray* pArray, size_t iLeft, size_t iRight);
bool xrtPtrArrayReverse(xptrarray* pArray);
bool xrtPtrArraySort(xptrarray* pArray, xarraycompare pCompare);
size_t xrtPtrArrayFind(const xptrarray* pArray, const void* pValue);
```

`Pop` 要求输出参数非空。`Find` 按指针值比较，包括查找 `NULL`。`Sort` 直接使用 `qsort` 语义，因此比较器参数是“指向指针元素的地址”：

```c
static int compare_object_ptr(const void* pLeft, const void* pRight)
{
	const object* pA = *(const object* const*)pLeft;
	const object* pB = *(const object* const*)pRight;

	return (pA->Key > pB->Key) - (pA->Key < pB->Key);
}
```

稳定槽位和空洞复用不是连续指针数组的职责。需要稳定句柄时，使用
`slot_map`；它以代际句柄复用空槽，并阻止旧索引误命中新对象。这样可以避免
把压缩列表与稀疏槽两套语义混在一个 API 中。

## 示例

### 结构体数组

```c
typedef struct item {
	int ID;
	int Score;
} item;

xarray Items;
item Value = { 7, 95 };

if ( xrtArrayInit(&Items, sizeof(item)) ) {
	xrtArrayPush(&Items, &Value);
	item* pFirst = (item*)xrtArrayGet(&Items, 0);
	/* 使用 pFirst；结构性修改前不要长期保存该地址。 */
	xrtArrayUnit(&Items);
}
```

### 指针数组

```c
xptrarray Objects;
object* pObject = create_object();

if ( xrtPtrArrayInit(&Objects) ) {
	if ( !xrtPtrArrayPush(&Objects, pObject) ) {
		destroy_object(pObject);
	}
	/* 容器不拥有对象，释放顺序由调用方决定。 */
	for ( size_t i = 0; i < Objects.Count; i++ ) {
		destroy_object(xrtPtrArrayGet(&Objects, i));
	}
	xrtPtrArrayUnit(&Objects);
}
```
