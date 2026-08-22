#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件节点把侵入链接嵌入业务对象。 */
typedef struct singlenode {
	int Key;
	xavlnode Link;
} singlenode;



/* 比较单头文件测试键与节点键。 */
static int singleCompare(const void* pKey, const xavlnode* pNode, ptr pUserData)
{
	const singlenode* pValue = (const singlenode*)((cbytes)pNode - offsetof(singlenode, Link));
	int iKey = *(const int*)pKey;

	(void)pUserData;
	return (iKey > pValue->Key) - (iKey < pValue->Key);
}



/* 单头文件必须独立提供侵入式插入、查找和删除。 */
int main(void)
{
	xavl tTree;
	xavliter tIterator;
	singlenode tNode;
	int iKey = 7;

	tNode.Key = iKey;
	xrtAVLNodeInit(&tNode.Link);
	if ( !xrtAVLInit(&tTree) ) {
		return 1;
	}
	if ( xrtAVLInsert(&tTree, &tNode.Link, &iKey, singleCompare, NULL, NULL) != &tNode.Link ) {
		return 2;
	}
	if ( xrtAVLFind(&tTree, &iKey, singleCompare, NULL) != &tNode.Link ) {
		return 3;
	}
	if (
		!xrtAVLIterFrom(
			&tTree,
			&iKey,
			singleCompare,
			NULL,
			&tIterator
		) ||
		(xrtAVLIterNext(&tIterator) != &tNode.Link)
	) {
		return 4;
	}
	if ( xrtAVLRemove(&tTree, &iKey, singleCompare, NULL) != &tNode.Link ) {
		return 5;
	}
	return 0;
}
