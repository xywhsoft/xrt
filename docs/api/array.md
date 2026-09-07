# Array 与 PtrArray

`array` 提供固定元素大小的连续动态数组；`ptr_array` 在同一存储核心上提供指针类型友好的接口。两者都是本地容器，不隐式加锁，不接管元素内部资源。

## 裁剪与依赖

| 能力 | 宏 | 依赖 |
|---|---|---|
| 连续动态数组 | `XRT_FEATURE_ARRAY` | `core` |
| 指针数组 | `XRT_FEATURE_PTR_ARRAY` | `array` |

只启用 `XRT_FEATURE_ARRAY` 不会带入指针便利层。启用 `XRT_FEATURE_PTR_ARRAY` 时必须同时启用 `XRT_FEATURE_ARRAY`，公共头会在依赖不完整时拒绝编译。

## 错误与线程

**错误**：全部失败路径经 `xrtGetError()` 报告，本模块只使用四个通用错误种类：

| 错误 | 含义 | 典型触发 |
|---|---|---|
| `XERR_ARGUMENT` | 参数非法 | 空指针、`iItemSize == 0`、对齐非二次幂、来源未按元素边界对齐 |
| `XERR_STATE` | 结构状态非法 | 结构被外部破坏（`ItemSize/Alignment/Count/Capacity` 组合不自洽） |
| `XERR_RANGE` | 索引或区间越界 | `iIndex >= Count`、删除区间不完整 |
| 分配失败 | 容量增长分配失败 | 内存耗尽；原数据保持 |

**线程**：无内部锁。一个数组同时只能由一个执行流修改；跨线程共享读需外部同步或使用专门的并发容器。全部 API 可在任意线程调用（无 Worker 归属约束）。

**所有权**：容器只拥有连续存储区（`Allocation`），不拥有元素内部资源，也不释放指针元素指向的对象。`Data` 与 `Get/ConstGet/Add/InsertSpace` 返回的都是借用地址——任何可能改变容量、位置或顺序的操作（`Add`、`Insert*`、`Remove*`、`Pop`、`Reverse`、`Sort`、`Trim`）都可能使旧地址失效。

## 稳定契约

- 所有索引和插入位点统一从 `0` 开始，有效索引范围是 `[0, Count)`。
- 扩容采用几何增长（首次至少 8 个元素、此后约 1.5 倍），小数组不会预留固定 256 元素。
- 除明确说明外，失败是原子失败：`Data`、`Count`、`Capacity` 和已有元素保持不变。
- `Reserve` 只增长容量，`Resize` 改变元素数量（新增元素清零），`Trim` 才主动缩减容量。
- `Remove` 要求删除区间完整有效，不会静默截断到数组末尾。
- 排序使用 C `qsort`（不稳定），相等元素的原顺序不保证。
- 复制族（`Append`/`Insert`/`Set`）允许来源指向数组自身活动元素区（含跨插入位点），实现按移动后的偏移复制、不建临时副本。

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

结构公开是为了底层代码能直接读取连续视图和计数，不表示调用方可以修改结构不变量；`Resize` 之外的新增槽位**不做零填充**，写入式追加应使用 `Add`/`InsertSpace` 返回的指针。

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | — | 内联小缓冲（柔性数组起点） |
| `Allocation` | — | 外部分配块（超出内联时） |
| `ItemSize` | — | 单元素字节数 |
| `Count` | — | 当前元素数 |
| `Capacity` | — | 当前容量 |
| `Flags` | — | 内部状态位 |

### `xptrarray`

```c
typedef xarray xptrarray;
```

指针数组与普通数组共享完全相同的存储结构，其 `ItemSize` 固定为 `sizeof(ptr)`。需要底层通用操作时，可以直接使用 `xrtArray*` API；常见代码应优先使用 `xrtPtrArray*` API，避免手工传递指针地址。指针数组 API 会拒绝 `ItemSize` 不是 `sizeof(ptr)` 的数组（防止普通元素数组被误当指针数组操作）。

## 生命周期

### `xrtArrayInit`

初始化调用方持有的数组结构，使用默认 16 字节对齐。

```c
bool xrtArrayInit(xarray* pArray, size_t iItemSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输出 | 非空 | 调用方持有的结构；成功后被清零并填入 `ItemSize` 与默认 `Alignment` |
| `iItemSize` | 输入 | `> 0` | 单个元素字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 结构已初始化为空数组 | — |
| `false` | 参数非法 | `pArray` 指向的内存不被触碰；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pArray == NULL` 或 `iItemSize == 0`

#### 范例

[containers/array · 基础范例](../../examples/containers/array/main.c) · 以记录结构体为元素初始化

```c
if ( !xrtArrayInit(&tRecords, sizeof(examplerecord)) ) {
	return 1;
}
```

### `xrtArrayInitAligned`

初始化显式过对齐数组，元素大小必须是对齐值的倍数。

```c
bool xrtArrayInitAligned(xarray* pArray, size_t iItemSize, size_t iAlignment);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输出 | 非空 | 调用方持有的结构 |
| `iItemSize` | 输入 | `> 0` 且 `% iAlignment == 0` | 单个元素字节数 |
| `iAlignment` | 输入 | 二次幂 | 数据起始地址对齐值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 结构已初始化，`Alignment` 为指定值 | — |
| `false` | 参数非法 | 结构不被触碰；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pArray == NULL`、`iItemSize == 0`、`iAlignment` 非二次幂、或 `iItemSize % iAlignment != 0`

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · `int` 元素按 4 字节对齐

```c
if ( !xrtArrayInitAligned(&Array, sizeof(int),
	 sizeof(int)) ||
```


### `xrtArrayCreate`

在堆上创建使用默认对齐的空数组结构。

```c
xarray* xrtArrayCreate(size_t iItemSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iItemSize` | 输入 | `> 0` | 单个元素字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空指针 | 新建的堆上数组，配对 `xrtArrayDestroy()` 释放 | — |
| `NULL` | 参数非法或结构分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `iItemSize == 0`
- 分配失败 — 结构内存申请失败

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 查找族使用堆形态

```c
xarray* pArray = xrtArrayCreate(sizeof(int));
```

### `xrtArrayCreateAligned`

在堆上创建显式过对齐的空数组结构。

```c
xarray* xrtArrayCreateAligned(size_t iItemSize, size_t iAlignment);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iItemSize` | 输入 | `> 0` 且 `% iAlignment == 0` | 单个元素字节数 |
| `iAlignment` | 输入 | 二次幂 | 数据起始地址对齐值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空指针 | 新建的过对齐数组，配对 `xrtArrayDestroy()` 释放 | — |
| `NULL` | 对齐参数非法或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 对齐参数组合非法（同 `xrtArrayInitAligned`）
- 分配失败 — 结构内存申请失败

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 对齐堆形态

```c
xarray* pAligned = xrtArrayCreateAligned(sizeof(int),
	sizeof(int));
```

### `xrtArrayUnit`

释放数组持有的元素内存并把结构重置为零，不释放数组结构本身。

```c
void xrtArrayUnit(xarray* pArray);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 允许空指针 | 内嵌形态的收尾；成功后结构可重新 `Init` |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作；不释放元素内部资源 |

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 内嵌段收尾

```c
xrtArrayUnit(&Array);
```

### `xrtArrayDestroy`

释放数组持有的全部资源和数组结构本身。

```c
void xrtArrayDestroy(xarray* pArray);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入 | 允许空指针 | 堆形态（`Create`/`CreateAligned` 产物）的收尾 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作；不释放元素内部资源 |

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 堆形态收尾

```c
xrtArrayDestroy(pArray);
```

### `xrtArrayClear`

把 `Count` 设为零并保留容量。

```c
void xrtArrayClear(xarray* pArray);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空 | 活动区原有字节不再属于有效元素，也不会被析构或清零 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 结构状态非法时空操作并设置错误（经 `xrtGetError()` 可查） |

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · Clear 后直接核对 `Count`

```c
xrtArrayClear(&Array);
if ( Array.Count != 0u ) {
	return 9;
}
```

## 容量

### `xrtArrayReserve`

保证数组至少具有指定元素容量；容量已足够时是成功空操作。

```c
bool xrtArrayReserve(xarray* pArray, size_t iCapacity);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且状态自洽 | 成功扩容后 `Data` 可能移动，旧借用地址失效 |
| `iCapacity` | 输入 | — | 请求的最小容量；小于当前容量时不做任何事 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 容量已满足（原本满足或已增长） | — |
| `false` | 状态非法或增长失败 | 原存储与元素完全不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` — 结构不变量被破坏
- 分配失败 — 容量字节数溢出或分配失败

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 编辑段预容量

```c
!xrtArrayReserve(&Array, 8u) ) {
```

### `xrtArrayResize`

调整元素数量：缩小只改 `Count`；增长时新增元素全部清零并保留容量。

```c
bool xrtArrayResize(xarray* pArray, size_t iCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且状态自洽 | 缩小时被裁剪区域的字节保留原值（不属于有效元素） |
| `iCount` | 输入 | — | 新数量；可为 0 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 数量已设置；增长路径的新元素为零值 | — |
| `false` | 状态非法或增长失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` — 结构不变量被破坏
- 分配失败 — 增长分配失败

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 重设 3 项

```c
if ( !xrtArrayResize(&Array, 3u) || (Array.Count != 3u) ||
```

### `xrtArrayTrim`

将容量裁剪到当前元素数量；`Count == 0` 时直接释放存储块。

```c
bool xrtArrayTrim(xarray* pArray);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且状态自洽 | 成功后 `Capacity == Count`；`Data` 可能移动 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 容量已收缩（或原本相等） | — |
| `false` | 状态非法或收缩分配失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` — 结构不变量被破坏
- 分配失败 — 精确容量分配失败

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 收缩空闲容量

```c
!xrtArrayTrim(&Array) ) {
```

## 访问

### `xrtArrayGet`

返回指定 0 基索引处的可写元素地址。

```c
ptr xrtArrayGet(xarray* pArray, size_t iIndex);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入 | 非空且状态自洽 | 只读访问不改变数组 |
| `iIndex` | 输入 | `< Count` | 0 基索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 元素的可写借用地址；结构性修改后失效 | — |
| `NULL` | 索引越界或状态非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` — `iIndex >= Count`
- `XERR_STATE` — 结构不变量被破坏

#### 范例

[containers/array · 基础范例](../../examples/containers/array/main.c) · 遍历读取记录

```c
examplerecord* pRecord = (examplerecord*)xrtArrayGet(&tRecords, i);
```

### `xrtArrayConstGet`

返回指定 0 基索引处的只读元素地址。

```c
const void* xrtArrayConstGet(const xarray* pArray, size_t iIndex);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入 | 非空且状态自洽 | — |
| `iIndex` | 输入 | `< Count` | 0 基索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 元素的只读借用地址 | — |
| `NULL` | 索引越界或状态非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` — `iIndex >= Count`
- `XERR_STATE` — 结构不变量被破坏

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 换删后核对下标 1

```c
(*(int*)xrtArrayConstGet(&Array, 1u) != 30) ||
```

### `xrtArrayAdd`

在末尾增加未初始化元素，并返回第一个新增元素的地址。

```c
ptr xrtArrayAdd(xarray* pArray, size_t iCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且状态自洽 | 成功后 `Count += iCount`；扩容可能移动 `Data` |
| `iCount` | 输入 | `> 0` | 新增元素数；**零是参数错误** |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 第一个新增槽位的可写地址——**槽位未初始化，必须由调用方写入**（不做零填充） | — |
| `NULL` | 参数/状态非法或增长失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `iCount == 0`
- `XERR_STATE` — 结构不变量被破坏
- 分配失败 — 数量或容量字节数溢出、分配失败

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 写入式追加：直接解引用写值

```c
pSlot = (int*)xrtArrayAdd(&Array, 1u);
if ( pSlot == NULL ) {
	return 2;
}
*pSlot = i * 10;
```

### `xrtArrayInsertSpace`

在指定 0 基位点插入未初始化元素，并返回第一个新增元素的地址。

```c
ptr xrtArrayInsertSpace(xarray* pArray, size_t iIndex, size_t iCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且状态自洽 | 位点之后的旧元素整体右移 |
| `iIndex` | 输入 | `<= Count` | 插入位点；`Count` 即尾插 |
| `iCount` | 输入 | `> 0` | 新增元素数；**零是参数错误** |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 第一个新槽位的可写地址——**未初始化，须由调用方写入** | — |
| `NULL` | 参数/状态非法或增长失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `iCount == 0`
- `XERR_RANGE` — `iIndex > Count`
- `XERR_STATE` — 结构不变量被破坏
- 分配失败 — 溢出或分配失败

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 中段腾两位并写入

```c
pSlot = (int*)xrtArrayInsertSpace(&Array, 2u, 2u);
if ( (pSlot == NULL) || (Array.Count != 6u) ) {
	return 3;
}
pSlot[0] = 2;
pSlot[1] = 2;
```

## 复制编辑

### `xrtArrayPush`

复制一个元素到数组末尾。

```c
bool xrtArrayPush(xarray* pArray, const void* pItem);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且状态自洽 | 成功后 `Count += 1` |
| `pItem` | 输入 | 非空 | 元素来源；允许指向数组自身元素（同 `Append` 规则） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已复制追加 | — |
| `false` | 来源/状态非法或增长失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pItem == NULL`
- `XERR_STATE` — 结构不变量被破坏
- 分配失败 — 分配失败

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · Clear 前补一项

```c
if ( !xrtArrayPush(&Array, &Out) ) {
	return 9;
}
```

### `xrtArrayAppend`

复制一段连续元素到数组末尾，允许来源是数组自身的有效元素区。

```c
bool xrtArrayAppend(xarray* pArray, const void* pItems, size_t iCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且状态自洽 | 成功后 `Count += iCount` |
| `pItems` | 输入 | `iCount > 0` 时非空 | 来源可与数组存储重叠，但必须完整位于活动元素区内并按元素边界对齐 |
| `iCount` | 输入 | — | 元素数；`0` 为成功空操作（允许 `pItems == NULL`） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已复制追加（自引用来源按扩容后的偏移复制） | — |
| `false` | 来源/状态非法或增长失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `iCount > 0 && pItems == NULL`，或重叠来源未按元素边界对齐/未完整位于活动区
- `XERR_STATE` — 结构不变量被破坏
- 分配失败 — 溢出或分配失败

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 一次追加五项

```c
if ( !xrtArrayAppend(pArray, Values, 5u) ||
```

### `xrtArrayInsert`

在指定 0 基位点复制插入连续元素，允许来源是数组自身的有效元素区（含跨插入位点）。

```c
bool xrtArrayInsert(xarray* pArray, size_t iIndex, const void* pItems, size_t iCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且状态自洽 | 位点之后旧元素右移 |
| `iIndex` | 输入 | `<= Count` | 插入位点 |
| `pItems` | 输入 | `iCount > 0` 时非空 | 自引用来源（含跨越插入位点）按移动后偏移复制，无临时副本 |
| `iCount` | 输入 | — | 元素数；`0` 为成功空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已复制插入 | — |
| `false` | 区间/来源/状态非法或增长失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` — `iIndex > Count`
- `XERR_ARGUMENT` — 来源空指针或重叠来源不对齐/越出活动区
- `XERR_STATE` — 结构不变量被破坏
- 分配失败 — 溢出或分配失败

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 下标 2 插入一项

```c
if ( !xrtArrayInsert(&Array, 2u, Values, 1u) ||
```

### `xrtArraySet`

覆盖指定 0 基索引处的一个元素。

```c
bool xrtArraySet(xarray* pArray, size_t iIndex, const void* pItem);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且状态自洽 | 数量不变 |
| `iIndex` | 输入 | `< Count` | 目标索引 |
| `pItem` | 输入 | 非空 | 来源允许是数组内另一个元素 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已覆盖 | — |
| `false` | 索引/来源/状态非法 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` — `iIndex >= Count`
- `XERR_ARGUMENT` — `pItem == NULL`
- `XERR_STATE` — 结构不变量被破坏

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 覆盖下标 3

```c
!xrtArraySet(&Array, 3u, Values) ) {
```

## 删除与重排

### `xrtArrayRemove`

删除指定 0 基索引开始的精确元素区间（保序）。

```c
bool xrtArrayRemove(xarray* pArray, size_t iIndex, size_t iCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且状态自洽 | 区间后的元素左移 |
| `iIndex` | 输入 | `< Count` | 删除起点 |
| `iCount` | 输入 | `> 0` 且 `<= Count - iIndex` | 删除数量；**零是错误**（与 `Append` 的空操作口径不同） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已删除，`Count -= iCount` | — |
| `false` | 区间不完整或状态非法 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` — `iCount == 0`、`iIndex >= Count`、或 `iCount > Count - iIndex`
- `XERR_STATE` — 结构不变量被破坏

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 删去下标 2 的一项

```c
if ( !xrtArrayRemove(&Array, 2u, 1u) ) {
	return 5;
}
```

### `xrtArrayRemoveSwap`

用末尾元素覆盖指定元素并删除末尾；不保留元素顺序，O(1)。

```c
bool xrtArrayRemoveSwap(xarray* pArray, size_t iIndex);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且状态自洽 | 目标位置获得原末尾元素（目标即末尾时直接缩短） |
| `iIndex` | 输入 | `< Count` | 待删索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已删除，`Count -= 1` | — |
| `false` | 索引越界或状态非法 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` — `iIndex >= Count`
- `XERR_STATE` — 结构不变量被破坏

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 换删下标 1 后核对

```c
if ( !xrtArrayRemoveSwap(&Array, 1u) ||
```

### `xrtArrayPop`

删除末尾元素，并可将元素内容复制到输出地址。

```c
bool xrtArrayPop(xarray* pArray, ptr pItem);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且状态自洽 | `Count -= 1` |
| `pItem` | 输出 | 允许空指针；非空时**不得指向数组自身存储** | 接收被删元素副本；`NULL` 表示只删除 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已删除（并按需复制） | — |
| `false` | 数组为空、输出地址重叠或状态非法 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` — `Count == 0`
- `XERR_ARGUMENT` — `pItem` 指向数组自身存储（输出与源重叠）
- `XERR_STATE` — 结构不变量被破坏

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 取走末位并核对值

```c
if ( !xrtArrayPop(&Array, &Out) || (Out != 20) ) {
	return 8;
}
```

### `xrtArraySwap`

交换两个 0 基索引处的元素。

```c
bool xrtArraySwap(xarray* pArray, size_t iLeft, size_t iRight);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且状态自洽 | 两元素按字节交换，不分配堆内存 |
| `iLeft` | 输入 | `< Count` | 一侧索引 |
| `iRight` | 输入 | `< Count` | 另一侧索引；与 `iLeft` 相等为成功空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已交换 | — |
| `false` | 索引越界或状态非法 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` — 任一索引 `>= Count`
- `XERR_STATE` — 结构不变量被破坏

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 交换首尾

```c
if ( !xrtArraySwap(&Array, 0u, 4u) ||
```

### `xrtArrayReverse`

原地将元素顺序整体反转。

```c
bool xrtArrayReverse(xarray* pArray);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且状态自洽 | `Count <= 1` 时为成功空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已反转 | — |
| `false` | 状态非法 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` — 结构不变量被破坏

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 与 Swap 连用

```c
!xrtArrayReverse(&Array) ||
```

### `xrtArraySort`

使用不稳定快速排序原地排列元素。

```c
bool xrtArraySort(xarray* pArray, xarraycompare pCompare);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且状态自洽 | 存储不移动，仅元素重排 |
| `pCompare` | 输入 | 非空 | 比较器收两个**元素地址**；相等元素原顺序不保证 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已排序（`Count < 2` 为空操作） | — |
| `false` | 比较器缺失或状态非法 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pCompare == NULL`
- `XERR_STATE` — 结构不变量被破坏

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 排序后供查找族使用

```c
if ( !xrtArrayAppend(pArray, Values, 5u) ||
	!xrtArraySort(pArray, exampleCompareInt) ||
```

## 查找

### `xrtArrayFind`

按元素字节线性查找第一个完全相同的元素。

```c
size_t xrtArrayFind(const xarray* pArray, const void* pItem);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入 | 非空且状态自洽 | 逐元素 `memcmp` 等值比较，与是否有序无关 |
| `pItem` | 输入 | 非空 | 待查元素的字节模式；结构体填充字节不稳定时应改用 `FindBy` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `[0, Count)` | 第一个匹配的索引 | — |
| `XRT_NPOS` | 无匹配或参数/状态非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pItem == NULL`
- `XERR_STATE` — 结构不变量被破坏

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 排序后命中下标 2

```c
(xrtArrayFind(pArray, &Values[0]) != 2u) ||
```

### `xrtArrayFindBy`

使用比较器线性查找第一个匹配元素，比较器第一个参数是 key。

```c
size_t xrtArrayFindBy(const xarray* pArray, const void* pKey, xarraycompare pCompare);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入 | 非空且状态自洽 | — |
| `pKey` | 输入 | 非空 | 交给比较器的键 |
| `pCompare` | 输入 | 非空 | 谓词：收 `(pKey, 元素地址)`，返回 0 表示匹配 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `[0, Count)` | 第一个 `pCompare` 判等的位置 | — |
| `XRT_NPOS` | 无匹配或参数/状态非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pKey` 或 `pCompare` 为空
- `XERR_STATE` — 结构不变量被破坏

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 等值谓词下与 `Find` 一致

```c
if ( xrtArrayFindBy(pArray, &Values[1],
		exampleCompareInt) != 0u ) {
```

### `xrtArrayBSearch`

在已按同一比较器排序的数组中二分查找元素。

```c
size_t xrtArrayBSearch(const xarray* pArray, const void* pKey, xarraycompare pCompare);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入 | 非空且状态自洽 | **前置条件**：已按 `pCompare` 升序排序，否则结果未定义 |
| `pKey` | 输入 | 非空 | 查找键 |
| `pCompare` | 输入 | 非空 | 与排序时相同的比较器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `[0, Count)` | 匹配位置（重复元素时返回其中之一） | — |
| `XRT_NPOS` | 无匹配（含 `Count == 0`）或参数/状态非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pKey` 或 `pCompare` 为空
- `XERR_STATE` — 结构不变量被破坏

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 排序后二分定位 3

```c
(xrtArrayBSearch(pArray, &Values[0],
	exampleCompareInt) != 2u) ) {
```

## 指针数组：生命周期

指针数组接口在下层数组之上增加两道防线：空指针参数报 `XERR_ARGUMENT`；`ItemSize != sizeof(ptr)` 的数组（被普通元素 API 初始化）报 `XERR_STATE`。除 `Pop` 要求出参非空、`Find` 收指针值本身外，其余契约与下层数组一致。销毁、清空、删除都不会释放指针指向的对象。

### `xrtPtrArrayInit`

初始化一个不拥有所存指针目标的空指针数组。

```c
bool xrtPtrArrayInit(xptrarray* pArray);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输出 | 非空 | 元素大小固定为 `sizeof(ptr)`、默认对齐 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化为空数组 | — |
| `false` | `pArray == NULL` | 结构不被触碰；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pArray == NULL`

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 内嵌形态配对

```c
if ( !xrtPtrArrayInit(&Embedded) ) {
	return 26;
}
```

### `xrtPtrArrayCreate`

在堆上创建一个不拥有所存指针目标的空指针数组。

```c
xptrarray* xrtPtrArrayCreate(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 元素大小固定为 `sizeof(ptr)` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空指针 | 新数组，配对 `xrtPtrArrayDestroy()` 释放 | — |
| `NULL` | 结构分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 分配失败 — 结构内存申请失败

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 指针段起始

```c
xptrarray* pList = xrtPtrArrayCreate();
```

### `xrtPtrArrayUnit`

释放指针存储区（不释放各指针指向的对象），并把结构重置为零。

```c
void xrtPtrArrayUnit(xptrarray* pArray);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 允许空指针 | 内嵌形态收尾 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作；**不释放元素指针指向的对象** |

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 内嵌配对收尾

```c
xrtPtrArrayUnit(&Embedded);
```

### `xrtPtrArrayDestroy`

释放指针数组结构及其存储区，不释放各指针指向的对象。

```c
void xrtPtrArrayDestroy(xptrarray* pArray);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入 | 允许空指针 | 堆形态收尾 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作；不释放元素指针指向的对象 |

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 指针段收尾

```c
xrtPtrArrayDestroy(pList);
```

### `xrtPtrArrayClear`

清空指针数组但保留容量。

```c
void xrtPtrArrayClear(xptrarray* pArray);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且 `ItemSize == sizeof(ptr)` | 只清 `Count`，槽位旧值不复位 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 类型/状态非法时空操作并设置错误（经 `xrtGetError()` 可查） |

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 清空后核对

```c
xrtPtrArrayClear(pList);
if ( pList->Count != 0u ) {
	return 25;
}
```

### `xrtPtrArrayReserve`

保证指针数组至少具有指定容量；委托 `xrtArrayReserve`。

```c
bool xrtPtrArrayReserve(xptrarray* pArray, size_t iCapacity);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且 `ItemSize == sizeof(ptr)` | 成功扩容后 `Data` 视图可能移动 |
| `iCapacity` | 输入 | — | 请求的最小容量（指针槽位数） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 容量已满足 | — |
| `false` | 类型/状态非法或分配失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pArray == NULL`
- `XERR_STATE` — `ItemSize != sizeof(ptr)` 或结构不自洽
- 分配失败 — 分配失败

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 预留 8 槽

```c
if ( (pList == NULL) || !xrtPtrArrayReserve(pList, 8u) ) {
```

### `xrtPtrArrayResize`

调整指针数量，新增位置全部设置为空指针；委托 `xrtArrayResize`。

```c
bool xrtPtrArrayResize(xptrarray* pArray, size_t iCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且 `ItemSize == sizeof(ptr)` | 缩小只改 `Count`；增长清零新增槽 |
| `iCount` | 输入 | — | 新数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 数量已设置；新增槽为 `NULL` | — |
| `false` | 类型/状态非法或分配失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pArray == NULL`
- `XERR_STATE` — 类型不符或结构不自洽
- 分配失败 — 分配失败

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 与 Trim 连用

```c
if ( !xrtPtrArrayResize(pList, 8u) ||
	!xrtPtrArrayTrim(pList) ) {
```

### `xrtPtrArrayTrim`

将容量裁剪到当前指针数量；委托 `xrtArrayTrim`。

```c
bool xrtPtrArrayTrim(xptrarray* pArray);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且 `ItemSize == sizeof(ptr)` | `Count == 0` 时释放存储块 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 容量已收缩 | — |
| `false` | 类型/状态非法或分配失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pArray == NULL`
- `XERR_STATE` — 类型不符或结构不自洽
- 分配失败 — 分配失败

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 见 `xrtPtrArrayResize` 范例

```c
!xrtPtrArrayTrim(pList) ) {
```

## 指针数组：访问与编辑

### `xrtPtrArrayData`

返回可直接遍历的可写指针槽位视图。

```c
ptr* xrtPtrArrayData(xptrarray* pArray);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入 | 非空且 `ItemSize == sizeof(ptr)` | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | `Count` 个连续 `ptr` 槽的可写借用地址；结构性修改后失效 | — |
| `NULL` | 类型/状态非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pArray == NULL`
- `XERR_STATE` — 类型不符或结构不自洽

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 非空校验

```c
if ( (xrtPtrArrayData(pList) == NULL) ||
```

### `xrtPtrArrayConstData`

返回可直接遍历的只读指针槽位视图。

```c
ptr const* xrtPtrArrayConstData(const xptrarray* pArray);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入 | 非空且 `ItemSize == sizeof(ptr)` | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 只读槽位视图；结构性修改后失效 | — |
| `NULL` | 类型/状态非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pArray == NULL`
- `XERR_STATE` — 类型不符或结构不自洽

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 读首槽

```c
(xrtPtrArrayConstData(pList)[0] != (ptr)5) ) {
```

### `xrtPtrArrayGet`

返回指定 0 基索引处的指针值。

```c
ptr xrtPtrArrayGet(const xptrarray* pArray, size_t iIndex);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入 | 非空且 `ItemSize == sizeof(ptr)` | — |
| `iIndex` | 输入 | `< Count` | 0 基索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 槽位指针值 | 该位置存的 `ptr` | — |
| `NULL` | 索引越界、类型/状态非法，**或槽位本身存的就是 `NULL`** | 错误经 `xrtGetError()` 报告；无错误时即空槽位 |

#### 错误

- `XERR_RANGE` — `iIndex >= Count`
- `XERR_ARGUMENT` — `pArray == NULL`
- `XERR_STATE` — 类型不符或结构不自洽

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 核对下标 2

```c
(xrtPtrArrayGet(pList, 2u) != (ptr)20) ||
```

### `xrtPtrArraySet`

覆盖指定 0 基索引处的指针值；委托 `xrtArraySet`。

```c
bool xrtPtrArraySet(xptrarray* pArray, size_t iIndex, ptr pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且 `ItemSize == sizeof(ptr)` | — |
| `iIndex` | 输入 | `< Count` | 目标索引 |
| `pValue` | 输入 | — | 新指针值；`NULL` 合法（清空槽位） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已覆盖 | — |
| `false` | 索引/类型/状态非法 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` — `iIndex >= Count`
- `XERR_ARGUMENT` — `pArray == NULL`
- `XERR_STATE` — 类型不符或结构不自洽

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 改写下标 2

```c
!xrtPtrArraySet(pList, 2u, (ptr)25) ) {
```

### `xrtPtrArrayPush`

向末尾追加一个指针值；委托 `xrtArrayPush`。

```c
bool xrtPtrArrayPush(xptrarray* pArray, ptr pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且 `ItemSize == sizeof(ptr)` | 成功后 `Count += 1` |
| `pValue` | 输入 | — | 指针值本身（非取地址） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已追加 | — |
| `false` | 类型/状态非法或分配失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pArray == NULL`
- `XERR_STATE` — 类型不符或结构不自洽
- 分配失败 — 分配失败

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 连续压栈

```c
if ( !xrtPtrArrayPush(pList, (ptr)30) ||
	!xrtPtrArrayPush(pList, (ptr)10) ||
```

### `xrtPtrArrayAppend`

向末尾复制追加一段连续指针；委托 `xrtArrayAppend`。

```c
bool xrtPtrArrayAppend(xptrarray* pArray, ptr const* pValues, size_t iCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且 `ItemSize == sizeof(ptr)` | — |
| `pValues` | 输入 | `iCount > 0` 时非空 | 连续指针数组来源；允许指向数组自身槽区 |
| `iCount` | 输入 | — | 指针数；`0` 为成功空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已复制追加 | — |
| `false` | 来源/类型/状态非法或分配失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 来源空指针或 `pArray == NULL`
- `XERR_STATE` — 类型不符或结构不自洽
- 分配失败 — 分配失败

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 批量两项

```c
if ( !xrtPtrArrayAppend(pList, Batch, 2u) ||
```

### `xrtPtrArrayInsert`

在指定位点插入一个指针值；委托 `xrtArrayInsert`。

```c
bool xrtPtrArrayInsert(xptrarray* pArray, size_t iIndex, ptr pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且 `ItemSize == sizeof(ptr)` | — |
| `iIndex` | 输入 | `<= Count` | 插入位点 |
| `pValue` | 输入 | — | 指针值本身 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已插入 | — |
| `false` | 区间/类型/状态非法或分配失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` — `iIndex > Count`
- `XERR_ARGUMENT` — `pArray == NULL`
- `XERR_STATE` — 类型不符或结构不自洽
- 分配失败 — 分配失败

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 下标 1 插 12

```c
!xrtPtrArrayInsert(pList, 1u, (ptr)12) ||
```

### `xrtPtrArrayInsertMany`

在指定位点复制插入一段连续指针；委托 `xrtArrayInsert`。

```c
bool xrtPtrArrayInsertMany(xptrarray* pArray, size_t iIndex, ptr const* pValues, size_t iCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且 `ItemSize == sizeof(ptr)` | — |
| `iIndex` | 输入 | `<= Count` | 插入位点 |
| `pValues` | 输入 | `iCount > 0` 时非空 | 连续指针来源 |
| `iCount` | 输入 | — | 指针数；`0` 为成功空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已复制插入 | — |
| `false` | 区间/来源/类型/状态非法或分配失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` — `iIndex > Count`
- `XERR_ARGUMENT` — 来源空指针或 `pArray == NULL`
- `XERR_STATE` — 类型不符或结构不自洽
- 分配失败 — 分配失败

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 下标 1 批量两项

```c
!xrtPtrArrayInsertMany(pList, 1u, Batch, 2u) ) {
```

### `xrtPtrArrayRemove`

删除指定区间（保序）；委托 `xrtArrayRemove`。

```c
bool xrtPtrArrayRemove(xptrarray* pArray, size_t iIndex, size_t iCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且 `ItemSize == sizeof(ptr)` | 被删指针指向的对象不受影响 |
| `iIndex` | 输入 | `< Count` | 删除起点 |
| `iCount` | 输入 | `> 0` 且 `<= Count - iIndex` | 删除数量；零是错误 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已删除 | — |
| `false` | 区间/类型/状态非法 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` — 区间不完整（含 `iCount == 0`）
- `XERR_ARGUMENT` — `pArray == NULL`
- `XERR_STATE` — 类型不符或结构不自洽

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 去掉头部两项

```c
if ( !xrtPtrArrayRemove(pList, 0u, 2u) ||
```

### `xrtPtrArrayRemoveSwap`

用末尾指针覆盖指定位置并删除末尾；委托 `xrtArrayRemoveSwap`。

```c
bool xrtPtrArrayRemoveSwap(xptrarray* pArray, size_t iIndex);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且 `ItemSize == sizeof(ptr)` | 顺序不保留 |
| `iIndex` | 输入 | `< Count` | 待删索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已删除 | — |
| `false` | 索引/类型/状态非法 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` — `iIndex >= Count`
- `XERR_ARGUMENT` — `pArray == NULL`
- `XERR_STATE` — 类型不符或结构不自洽

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 换删头部

```c
!xrtPtrArrayRemoveSwap(pList, 0u) ||
```

### `xrtPtrArrayPop`

删除末尾指针并写入输出参数——**输出参数不能为空**（与 `xrtArrayPop` 可空不同）。

```c
bool xrtPtrArrayPop(xptrarray* pArray, ptr* pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且 `ItemSize == sizeof(ptr)` | `Count -= 1` |
| `pValue` | 输出 | 非空 | 接收被删指针值；不得指向数组自身槽区 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已删除并写出 | — |
| `false` | 出参为空、数组为空或类型/状态非法 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pValue == NULL` 或 `pArray == NULL`
- `XERR_RANGE` — `Count == 0`
- `XERR_STATE` — 类型不符或结构不自洽

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 排序后取最大值

```c
if ( !xrtPtrArrayPop(pList, &Slot) ||
	(Slot != (ptr)30) ) {
```

### `xrtPtrArraySwap`

交换两个 0 基索引处的指针值。

```c
bool xrtPtrArraySwap(xptrarray* pArray, size_t iLeft, size_t iRight);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且 `ItemSize == sizeof(ptr)` | — |
| `iLeft` | 输入 | `< Count` | 一侧索引 |
| `iRight` | 输入 | `< Count` | 另一侧索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已交换 | — |
| `false` | 索引/类型/状态非法 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` — 任一索引越界
- `XERR_ARGUMENT` — `pArray == NULL`
- `XERR_STATE` — 类型不符或结构不自洽

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 交换 0 与 5

```c
if ( !xrtPtrArraySwap(pList, 0u, 5u) ||
```

### `xrtPtrArrayReverse`

原地反转指针顺序。

```c
bool xrtPtrArrayReverse(xptrarray* pArray);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且 `ItemSize == sizeof(ptr)` | `Count <= 1` 为成功空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已反转 | — |
| `false` | 类型/状态非法 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pArray == NULL`
- `XERR_STATE` — 类型不符或结构不自洽

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 反转后核对首槽

```c
!xrtPtrArrayReverse(pList) ||
```

### `xrtPtrArraySort`

按比较器原地排序；比较器收到的是**槽位地址**（`ptr const*`），解引用后才是指针值。

```c
bool xrtPtrArraySort(xptrarray* pArray, xarraycompare pCompare);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入输出 | 非空且 `ItemSize == sizeof(ptr)` | — |
| `pCompare` | 输入 | 非空 | 两个参数都是槽位地址；按指针值排序须在比较器内解引用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已排序 | — |
| `false` | 比较器/类型/状态非法 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pCompare == NULL` 或 `pArray == NULL`
- `XERR_STATE` — 类型不符或结构不自洽

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 按解引用值排序后核两端

```c
if ( !xrtPtrArraySort(pList, exampleCompareValue) ||
	(xrtPtrArrayGet(pList, 0u) != (ptr)5) ||
```

### `xrtPtrArrayFind`

按**指针值**查找第一个匹配槽位。

```c
size_t xrtPtrArrayFind(const xptrarray* pArray, const void* pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入 | 非空且 `ItemSize == sizeof(ptr)` | 线性等值扫描，包括查找 `NULL` |
| `pValue` | 输入 | — | **关键字指针值本身**（装箱传入），不是指向它的地址——传地址会搜索那个栈地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `[0, Count)` | 第一个存有该指针值的槽位 | — |
| `XRT_NPOS` | 无匹配或类型/状态非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pArray == NULL`
- `XERR_STATE` — 类型不符或结构不自洽

#### 范例

[containers/array_tour](../../examples/containers/array_tour/main.c) · 装箱传值查 15

```c
if ( xrtPtrArrayFind(pList, (const void*)(ptr)15) != 2u ) {
```

## 与其他容器的关系

稳定槽位和空洞复用不是连续指针数组的职责。需要稳定句柄时，使用 `slot_map`——它以代际句柄复用空槽，并阻止旧索引误命中新对象。这样可以避免把压缩列表与稀疏槽两套语义混在一个 API 中。
