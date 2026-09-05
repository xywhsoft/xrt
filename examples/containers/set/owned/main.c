/*
 * 范例：containers/set/owned —— 拥有式集合：自定义键、深复制与资源移交
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtSetSetKeyPolicy    安装自定义 哈希 + 相等（键=业务字段 Id）
 *   xrtSetSetLifecycle    安装 深复制 + 析构（集合接管元素内存）
 *   xrtSetAdd             插入（内部用 Copy 深复制，不再 memcmp）
 *   xrtSetGet             按自定义键查找规范元素
 *   xrtSetClone           深克隆整个集合（每个元素都走 Copy）
 *   xrtSetTake            移出元素：集合放弃所有权，调用方负责释放
 * 模块宏：XRT_MODULE_SET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/set/owned/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   taken tag: 7 primary
 *
 * 与默认集合（set/main.c）的三个进阶差异：
 *   1. 键不再是整条字节，而是 Id 字段（Hash/Equal 自己定义）；
 *   2. 元素拥有堆资源（Name 副本）——插入即深复制，
 *      集合销毁时统一走 Drop 释放，绝不泄漏；
 *   3. "规范元素"语义：Id 相同即同一元素，后插入的重复项被忽略
 *      （用 duplicate 查回的是首次存入的 primary）。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 拥有型标签：Name 指向集合自己分配的副本。 */
typedef struct exampletag {
	int Id;
	char* Name;
} exampletag;



/* 按业务编号计算标签哈希（只用 Id，忽略 Name）。 */
static uint64 exampleTagHash(const void* pItem, ptr pUserData)
{
	const exampletag* pTag = (const exampletag*)pItem;

	(void)pUserData;
	return xrtHash64(&pTag->Id, sizeof(pTag->Id));
}



/* 按业务编号判断两个标签是否等价（Id 相同 = 同一元素）。 */
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



/*
 * 深度复制：为 Name 分配新副本。约定：失败返回 false 时
 * 目标必须保持"不拥有任何资源"的清零状态（集合会安全丢弃它）。
 */
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



/* 析构：释放元素拥有的 Name 副本（集合销毁/移除时逐个调用）。 */
static void exampleTagDrop(ptr pItem, ptr pUserData)
{
	exampletag* pTag = (exampletag*)pItem;

	(void)pUserData;
	xrtFree(pTag->Name);
	pTag->Name = NULL;
}



int main(void)
{
	xset tTags;
	xset* pClone = NULL;     /* 堆分配：克隆结果 */
	exampletag tPrimary = { 7, "primary" };
	exampletag tDuplicate = { 7, "duplicate" };   /* 同 Id：将被去重 */
	exampletag tTaken = { 0 };                    /* Take 的接收槽 */
	const exampletag* pStored;
	int iResult = 0;

	if ( !xrtSetInit(&tTags, sizeof(exampletag)) ) {
		return 1;
	}

	/*
	 * 安装两套策略（须在插入任何元素之前）：
	 *   KeyPolicy  —— 怎么算哈希、怎么判等；
	 *   Lifecycle  —— 插入时深复制、销毁时释放。
	 * 之后集合对元素的全部读写都走这四个回调。
	 */
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

	/*
	 * 插入两条 Id=7 的标签：第二条因键等价被忽略。
	 * 集合内部存的是 Copy 出来的深副本，与栈上原对象解耦。
	 */
	if ( !xrtSetAdd(&tTags, &tPrimary) ||
		!xrtSetAdd(&tTags, &tDuplicate) ) {
		iResult = 3;
		goto cleanup;
	}

	/* 用 duplicate 作查询键：命中的必须是首次存入的 primary。 */
	pStored = (const exampletag*)xrtSetGet(&tTags, &tDuplicate);
	if ( (pStored == NULL) || (strcmp(pStored->Name, "primary") != 0) ) {
		iResult = 4;
		goto cleanup;
	}

	/* 深克隆：Name 副本也逐个复制，两个集合互不影响。 */
	pClone = xrtSetClone(&tTags);
	if ( pClone == NULL ) {
		iResult = 5;
		goto cleanup;
	}

	/*
	 * 移出元素：tTaken 收到一份完整的深副本（含 Name 所有权），
	 * 集合中的对应槽位被摘除——之后由我们 xrtFree(tTaken.Name)。
	 */
	if ( !xrtSetTake(&tTags, &tPrimary, &tTaken) ) {
		iResult = 6;
		goto cleanup;
	}
	printf("taken tag: %d %s\n", tTaken.Id, tTaken.Name);

cleanup:
	/* Take 的资源归调用方；克隆销毁；原集合销毁（Drop 逐元素善后）。 */
	xrtFree(tTaken.Name);
	if ( pClone != NULL ) {
		xrtSetDestroy(pClone);
	}
	xrtSetUnit(&tTags);
	return iResult;
}
