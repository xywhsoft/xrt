/*
 * 范例：containers/list —— 内嵌节点双向链表实现 O(1) LRU 顺序维护
 * ----------------------------------------------------------------
 * 演示 API：
 *   XRT_LIST_INIT / XRT_LIST_NODE_INIT  静态初始化链表与节点
 *   xrtListPushFront    把节点挂到链头（O(1)）
 *   xrtListMoveFront    把已在链中的节点移到链头（O(1)，LRU 核心）
 *   xrtListFirst/Next   遍历（头 → 尾）
 *   xrtListClear        摘除全部节点（不释放宿主对象）
 *   XRT_CONTAINER_OF    由内嵌节点反查宿主结构地址
 * 模块宏：XRT_MODULE_LIST
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/list/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   3=300 2=200 1=100
 *   1=100 3=300 2=200
 *
 * 侵入式设计：链表节点内嵌在宿主对象里（本例 Recent 字段），
 *   - 挂链零分配：链操作只改指针；
 *   - 一个对象可同时挂多条链（再加一个节点字段即可）；
 *   - Clear 只摘链不删对象——对象生命周期完全归调用方。
 * 链头 = 最近使用，链尾 = 最久未用；MoveFront 就是 LRU 命中提升。
 */

#include <stdio.h>

#include <xrt.h>



/* 缓存条目：Recent 内嵌节点让它可以直接挂进 LRU 链。 */
typedef struct cacheitem {
	int Key;
	int Value;
	xlistnode Recent;
} cacheitem;



/* 打印从最近到最久未使用的缓存条目。 */
static void printCache(const xlist* pCache)
{
	for ( xlistnode* pNode = xrtListFirst(pCache);
		  pNode != NULL;
		  pNode = xrtListNext(pNode) ) {
		/* 由节点地址减去字段偏移，反查宿主 cacheitem 地址。 */
		cacheitem* pItem = XRT_CONTAINER_OF(pNode, cacheitem, Recent);

		printf("%d=%d ", pItem->Key, pItem->Value);
	}
	printf("\n");
}



int main(void)
{
	xlist Cache = XRT_LIST_INIT;    /* 空链表，无分配 */
	cacheitem Items[] = {
		{ 1, 100, XRT_LIST_NODE_INIT },
		{ 2, 200, XRT_LIST_NODE_INIT },
		{ 3, 300, XRT_LIST_NODE_INIT }
	};

	/* 依次压到链头：最终顺序 3(最新) 2 1(最旧)。 */
	for ( size_t i = 0; i < 3u; i++ ) {
		if ( !xrtListPushFront(&Cache, &Items[i].Recent) ) {
			return 1;
		}
	}
	printCache(&Cache);

	/*
	 * LRU 核心：访问条目 1 → O(1) 移到链头。
	 * MoveFront 只调整两个方向的指针，与链长无关。
	 */
	if ( !xrtListMoveFront(&Cache, &Items[0].Recent) ) {
		return 2;
	}
	printCache(&Cache);

	/* 摘除全部节点；Items 是栈数组，无需也不应释放。 */
	if ( !xrtListClear(&Cache) ) {
		return 3;
	}
	return 0;
}
