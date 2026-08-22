#include <stdio.h>

#include <xrt.h>



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
		cacheitem* pItem = XRT_CONTAINER_OF(pNode, cacheitem, Recent);

		printf("%d=%d ", pItem->Key, pItem->Value);
	}
	printf("\n");
}



/* 演示对象内嵌节点和 O(1) LRU 顺序移动。 */
int main(void)
{
	xlist Cache = XRT_LIST_INIT;
	cacheitem Items[] = {
		{ 1, 100, XRT_LIST_NODE_INIT },
		{ 2, 200, XRT_LIST_NODE_INIT },
		{ 3, 300, XRT_LIST_NODE_INIT }
	};

	for ( size_t i = 0; i < 3u; i++ ) {
		if ( !xrtListPushFront(&Cache, &Items[i].Recent) ) {
			return 1;
		}
	}
	printCache(&Cache);

	if ( !xrtListMoveFront(&Cache, &Items[0].Recent) ) {
		return 2;
	}
	printCache(&Cache);

	if ( !xrtListClear(&Cache) ) {
		return 3;
	}
	return 0;
}
