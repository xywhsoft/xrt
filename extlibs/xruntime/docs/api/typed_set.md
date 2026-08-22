# 类型集合

`typed_set` 是拥有运行时类型值的唯一集合。启用宏为 `XRUNTIME_FEATURE_TYPED_SET`，依赖
`set` 与 `runtime_type`。

四类类型容器共享的所有权、失败原子性、回调重入和迭代失效规则见
[类型容器共同契约](typed_containers.md)。

```c
#include <xrt/typed_set.h>
```

## 定位与分层

`xtypedset` 复用 `xset` 的独立哈希条目和稳定插入顺序，同时用 `xrttype` 统一值生命周期、
比较和散列。查找、插入和删除平均复杂度为 `O(1)`：

- 等价值只保存一份，首次插入的值是后续查询返回的规范值；
- 每个元素地址稳定，其他插入、删除和桶扩容不会搬迁已有元素；
- 迭代顺序确定为首次插入顺序，删除后重新加入会移动到末尾；
- 普通 C 结构不带引用计数，对象身份由更高一层 `xrtobject` 提供；
- 底层 `xset` 的集合代数、事务插入和回调门禁直接复用，不另造哈希实现。

需要可重复值和连续随机访问时使用 `xtypedarray`，需要稀疏整数键和键排序时使用
`xtypedlist`。

## 类型契约

元素类型必须通过 `xrtTypeValidate`，占用非零存储，并且可复制、可比较、可散列。集合条目
不会搬迁，因此不要求 `XRT_TYPE_FLAG_RELOCATABLE`。类型描述由调用方持有且视为不可变，必须
比集合及其所属对象存活更久。

两个类型集合参与合并、集合代数或关系判断时，必须借用同一个 `xrttype` 描述地址。仅有相同
类型 ID 和 ABI 名还不足以证明生命周期回调及其私有状态可互换。XRT 内建类型和宿主注册
类型应使用进程期规范描述指针。

存入集合的值在删除前必须保持比较和散列意义不变。公开查询只返回 `const void*`，调用方不得
强制去除只读限定后修改规范值，否则桶位置和唯一性契约都会失效。

## 生命周期与容量

```c
xtypedset Values;
int64 Value = 7;

if ( !xrtTypedSetInit(&Values, xrtTypeInt64()) ||
	 !xrtTypedSetAdd(&Values, &Value) ) {
	return false;
}
xrtTypedSetUnit(&Values);
```

`xrtTypedSetInit/Unit` 管理调用方提供的结构，`xrtTypedSetCreate/Destroy` 管理堆结构。
`xrtTypedSetClear` 销毁全部值但保留桶数组，`Reserve` 预留容量，`Trim` 在突发负载后收缩桶数组。

## 唯一值操作

`xrtTypedSetGetOrAdd` 返回集合中的规范值，并通过可选 `new` 输出区分已有值和新插入值。
`xrtTypedSetAdd` 是只关心成功与否的便利层。新值先在未提交条目中初始化和复制，散列、相等性
与来源复核通过后才接入集合；分配、初始化或复制失败不会改变可见集合。

`Get` 和 `Has` 按等价值查询，缺失是正常结果且不设置错误。复制或查询来源可以是外部值，也
可以是同一集合中准确的规范值地址；部分覆盖集合结构、桶数组或元素条目的地址会在读取前被
拒绝。

`Remove` 销毁并删除规范值。`Take` 使用类型移动操作把值交给调用方已经初始化的同类型外部
输出，再删除条目；它支持不可重定位的拥有值，输出不得与集合拥有的任何内存相交。缺失值
返回 `false` 且不设置错误。

`At` 按插入顺序返回第 `index` 个规范值，并从距离更近的一端遍历，复杂度为
`O(min(index, count - index))`。它用于少量位置访问，不应在热循环中替代哈希查询。

## 迭代与集合代数

`xrtTypedSetIterBegin` 和 `xrtTypedSetIterRBegin` 分别按插入顺序正向、反向启动无分配迭代，
`IterNext` 返回只读规范值，`IterEnd` 提前结束。结构修改会使现有迭代器失效，下一次推进返回
空并设置 `XERR_STATE`；自然到达末尾不设置错误。

`Clone` 深复制集合。`Merge` 事务加入来源中的缺失值，保留目标已有规范值和原相对顺序；任一
分配或类型复制失败时目标完全不变。以下构造函数返回独立堆集合：

| API | 结果 |
| --- | --- |
| `xrtTypedSetUnion` | 并集，左侧顺序优先，右侧新增值随后加入。 |
| `xrtTypedSetIntersection` | 交集，保持左侧相对顺序。 |
| `xrtTypedSetDifference` | 左侧相对右侧的差集。 |
| `xrtTypedSetSymmetricDifference` | 只在一侧出现的值。 |

`IsSubset`、`IsSuperset` 支持严格关系开关，`IsDisjoint` 判断相离，`Equals` 判断相同值集合。

`xtypedsetiter` 是调用方持有的外置迭代状态；自然结束会自动释放借用状态，提前停止必须调用
`xrtTypedSetIterEnd`。

## API 索引

```c
bool xrtTypedSetInit(xtypedset* set, const xrttype* item_type);
xtypedset* xrtTypedSetCreate(const xrttype* item_type);
void xrtTypedSetUnit(xtypedset* set);
void xrtTypedSetDestroy(xtypedset* set);

const xrttype* xrtTypedSetItemType(const xtypedset* set);
size_t xrtTypedSetCount(const xtypedset* set);
size_t xrtTypedSetCapacity(const xtypedset* set);
bool xrtTypedSetClear(xtypedset* set);
bool xrtTypedSetReserve(xtypedset* set, size_t capacity);
bool xrtTypedSetTrim(xtypedset* set);

const void* xrtTypedSetGetOrAdd(
	xtypedset* set,
	const void* item,
	bool* new_item
);
bool xrtTypedSetAdd(xtypedset* set, const void* item);
const void* xrtTypedSetGet(const xtypedset* set, const void* item);
bool xrtTypedSetHas(const xtypedset* set, const void* item);
bool xrtTypedSetRemove(xtypedset* set, const void* item);
bool xrtTypedSetTake(xtypedset* set, const void* item, ptr value);
const void* xrtTypedSetAt(const xtypedset* set, size_t index);

bool xrtTypedSetIterBegin(xtypedset* set, xtypedsetiter* iterator);
bool xrtTypedSetIterRBegin(xtypedset* set, xtypedsetiter* iterator);
const void* xrtTypedSetIterNext(xtypedsetiter* iterator);
void xrtTypedSetIterEnd(xtypedsetiter* iterator);

bool xrtTypedSetMerge(xtypedset* target, const xtypedset* source);
xtypedset* xrtTypedSetClone(const xtypedset* set);
xtypedset* xrtTypedSetUnion(const xtypedset* left, const xtypedset* right);
xtypedset* xrtTypedSetIntersection(
	const xtypedset* left,
	const xtypedset* right
);
xtypedset* xrtTypedSetDifference(
	const xtypedset* left,
	const xtypedset* right
);
xtypedset* xrtTypedSetSymmetricDifference(
	const xtypedset* left,
	const xtypedset* right
);
bool xrtTypedSetIsSubset(
	const xtypedset* left,
	const xtypedset* right,
	bool proper
);
bool xrtTypedSetIsSuperset(
	const xtypedset* left,
	const xtypedset* right,
	bool proper
);
bool xrtTypedSetIsDisjoint(
	const xtypedset* left,
	const xtypedset* right
);
bool xrtTypedSetEquals(const xtypedset* left, const xtypedset* right);

bool xrtTypedSetTypeValidate(const xrttype* type);
const xrtinstanceops* xrtTypedSetInstanceOps(void);
```

## 对象负载

`xrtTypedSetInstanceOps()` 返回进程期稳定的对象负载操作表。泛型集合类型描述必须使用
`XRT_TYPE_SET`、引用布局、一个元素类型实参、准确的 `xtypedset` 负载大小和该操作表。
`xrtTypedSetTypeValidate` 一次验证完整契约。

实例追踪按每个规范值调用 `xrtTypeTraceValue`。集合模块本身不依赖对象系统；安装
`xrtObjectValueOps()`、对象图追踪和循环回收由对象层组合完成。

模块化和单头集成测试都构造了保存自身强引用的集合对象。重复插入必须仍只有一个规范槽位，
对象图必须观察到一条边，并在释放外部根后完整回收该自环。

## 错误、重入与线程

`xtypedseterror` 是错误域 `xrt.typed-set` 的稳定代码：

| 代码 | 常量 | 含义 |
| --- | --- | --- |
| 1 | `XTYPED_SET_ERROR_ARGUMENT` | 空参数、未初始化集合或非法别名。 |
| 2 | `XTYPED_SET_ERROR_TYPE` | 元素能力、对象描述或操作数类型不兼容。 |
| 3 | `XTYPED_SET_ERROR_RANGE` | 插入顺序位置越界。 |
| 4 | `XTYPED_SET_ERROR_OPERATION` | 存储或元素生命周期操作失败。 |
| 5 | `XTYPED_SET_ERROR_STATE` | 结构无效、回调重入、遍历中修改或追踪不完整。 |

下层 `xrt.type`、`xrt.set` 和用户复制错误保留为原因，OOM 的最终类别仍为 `XERR_MEMORY`。
正常缺失、关系为假和迭代结束不会制造错误。

元素的 `Init`、`Copy`、`Move`、`Drop`、`Compare`、`Hash` 和 `Trace` 回调期间，同一集合
的任何公开 API 都会以 `XERR_STATE` 拒绝；其他集合不受影响。同一集合的并发访问由调用方
同步，不同集合可以并行使用。

## 历史资产

本模块保留旧版 `lib/typed_container.h` 的类型拥有、规范值查询、克隆、合并和完整集合代数，
并复用新版 `xset` 已压实的稳定条目、插入顺序、75% 负载扩容、失败原子批量插入、回调重入
门禁和边界测试。

旧版重复的 `RefCount/HeapOwned`、所有者模式、`CloneEx`、`MoveToShared` 和值装箱便利层不再
进入普通容器。对象共享与装箱属于对象和值层，类型集合只承担一个清晰职责。
