#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 拥有型标签按编号去重，并拥有名称副本。 */
typedef struct exampletag {
	int Id;
	char* Name;
} exampletag;



/* 按业务编号计算标签哈希。 */
static uint64 exampleTagHash(const void* pItem, ptr pUserData)
{
	const exampletag* pTag = (const exampletag*)pItem;

	(void)pUserData;
	return xrtHash64(&pTag->Id, sizeof(pTag->Id));
}



/* 按业务编号判断两个标签是否等价。 */
static bool exampleTagEqual(
	const void* pLeft,
	const void* pRight,
	ptr pUserData
)
{
	const exampletag* pLeftTag = (const exampletag*)pLeft;
	const exampletag* pRightTag = (const exampletag*)pRight;

	(void)pUserData;
	return pLeftTag->Id == pRightTag->Id;
}



/* 深度复制标签名称，失败时保持已清零目标不拥有资源。 */
static bool exampleTagCopy(ptr pTarget, const void* pSource, ptr pUserData)
{
	exampletag* pTargetTag = (exampletag*)pTarget;
	const exampletag* pSourceTag = (const exampletag*)pSource;
	size_t iNameSize;

	(void)pUserData;
	iNameSize = strlen(pSourceTag->Name) + 1u;
	pTargetTag->Name = (char*)xrtMalloc(iNameSize);
	if ( pTargetTag->Name == NULL ) {
		return false;
	}
	pTargetTag->Id = pSourceTag->Id;
	memcpy(pTargetTag->Name, pSourceTag->Name, iNameSize);
	return true;
}



/* 释放标签拥有的名称副本。 */
static void exampleTagDrop(ptr pItem, ptr pUserData)
{
	exampletag* pTag = (exampletag*)pItem;

	(void)pUserData;
	xrtFree(pTag->Name);
	pTag->Name = NULL;
}



/* 演示自定义键、深复制、规范元素、克隆和资源移交。 */
int main(void)
{
	xset tTags;
	xset* pClone = NULL;
	exampletag tPrimary = { 7, "primary" };
	exampletag tDuplicate = { 7, "duplicate" };
	exampletag tTaken = { 0 };
	const exampletag* pStored;
	int iResult = 0;

	if ( !xrtSetInit(&tTags, sizeof(exampletag)) ) {
		return 1;
	}
	if (
		!xrtSetSetKeyPolicy(
			&tTags,
			exampleTagHash,
			exampleTagEqual,
			NULL
		) ||
		!xrtSetSetLifecycle(
			&tTags,
			exampleTagCopy,
			exampleTagDrop,
			NULL
		)
	) {
		iResult = 2;
		goto cleanup;
	}
	if ( !xrtSetAdd(&tTags, &tPrimary) ||
		!xrtSetAdd(&tTags, &tDuplicate) ) {
		iResult = 3;
		goto cleanup;
	}

	pStored = (const exampletag*)xrtSetGet(&tTags, &tDuplicate);
	if ( (pStored == NULL) || (strcmp(pStored->Name, "primary") != 0) ) {
		iResult = 4;
		goto cleanup;
	}
	pClone = xrtSetClone(&tTags);
	if ( pClone == NULL ) {
		iResult = 5;
		goto cleanup;
	}
	if ( !xrtSetTake(&tTags, &tPrimary, &tTaken) ) {
		iResult = 6;
		goto cleanup;
	}
	printf("taken tag: %d %s\n", tTaken.Id, tTaken.Name);

cleanup:
	xrtFree(tTaken.Name);
	if ( pClone != NULL ) {
		xrtSetDestroy(pClone);
	}
	xrtSetUnit(&tTags);
	return iResult;
}
