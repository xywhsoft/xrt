# 类型字典

`typed_dict` 是复制文本键并拥有运行时类型值的稳定字典。启用宏为
`XRUNTIME_FEATURE_TYPED_DICT`，依赖 `map` 与 `runtime_type`。

四类类型容器共享的所有权、失败原子性、回调重入和迭代失效规则见
[类型容器共同契约](typed_containers.md)。

```c
#include <xrt/typed_dict.h>
```

## 定位与类型契约

`xtypeddict` 复用 `xmap` 的哈希桶、紧凑条目和稳定插入顺序，查找、插入和删除平均为
`O(1)`。键使用 `xstrview`，由字典复制且不要求末尾零；空键和内嵌零合法，长度是唯一边界。
内部副本额外补零，方便文本互操作，但不会把 `strlen` 隐式带入底层热路径。

值类型必须通过 `xrtTypeValidate`、占用非零存储且可复制。独立条目不会搬迁值，因此不要求
`XRT_TYPE_FLAG_RELOCATABLE`。类型描述由调用方持有并视为不可变，必须比字典及其所属对象存活
更久。

两个字典参与合并或相等比较时必须借用同一个 `xrttype` 描述地址。相同类型 ID 不能替代准确
生命周期 ABI；XRT 内建类型和宿主注册类型应使用规范描述指针。

## 生命周期与容量

```c
xtypeddict Scores;
int64 Score = 95;

if ( !xrtTypedDictInit(&Scores, xrtTypeInt64()) ||
	 !xrtTypedDictSet(&Scores, XRT_STR_LITERAL("alice"), &Score) ) {
	return false;
}
xrtTypedDictUnit(&Scores);
```

`Init/Unit` 管理调用方结构，`Create/Destroy` 管理堆结构。`Clear` 销毁全部值但保留桶数组，
`Reserve` 预留容量，`Trim` 在突发负载后收缩桶数组。

## 键值操作

`xrtTypedDictGetOrAdd` 对已有键返回可写稳定值槽；缺失键先复制键并通过
`xrtTypeInitValue` 默认构造值，成功后才提交条目。可选 `new` 输出区分两条路径。

`xrtTypedDictSet` 失败原子地复制插入或替换。已有值通过类型复制操作直接替换，失败保持旧值；
新值在未提交条目中完成初始化和复制。来源可以是外部值或同一字典中准确的活动值槽，部分
覆盖字典元数据、桶或条目的来源会在修改前拒绝。

`xrtTypedDictSetTake` 失败原子地把外部已初始化值移入新键或已有键。成功后来源恢复为该类型的空值；参数、分配或类型移动失败时，来源和字典都保持不变。移动来源必须完全位于字典拥有的内存之外，避免替换或扩容使来源失效。

`Get`、`ConstGet` 和 `Has` 查询键，缺失不设置错误。`StoredKey` 返回字典内部规范键副本。
键和值地址在该键删除、字典清理或结束前保持稳定。

`Remove` 销毁并删除值。`Take` 使用类型移动操作把值交给调用方已经初始化的同类型外部输出，
再删除条目；它支持不可重定位的拥有值，输出不得触及字典拥有的任何内存。缺失键返回
`false` 且不设置错误。

`At/ConstAt` 按插入顺序从距离更近的一端返回位置值和可选键，复杂度为
`O(min(index, count - index))`。哈希查询仍是常规路径。

## 迭代、克隆与合并

`IterBegin` 和 `IterRBegin` 启动正反插入顺序迭代，`IterNext` 返回可写值槽和可选规范键，
`IterEnd` 提前结束。新键、删除、清空和事务合并会使旧迭代器失效；下一次推进返回空并设置
`XERR_STATE`。自然结束不设置错误。

`Clone` 深复制键和值。`Merge(target, source, replace)` 先构建完整工作副本：

- `replace == false` 保留目标冲突值，只加入缺失键；
- `replace == true` 使用来源值替换冲突键；
- 自合并是无操作成功；
- 任一 OOM、初始化或复制失败都保持目标、来源及其拥有关系不变；
- 提交会使目标旧键值借用和迭代器失效。

`Equals` 比较精确类型身份、键集合和值内容，要求值类型可比较。不可比较是
`XERR_UNSUPPORTED`，不会被误报成普通不相等。

`xtypeddictiter` 是调用方持有的外置迭代状态；自然结束会自动释放借用状态，提前停止必须调用
`xrtTypedDictIterEnd`。

## API 索引

```c
bool xrtTypedDictInit(xtypeddict* dict, const xrttype* item_type);
xtypeddict* xrtTypedDictCreate(const xrttype* item_type);
void xrtTypedDictUnit(xtypeddict* dict);
void xrtTypedDictDestroy(xtypeddict* dict);

const xrttype* xrtTypedDictItemType(const xtypeddict* dict);
size_t xrtTypedDictCount(const xtypeddict* dict);
size_t xrtTypedDictCapacity(const xtypeddict* dict);
bool xrtTypedDictClear(xtypeddict* dict);
bool xrtTypedDictReserve(xtypeddict* dict, size_t capacity);
bool xrtTypedDictTrim(xtypeddict* dict);

ptr xrtTypedDictGetOrAdd(xtypeddict* dict, xstrview key, bool* new_item);
bool xrtTypedDictSet(
	xtypeddict* dict,
	xstrview key,
	const void* item
);
bool xrtTypedDictSetTake(xtypeddict* dict, xstrview key, ptr item);
ptr xrtTypedDictGet(xtypeddict* dict, xstrview key);
const void* xrtTypedDictConstGet(const xtypeddict* dict, xstrview key);
bool xrtTypedDictHas(const xtypeddict* dict, xstrview key);
bool xrtTypedDictStoredKey(
	const xtypeddict* dict,
	xstrview key,
	xstrview* stored_key
);
bool xrtTypedDictRemove(xtypeddict* dict, xstrview key);
bool xrtTypedDictTake(xtypeddict* dict, xstrview key, ptr value);
ptr xrtTypedDictAt(xtypeddict* dict, size_t index, xstrview* key);
const void* xrtTypedDictConstAt(
	const xtypeddict* dict,
	size_t index,
	xstrview* key
);

bool xrtTypedDictIterBegin(xtypeddict* dict, xtypeddictiter* iterator);
bool xrtTypedDictIterRBegin(xtypeddict* dict, xtypeddictiter* iterator);
ptr xrtTypedDictIterNext(xtypeddictiter* iterator, xstrview* key);
void xrtTypedDictIterEnd(xtypeddictiter* iterator);

bool xrtTypedDictMerge(
	xtypeddict* target,
	const xtypeddict* source,
	bool replace
);
xtypeddict* xrtTypedDictClone(const xtypeddict* dict);
bool xrtTypedDictEquals(const xtypeddict* left, const xtypeddict* right);

bool xrtTypedDictTypeValidate(const xrttype* type);
const xrtinstanceops* xrtTypedDictInstanceOps(void);
```

## 对象负载

`xrtTypedDictInstanceOps()` 返回稳定对象负载操作表。对象字典描述使用 `XRT_TYPE_DICT`、引用
布局、一个值类型实参、准确 `xtypeddict` 负载大小和该操作表。实例追踪按每个值调用
`xrtTypeTraceValue`，因此可直接承载宿主对象强引用并参与循环回收。

字典模块本身不依赖对象系统。动态字段、值装箱和宿主对象策略建立在这一层之上，不反向污染
基础映射和普通 C 类型字典。

模块化和单头集成测试都构造了由两个不同键持有自身强引用的字典对象。对象图必须观察到两个
独立值槽和两条边，并在释放外部根后完整回收该自环。

## 错误、重入与线程

`xtypeddicterror` 是错误域 `xrt.typed-dict` 的稳定代码：

| 代码 | 常量 | 含义 |
| --- | --- | --- |
| 1 | `XTYPED_DICT_ERROR_ARGUMENT` | 空参数、未初始化字典、非法键视图或别名。 |
| 2 | `XTYPED_DICT_ERROR_TYPE` | 值能力、对象描述或操作数类型不兼容。 |
| 3 | `XTYPED_DICT_ERROR_RANGE` | 插入顺序位置越界。 |
| 4 | `XTYPED_DICT_ERROR_OPERATION` | 存储或值生命周期操作失败。 |
| 5 | `XTYPED_DICT_ERROR_STATE` | 结构无效、回调重入、遍历中修改或追踪不完整。 |

下层 `xrt.type`、`xrt.map` 和用户回调错误保留为原因，OOM 最终类别仍为 `XERR_MEMORY`。
值的 `Init`、`Copy`、`Move`、`Drop`、`Compare` 和 `Trace` 回调期间，同一字典的任何公开
API 都会以 `XERR_STATE` 拒绝；其他字典不受影响。多步克隆、合并、比较和追踪期间结构修改
会被拒绝。同一字典的并发访问由调用方同步。

## 历史资产

本模块保留旧版 `lib/typed_container.h` 的复制键、稳定值槽、类型生命周期、位置访问、Take、
Clone、Merge、Equals 和对象图语义；底层直接复用新版 `xmap` 已验证的二进制键复制、紧凑单次
条目分配、稳定插入顺序、75% 负载扩容、别名检查和回调门禁。

旧版重复的引用计数、所有者模式、`CloneEx`、`MoveToShared`、隐式 `strlen` 和值装箱入口不再
进入普通类型字典。文本视图、类型拥有、对象身份和动态值分别位于清晰层级。
