#include <stdio.h>
#include <stddef.h>

#include <xrt.h>



/* 外部会话对象直接嵌入树节点，不产生额外分配。 */
typedef struct examplesession {
	int ID;
	const char* Name;
	xavlnode Index;
} examplesession;



/* 从侵入式链接恢复完整会话。 */
static examplesession* exampleSession(xavlnode* pNode)
{
	return (examplesession*)((bytes)pNode - offsetof(examplesession, Index));
}



/* 按会话编号比较查找键与节点。 */
static int exampleCompare(const void* pKey, const xavlnode* pNode, ptr pUserData)
{
	const examplesession* pSession = (const examplesession*)(
		(cbytes)pNode - offsetof(examplesession, Index)
	);
	int iID = *(const int*)pKey;

	(void)pUserData;
	return (iID > pSession->ID) - (iID < pSession->ID);
}



/* 演示零分配索引和外置有序迭代器。 */
int main(void)
{
	xavl tSessions;
	xavliter tIterator;
	examplesession pSessions[] = {
		{ 30, "gamma", { 0 } },
		{ 10, "alpha", { 0 } },
		{ 20, "beta", { 0 } }
	};

	if ( !xrtAVLInit(&tSessions) ) {
		return 1;
	}
	for ( size_t i = 0; i < 3; i++ ) {
		xrtAVLNodeInit(&pSessions[i].Index);
		if (
			xrtAVLInsert(
				&tSessions,
				&pSessions[i].Index,
				&pSessions[i].ID,
				exampleCompare,
				NULL,
				NULL
			) == NULL
		) {
			return 2;
		}
	}

	xrtAVLIterBegin(&tSessions, &tIterator);
	while ( true ) {
		xavlnode* pNode = xrtAVLIterNext(&tIterator);
		examplesession* pSession;

		if ( pNode == NULL ) {
			break;
		}
		pSession = exampleSession(pNode);
		printf("id=%d name=%s\n", pSession->ID, pSession->Name);
	}
	return 0;
}
