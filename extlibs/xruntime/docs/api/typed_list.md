# 类型列表

`typed_list` 是拥有运行时类型值的稀疏有序列表。启用宏为
`XRUNTIME_FEATURE_TYPED_LIST`，依赖 `int_map` 与 `runtime_type`。

四类类型容器共享的所有权、失败原子性、回调重入和迭代失效规则见
[类型容器共同契约](typed_containers.md)。

```c
#include <xrt/typed_list.h>
```

## 定位与分层

`xtypedlist` 使用 `int64` 键，按键升序保存元素，查找、插入和删除复杂度为
`O(log n)`。它适合稀疏宿主负载、稀疏编号、稳定句柄表以及需要有序键遍历的场景：

- 键不要求连续，可以为负数；
- 元素保存在 `xintmap` 的稳定节点中，插入其他键不会改变已有元素地址；
- 列表拥有每个值，生命周期统一经过 `xrtType*Value`；
- 普通 C 结构不带引用计数，也不依赖对象系统；
- 需要宿主对象身份、弱引用和循环回收时，再把它作为 `xrtobject` 负载。

连续、缓存友好的密集数据应使用 `xtypedarray`。两者不是同一容器的不同名字，也不会
为了统一表面 API 而牺牲各自的数据结构契约。

## 生命周期

```c
xtypedlist Values;
int64 Value = 7;

if ( !xrtTypedListInit(&Values, xrtTypeInt64()) ||
	 !xrtTypedListSet(&Values, 10, &Value) ) {
	return false;
}
xrtTypedListUnit(&Values);
```

`xrtTypedListInit/Unit` 管理调用方提供的结构，`xrtTypedListCreate/Destroy` 管理堆结构。
`xrtTypedListClear` 销毁所有值但保留节点池，`xrtTypedListTrim` 释放空闲池页并允许保留
指定数量，适合在突发负载后主动归还内存。

类型描述由调用方持有，必须比列表及其所属对象存活更久。初始化要求元素类型通过
`xrtTypeValidate`、`Size != 0` 且可复制。稳定节点不会整体搬迁元素，因此与类型数组不同，
列表不要求 `XRT_TYPE_FLAG_RELOCATABLE`。

## 键和值

`xrtTypedListSet` 复制设置指定键。已有键使用失败原子的类型复制替换；缺失键先在未提交
节点中初始化并复制，成功后才进入树。分配、初始化或复制失败均不改变可见键集合和已有值。

`xrtTypedListAppend` 在当前最大键之后插入：空列表从键 `0` 开始，否则使用
`max_key + 1`。最大键为 `INT64_MAX` 时以 `XERR_RANGE` 拒绝，不发生整数回绕。
删除最大键后，后续追加会重新使用新的最大键之后的位置；该 API 不维护隐藏的单调序号。

`xrtTypedListGet` 和 `xrtTypedListConstGet` 返回借用值槽，`xrtTypedListHas` 只查询键。
缺失键是正常结果，不设置错误。借用地址在该键被删除、列表结束或节点池被释放前保持稳定；
调用方不能释放该地址，也不能把它保存到列表生命周期之外。

`xrtTypedListAt` 和 `xrtTypedListConstAt` 按键顺序返回第 `index` 个值及其实际键。实现从
距离更近的一端开始，复杂度为 `O(min(index, count - index))`；越界是正常结果。按键查询仍应
优先使用 `Get`，不要在循环中把 `At` 当作随机访问数组。

复制来源可以是外部值，也可以是同一列表中另一个准确的活动值槽。任何只部分覆盖列表结构、
节点或值槽的来源都会在修改前拒绝，避免扩容、替换或释放期间形成悬空读取。

`xrtTypedListRemove` 销毁并删除值。`xrtTypedListTake` 把值移动到调用方已经初始化的同类型
外部输出，再删除节点；输出区间不得与列表拥有的任何内存相交。未找到键返回 `false` 且不
设置错误。

## 查找与组合

`xrtTypedListFind` 按键升序返回第一个相等值的键，`xrtTypedListContains` 是其便利层。
两者要求元素类型可比较；未找到不设置错误，不可比较则设置 `XERR_UNSUPPORTED`。

`xrtTypedListClone` 深复制全部键值。`xrtTypedListMerge` 接受同元素类型的来源：

- `replace == false` 时保留目标中的冲突键；
- `replace == true` 时使用来源值替换冲突键；
- 自合并是无操作成功；
- 实现先复制目标快照并在副本上完成合并，任一 OOM 或值复制失败都保留原目标。

`xrtTypedListEquals` 同时比较元素类型身份、键集合和值内容。不可比较元素会返回失败并设置
错误，而不是把“无法比较”误报为“不相等”。

## 迭代

外置迭代器不分配内存：

| API | 起点与方向 |
| --- | --- |
| `xrtTypedListIterBegin` | 最小键开始升序 |
| `xrtTypedListIterRBegin` | 最大键开始降序 |
| `xrtTypedListIterFrom` | 第一个不小于边界的键开始升序 |
| `xrtTypedListIterRFrom` | 第一个不大于边界的键开始降序 |
| `xrtTypedListIterNext` | 返回下一借用值和可选键 |
| `xrtTypedListIterEnd` | 提前结束并释放迭代借用状态 |

迭代期间可以读取键和值，也可以直接修改当前值槽内容。任何可能改变结构版本的操作都会让
现有外置迭代器失效，下一次 `IterNext` 返回空并设置状态错误；调用方必须结束该迭代器，不能
继续使用先前借用。自然到达末尾后迭代器自动结束；提前停止必须显式调用
`xrtTypedListIterEnd`。回调式对象图追踪使用更严格的访问门禁，期间结构和生命周期修改会被拒绝。

`xtypedlistiter` 是调用方持有的外置迭代状态，同一迭代器在 End 或自然结束前不能再次 Begin。

## API 索引

```c
bool xrtTypedListInit(xtypedlist* list, const xrttype* item_type);
xtypedlist* xrtTypedListCreate(const xrttype* item_type);
void xrtTypedListUnit(xtypedlist* list);
void xrtTypedListDestroy(xtypedlist* list);

const xrttype* xrtTypedListItemType(const xtypedlist* list);
size_t xrtTypedListCount(const xtypedlist* list);
bool xrtTypedListClear(xtypedlist* list);
size_t xrtTypedListTrim(xtypedlist* list, size_t retain_empty);

bool xrtTypedListSet(xtypedlist* list, int64 key, const void* item);
bool xrtTypedListAppend(xtypedlist* list, const void* item, int64* key);
ptr xrtTypedListGet(xtypedlist* list, int64 key);
const void* xrtTypedListConstGet(const xtypedlist* list, int64 key);
bool xrtTypedListHas(const xtypedlist* list, int64 key);
ptr xrtTypedListAt(xtypedlist* list, size_t index, int64* key);
const void* xrtTypedListConstAt(
	const xtypedlist* list,
	size_t index,
	int64* key
);
bool xrtTypedListRemove(xtypedlist* list, int64 key);
bool xrtTypedListTake(xtypedlist* list, int64 key, ptr value);

bool xrtTypedListFind(
	const xtypedlist* list,
	const void* item,
	int64* key
);
bool xrtTypedListContains(const xtypedlist* list, const void* item);
bool xrtTypedListMerge(
	xtypedlist* target,
	const xtypedlist* source,
	bool replace
);
xtypedlist* xrtTypedListClone(const xtypedlist* list);
bool xrtTypedListEquals(const xtypedlist* left, const xtypedlist* right);

bool xrtTypedListIterBegin(xtypedlist* list, xtypedlistiter* iterator);
bool xrtTypedListIterRBegin(xtypedlist* list, xtypedlistiter* iterator);
bool xrtTypedListIterFrom(
	xtypedlist* list,
	int64 key,
	xtypedlistiter* iterator
);
bool xrtTypedListIterRFrom(
	xtypedlist* list,
	int64 key,
	xtypedlistiter* iterator
);
ptr xrtTypedListIterNext(xtypedlistiter* iterator, int64* key);
void xrtTypedListIterEnd(xtypedlistiter* iterator);

bool xrtTypedListTypeValidate(const xrttype* type);
const xrtinstanceops* xrtTypedListInstanceOps(void);
```

## 对象负载

`xrtTypedListInstanceOps()` 返回进程期稳定的实例操作表。对象列表类型描述应满足：

```c
const xrttype* Arguments[] = { xrtTypeInt64() };
xrttype ListType = {
	.Id = xrtTypeId(XRT_STR_LITERAL("app.list<int64>")),
	.Kind = XRT_TYPE_LIST,
	.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
	.Name = XRT_STR_INIT("list<int64>"),
	.AbiName = XRT_STR_INIT("app.list<int64>"),
	.Size = sizeof(ptr),
	.Align = _Alignof(ptr),
	.InstanceSize = sizeof(xtypedlist),
	.InstanceAlign = _Alignof(xtypedlist),
	.Ops = xrtObjectValueOps(),
	.InstanceOps = xrtTypedListInstanceOps(),
	.ArgumentCount = 1u,
	.Arguments = Arguments
};
```

普通列表模块不依赖 `runtime_object`，因此描述中是否安装对象引用槽操作由上层决定。
`xrtTypedListTypeValidate` 检查列表类别、引用布局、唯一泛型实参、负载大小、对齐、准确实例
操作表和元素能力。实例追踪逐值调用 `xrtTypeTraceValue`，同一对象被多个槽拥有时会报告多条
独立强引用边。

模块化和单头集成测试都构造了保存两个自身强引用的稀疏列表对象。对象图必须观察到两个独立
槽位和两条边，并在释放外部根后完整回收该自环。

## 失败、重入与线程

`xtypedlisterror` 是错误域 `xrt.typed-list` 的稳定代码：

| 代码 | 常量 | 含义 |
| --- | --- | --- |
| 1 | `XTYPED_LIST_ERROR_ARGUMENT` | 空参数、未初始化列表或非法别名。 |
| 2 | `XTYPED_LIST_ERROR_TYPE` | 元素或对象列表类型不满足能力契约。 |
| 3 | `XTYPED_LIST_ERROR_KEY` | 追加键溢出。 |
| 4 | `XTYPED_LIST_ERROR_OPERATION` | 存储或元素生命周期操作失败。 |
| 5 | `XTYPED_LIST_ERROR_STATE` | 结构布局无效、迭代中修改或遍历不完整。 |

下层 `xrt.type`、`xrt.int-map` 或用户回调错误保留为原因，OOM 的最终种类仍为
`XERR_MEMORY`。正常的缺失键、查找未命中和迭代结束不会制造错误。

同一列表上的并发读写不是内建同步 API；多线程共享时由调用方加锁。值的 `Init`、`Copy`、
`Move`、`Drop`、`Compare` 和 `Trace` 回调期间，同一列表的任何公开 API 都会以
`XERR_STATE` 拒绝；其他列表不受影响。全局内存分配器仍遵守 XRT 核心模块的线程合同。

## 历史资产

本模块保留旧版 `lib/typed_container.h` 和 `lib/list.h` 中有通用价值的 `int64` 稀疏键、
有序遍历、稳定值地址、类型生命周期、克隆、合并和对象图追踪语义；底层平衡树、节点池、迭代
版本和 OOM 边界直接复用已经验证的 `xintmap`/AVL 实现。

旧版每个容器重复的 `RefCount/HeapOwned`、所有者模式、静默失败回调和隐藏对象旁路表不再保留。
普通 C 容器、类型事实和对象身份分别位于清晰层级，既可独立裁剪，也能在宿主对象层自然组合。
