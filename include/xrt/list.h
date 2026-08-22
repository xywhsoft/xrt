#ifndef XRT_LIST_H
#define XRT_LIST_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_LIST)

/* 侵入式链表不拥有节点内存；一个对象可嵌入多个独立节点。 */
typedef struct xlist xlist;

typedef struct xlistnode {
	struct xlistnode* Prev;
	struct xlistnode* Next;
	xlist* Owner;
} xlistnode;



/* 链表字段公开只读，结构修改必须经过 API 以维持所有权和版本不变量。 */
struct xlist {
	xlistnode* First;
	xlistnode* Last;
	size_t Count;
	uint64 Version;
	uint32 State;
};



/* 外置迭代器不分配内存，并允许通过专用操作删除当前节点。 */
typedef struct xlistiter {
	xlist* List;
	xlistnode* Next;
	xlistnode* Current;
	uint64 Version;
	bool Reverse;
} xlistiter;



/* 链表模块稳定错误代码。 */
typedef enum xlisterror {
	XLIST_ERROR_ARGUMENT = 1,
	XLIST_ERROR_STATE,
	XLIST_ERROR_RANGE,
	XLIST_ERROR_MODIFIED
} xlisterror;



/* 静态链表和节点初始化器。 */
#define XRT_LIST_INIT { NULL, NULL, 0u, 0u, 1u }
#define XRT_LIST_NODE_INIT { NULL, NULL, NULL }



/* 从嵌入成员地址恢复所属结构地址；参数只求值一次。 */
#define XRT_CONTAINER_OF(pMember, Type, Member) \
	((Type*)((unsigned char*)(pMember) - offsetof(Type, Member)))



XRT_EXTERN_C_BEGIN



/* 初始化新的链表存储；不能用于仍然持有节点的活动链表。 */
XRT_API void xrtListInit(xlist* pList);



/* 初始化新的节点存储；不能用于仍然属于链表的节点。 */
XRT_API void xrtListNodeInit(xlistnode* pNode);



/* 判断链表是否已经初始化。 */
XRT_API bool xrtListReady(const xlist* pList);



/* 完整验证节点所有权、双向链接、端点和计数。 */
XRT_API bool xrtListValidate(const xlist* pList);



/* 返回链表是否为空和当前节点数量。 */
XRT_API bool xrtListEmpty(const xlist* pList);
XRT_API size_t xrtListCount(const xlist* pList);



/* 返回首尾节点；空链表返回空指针。 */
XRT_API xlistnode* xrtListFirst(const xlist* pList);
XRT_API xlistnode* xrtListLast(const xlist* pList);



/* 返回节点的前后节点和所属链表。 */
XRT_API xlistnode* xrtListPrev(const xlistnode* pNode);
XRT_API xlistnode* xrtListNext(const xlistnode* pNode);
XRT_API xlist* xrtListOwner(const xlistnode* pNode);



/* 判断节点是否属于指定链表，或是否已经连接到任意链表。 */
XRT_API bool xrtListContains(const xlist* pList, const xlistnode* pNode);
XRT_API bool xrtListLinked(const xlistnode* pNode);



/* 在链表首尾插入一个已初始化且未连接的节点。 */
XRT_API bool xrtListPushFront(xlist* pList, xlistnode* pNode);
XRT_API bool xrtListPushBack(xlist* pList, xlistnode* pNode);



/* 在所属链表中的参考节点前后插入未连接节点。 */
XRT_API bool xrtListInsertBefore(
	xlist* pList,
	xlistnode* pPosition,
	xlistnode* pNode
);
XRT_API bool xrtListInsertAfter(
	xlist* pList,
	xlistnode* pPosition,
	xlistnode* pNode
);



/* 从指定链表移除节点，但不释放节点内存。 */
XRT_API bool xrtListRemove(xlist* pList, xlistnode* pNode);



/* 移除并返回首尾节点；空链表是正常结果。 */
XRT_API xlistnode* xrtListPopFront(xlist* pList);
XRT_API xlistnode* xrtListPopBack(xlist* pList);



/* 把已有节点移动到链表首尾，不改变节点数量。 */
XRT_API bool xrtListMoveFront(xlist* pList, xlistnode* pNode);
XRT_API bool xrtListMoveBack(xlist* pList, xlistnode* pNode);



/* 分离全部节点并保留可继续使用的空链表。 */
XRT_API bool xrtListClear(xlist* pList);



/* 启动正向或反向迭代。 */
XRT_API bool xrtListIterBegin(xlist* pList, xlistiter* pIterator);
XRT_API bool xrtListIterRBegin(xlist* pList, xlistiter* pIterator);



/* 返回下一节点；自然结束不设置错误，外部结构修改会被拒绝。 */
XRT_API xlistnode* xrtListIterNext(xlistiter* pIterator);



/* 移除最近一次返回的节点，并让迭代器继续保持有效。 */
XRT_API bool xrtListIterRemove(xlistiter* pIterator);



/* 提前结束迭代；允许传入空指针。 */
XRT_API void xrtListIterEnd(xlistiter* pIterator);



XRT_EXTERN_C_END

#endif

#endif
