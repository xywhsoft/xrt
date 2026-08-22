# 类型树

`typed_tree` 是以任意可比较运行时类型为键的有序字典。实现直接复用拥有式 AVL 树和固定对象池，
保持 `O(log n)` 查询、插入与删除，以及删除前稳定的节点地址；模块不复制 AVL 算法，也不依赖
`typed_array` 收集键和值。

```c
#include <xrt/typed_tree.h>
```

## 裁剪与分层

启用 `XRUNTIME_FEATURE_TYPED_TREE` 只依赖 `XRT_FEATURE_AVL_TREE` 和
`XRUNTIME_FEATURE_RUNTIME_TYPE`。需要连续键列表时，调用方可以通过零分配迭代器写入自行选择的数组，
因此有序字典不会强制带入类型数组或线程同步模块。

类型树不内建锁。跨线程共享同一棵树时由调用方提供同步；需要宿主对象、循环回收或弱引用时，
通过 `xrtTypedTreeInstanceOps` 把它组合为运行时对象负载。

## 生命周期与所有权

`xrtTypedTreeInit`/`Unit` 管理调用方结构，`xrtTypedTreeCreate`/`Destroy` 管理堆结构。键和值类型
描述均由树借用，必须覆盖树的完整生命周期。

- `Set` 复制键和值；已有键只替换值，不改变规范键。
- `SetTake` 复制键并移动外部值；成功后来源恢复为空值。
- `GetOrAdd` 在缺失时复制键并默认初始化值。
- `Take` 把值移动到已初始化的外部输出，再销毁键和空值。
- `Remove`、`Clear`、`Unit` 和 `Destroy` 销毁仍由树拥有的每一个键和值。

键决定树结构。`StoredKey`、边界查询和迭代返回的键只能只读借用，不得原地修改。`Get` 返回的
值槽允许修改。节点地址在对应键删除、清空或销毁前稳定；事务合并会整体替换存储，使旧借用全部
失效。

## 查询与迭代

`First`、`Last` 返回首尾项。`LowerBound` 返回第一项不小于查询键的条目，`UpperBound` 返回
第一项严格大于查询键的条目。未命中是正常结果，返回空指针且不设置错误。

`IterBegin`/`IterRBegin` 从首尾开始，`IterFrom` 从第一项不小于边界的位置升序开始，
`IterRFrom` 从第一项不大于边界的位置降序开始。迭代器不分配内存；结构修改后下一次推进返回空
并设置 `XERR_STATE`，自然结束不设置错误。

```c
xtypedtree Tree;
int32 Key = 20;
int64 Value = 200;
const void* StoredKey;

if ( !xrtTypedTreeInit(&Tree, xrtTypeInt32(), xrtTypeInt64()) ||
	 !xrtTypedTreeSet(&Tree, &Key, &Value) ) {
	return false;
}
Value = *(const int64*)xrtTypedTreeLowerBound(&Tree, &Key, &StoredKey);
xrtTypedTreeUnit(&Tree);
```

## 组合与失败

`Clone` 深复制全部键值。`Merge(target, source, false)` 保留目标冲突值，`true` 使用来源值替换。
合并先克隆目标并在独立工作树完成全部复制，成功后一次交换；分配、初始化或复制失败时，目标的
内容、节点地址、版本和所有权均保持不变。清理阶段产生的次级错误不会替换最初失败原因。

所有适用的类型回调期间，同一棵树的公开 API 都拒绝重入并设置 `XERR_STATE`。复制来源若指向
树内部，必须正好位于活动键槽或值槽边界；移动来源和输出必须完全位于树拥有的内存之外。

## 历史资产

本模块继承旧版 `dev/ver1/lib/typed_special.h` 的泛型键值所有权和有序树用途，以及
`dev/ver1/test/test_typed_special.c` 的字符串键、有序键收集和删除回归。新版直接建立在已经压实
的 `avl_tree`、固定对象池和运行时类型契约上，移除了旧版每树互斥锁、引用计数、节点内自指针、
重复长度字段和先销毁后复制的非原子替换路径。
