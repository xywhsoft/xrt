#define XRT_MODULE_LIST
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



typedef struct testitem {
	int Value;
	xlistnode Link;
} testitem;



/* 单头文件必须独立提供完整的侵入式链表实现。 */
int main(void)
{
	xlist List = XRT_LIST_INIT;
	testitem First = { 1, XRT_LIST_NODE_INIT };
	testitem Second = { 2, XRT_LIST_NODE_INIT };
	xlistnode* pNode;

	if ( !xrtListPushBack(&List, &First.Link) ||
		 !xrtListPushBack(&List, &Second.Link) ) {
		return 1;
	}
	pNode = xrtListPopFront(&List);
	if ( (pNode != &First.Link) || (List.First != &Second.Link) ) {
		return 2;
	}
	if ( !xrtListClear(&List) || !xrtListEmpty(&List) ) {
		return 3;
	}
	return 0;
}
