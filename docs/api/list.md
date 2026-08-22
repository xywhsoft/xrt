# 侵入式链表 API

`list` 提供无分配、无锁的双向侵入式链表。启用宏为 `XRT_FEATURE_LIST`，只依赖
不可裁剪的核心模块。

```c
#include <xrt/list.h>
```

## 定位

链表不拥有节点，也不决定对象类型和分配方式。用户把 `xlistnode` 嵌入自己的对象，
对象可以来自栈、全局存储、XRT 堆或任意内存池。一个对象需要同时进入多个链表时，
嵌入多个独立节点即可。

每个节点保存 `Prev`、`Next` 和只读的 `Owner`。额外的所属链表指针让插入、删除、
移动和归属检查保持 `O(1)`，同时能够确定性拒绝重复插入和跨链表删除。链表不内建
同步；多个线程读写同一链表时由调用方加锁。

连续数据使用 `array`，FIFO/LIFO 工作负载使用 `queue` / `stack`，拥有运行时类型值的
稀疏有序容器使用 `typed_list`。本模块不重复这些高层容器的分配和类型生命周期逻辑。

## 类型与初始化

```c
typedef struct xlistnode {
	struct xlistnode* Prev;
	struct xlistnode* Next;
	xlist* Owner;
} xlistnode;

typedef struct xlist {
	xlistnode* First;
	xlistnode* Last;
	size_t Count;
	uint64 Version;
	uint32 State;
} xlist;
```

`XRT_LIST_INIT` 和 `XRT_LIST_NODE_INIT` 用于静态或聚合初始化；动态对象使用
`xrtListInit` 和 `xrtListNodeInit`。初始化函数只用于新的存储，不能覆盖仍然持有节点
的链表或仍然已连接的节点。`xrtListReady` 判断链表是否初始化，`xrtListValidate`
以 `O(n)` 完整检查端点、所有权、双向链接和计数。

链表字段公开是为了零成本检查和调试，但调用方只能读取。直接改写 `First`、`Last`、
`Count`、`Version`、`State` 或活动节点的链接字段会破坏契约。

`XRT_CONTAINER_OF(node, type, member)` 从任意位置嵌入的节点恢复对象，不要求节点是
结构的第一个成员：

```c
typedef struct job {
	int Priority;
	xlistnode Ready;
} job;

job* pJob = XRT_CONTAINER_OF(pNode, job, Ready);
```

## 查询

- `xrtListEmpty` / `xrtListCount` 返回空状态和节点数。
- `xrtListFirst` / `xrtListLast` 返回借用端点，空链表返回 `NULL`。
- `xrtListPrev` / `xrtListNext` 返回借用相邻节点。
- `xrtListOwner` 返回节点所属链表，未连接节点返回 `NULL`。
- `xrtListContains` 判断节点是否属于指定链表。
- `xrtListLinked` 判断节点是否属于任意链表。

借用节点在被移除、弹出或清空后仍由用户拥有，但其 `Prev`、`Next` 和 `Owner` 会全部
恢复为 `NULL`，可以安全插入其他链表。

## 结构操作

`xrtListPushFront` / `xrtListPushBack` 在首尾插入未连接节点。
`xrtListInsertBefore` / `xrtListInsertAfter` 要求参考节点属于目标链表；参考位置不能用
`NULL` 表示，首尾便利路径由 Push 函数明确表达。

`xrtListRemove` 只分离节点，不释放对象。`xrtListPopFront` / `xrtListPopBack` 分离并
返回端点，空链表返回 `NULL` 且不设置错误。`xrtListMoveFront` / `xrtListMoveBack`
把已有节点移动到首尾，不改变计数。`xrtListClear` 先完整验证链表，再分离所有节点；
验证失败时不会部分清空。

所有成功的结构变化都会更新 `Version`。节点数使用 `size_t`，达到 `SIZE_MAX` 后新增
节点以 `XERR_RANGE` 拒绝，不发生回绕。

## 安全迭代

`xlistiter` 是调用方持有的外置状态，不分配内存：

```c
xlistiter Iterator;
xlistnode* pNode;

if ( !xrtListIterBegin(&List, &Iterator) ) {
	return false;
}
while ( (pNode = xrtListIterNext(&Iterator)) != NULL ) {
	job* pJob = XRT_CONTAINER_OF(pNode, job, Ready);

	if ( pJob->Priority < 0 ) {
		if ( !xrtListIterRemove(&Iterator) ) {
			return false;
		}
	}
}
```

`xrtListIterBegin` 正向迭代，`xrtListIterRBegin` 反向迭代。自然结束不设置错误。
`xrtListIterRemove` 只能删除最近一次 `Next` 返回的当前节点，并同步迭代版本；连续删除
两次会失败。通过其他 API 修改链表会使现有迭代器失效，下一次 Next/Remove 以
`XLIST_ERROR_MODIFIED` 拒绝。`xrtListIterEnd` 用于提前结束，允许传入 `NULL`。

## 错误

错误域为 `xrt.list`：

| 代码 | 常量 | 含义 |
| --- | --- | --- |
| 1 | `XLIST_ERROR_ARGUMENT` | 空链表、节点或迭代器参数。 |
| 2 | `XLIST_ERROR_STATE` | 未初始化、节点归属错误或链接不一致。 |
| 3 | `XLIST_ERROR_RANGE` | 节点计数将溢出。 |
| 4 | `XLIST_ERROR_MODIFIED` | 迭代期间发生外部结构修改。 |

查询未命中、空链表弹出和迭代自然结束是正常结果，不制造错误。成功操作不会隐式清除
调用方先前保存的错误。

## API 索引

```c
void xrtListInit(xlist* list);
void xrtListNodeInit(xlistnode* node);
bool xrtListReady(const xlist* list);
bool xrtListValidate(const xlist* list);
bool xrtListEmpty(const xlist* list);
size_t xrtListCount(const xlist* list);
xlistnode* xrtListFirst(const xlist* list);
xlistnode* xrtListLast(const xlist* list);
xlistnode* xrtListPrev(const xlistnode* node);
xlistnode* xrtListNext(const xlistnode* node);
xlist* xrtListOwner(const xlistnode* node);
bool xrtListContains(const xlist* list, const xlistnode* node);
bool xrtListLinked(const xlistnode* node);

bool xrtListPushFront(xlist* list, xlistnode* node);
bool xrtListPushBack(xlist* list, xlistnode* node);
bool xrtListInsertBefore(xlist* list, xlistnode* position, xlistnode* node);
bool xrtListInsertAfter(xlist* list, xlistnode* position, xlistnode* node);
bool xrtListRemove(xlist* list, xlistnode* node);
xlistnode* xrtListPopFront(xlist* list);
xlistnode* xrtListPopBack(xlist* list);
bool xrtListMoveFront(xlist* list, xlistnode* node);
bool xrtListMoveBack(xlist* list, xlistnode* node);
bool xrtListClear(xlist* list);

bool xrtListIterBegin(xlist* list, xlistiter* iterator);
bool xrtListIterRBegin(xlist* list, xlistiter* iterator);
xlistnode* xrtListIterNext(xlistiter* iterator);
bool xrtListIterRemove(xlistiter* iterator);
void xrtListIterEnd(xlistiter* iterator);
```

完整示例位于 `examples/containers/list/main.c`。

## 旧版资产决策

新版保留旧 `LList Base` 的侵入节点、双向遍历、首尾操作、任意内存来源和多链接对象
能力，并补齐节点所有权、`size_t` 计数、结构验证、安全迭代和结构化错误。旧宏缺少
`do/while` 边界，重复插入、外来参考和跨链表删除会静默损坏结构，因此不保留原名称和
布局。

旧自动分配 `LList` 把节点头与未对齐的匿名字节负载拼接，并重复承担固定池、队列、栈和
类型容器职责。该层不再恢复：低层链接使用本模块，分配复用使用 Pool，高层数据结构使用
Array/Queue/Stack/Typed List，避免第二套节点分配与类型生命周期实现。
