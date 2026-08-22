#include <stdio.h>

#include <xrt.h>



/* 拥有型配置对象按名称编号索引。 */
typedef struct exampleconfig {
	int ID;
	int Timeout;
} exampleconfig;



/* 按配置编号比较查找键与对象。 */
static int exampleCompare(const void* pKey, const void* pItem, ptr pUserData)
{
	int iID = *(const int*)pKey;
	int iItemID = ((const exampleconfig*)pItem)->ID;

	(void)pUserData;
	return (iID > iItemID) - (iID < iItemID);
}



/* 演示复制添加、直接查找和自动对象存储。 */
int main(void)
{
	xavltree tConfigs;
	xavltreeiter tIterator;
	exampleconfig pInput[] = {
		{ 30, 3000 },
		{ 10, 1000 },
		{ 20, 2000 }
	};
	int iSearch = 20;

	if ( !xrtAVLTreeInit(&tConfigs, sizeof(exampleconfig), exampleCompare, NULL) ) {
		return 1;
	}
	for ( size_t i = 0; i < 3; i++ ) {
		if ( xrtAVLTreeAdd(&tConfigs, &pInput[i].ID, &pInput[i], NULL) == NULL ) {
			xrtAVLTreeUnit(&tConfigs);
			return 2;
		}
	}

	{
		exampleconfig* pConfig = (exampleconfig*)xrtAVLTreeFind(&tConfigs, &iSearch);

		if ( pConfig != NULL ) {
			printf("id=%d timeout=%d\n", pConfig->ID, pConfig->Timeout);
		}
	}
	if ( xrtAVLTreeIterFrom(&tConfigs, &iSearch, &tIterator) ) {
		exampleconfig* pConfig;

		while ( (pConfig = (exampleconfig*)xrtAVLTreeIterNext(&tIterator)) != NULL ) {
			printf("range id=%d timeout=%d\n", pConfig->ID, pConfig->Timeout);
		}
	}
	xrtAVLTreeUnit(&tConfigs);
	return 0;
}
