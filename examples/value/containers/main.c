#include <stdio.h>
#include <xrt.h>

/*
 * 范例：value/containers —— 嵌套 COW：克隆、按路径改写与快照迭代
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtValueClone               浅克隆外壳（元素 COW 共享）
 *   xrtValueObjectEdit          按名取可写成员（触发 COW 分裂）
 *   xrtValueArrayResolve        负索引解析为 0 基下标（-1 = 末元素）
 *   xrtValueArraySetNew         按下标替换元素（接管新值）
 *   xrtValueIterBegin/Next/End  对象快照迭代（键 + 借用值）
 * 模块宏：XRT_MODULE_VALUE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c  *       examples/value/containers/main.c -lws2_32 -liphlpapi
 * 预期输出（对象按插入序）：
 *   code: int
 *   tags: array
 *
 * COW 演示主线：{code:200, tags:[xrt,network]} 克隆后，
 *   对副本的 tags 末位（负索引 -1）改写为 http——
 *   原对象不受影响（这正是 Clone 后各自 Edit 安全的原因）。
 * 负索引：Resolve(-1) → 最后一个元素，Python 风格；
 *   越界失败且不改动输出参数。
 */





/* 演示 Array、Object、负索引、嵌套 COW 和快照迭代。 */
int main(void)
{
	xvalue* pResponse = xrtValueObject();
	xvalue* pTags = xrtValueArray();
	xvalue* pCopy = NULL;
	xvalue* pMutableTags;
	xvalueiter tIterator;
	xvaluekey Key;
	xvalue* pItem;
	size_t iLast;
	int iResult = 1;

	if ( (pResponse == NULL) || (pTags == NULL) ||
		 !xrtValueArrayAppendNew(
			pTags,
			xrtValueString(XRT_STR_LITERAL("xrt"))
		 ) ||
		 !xrtValueArrayAppendNew(
			pTags,
			xrtValueString(XRT_STR_LITERAL("network"))
		 ) ||
		 !xrtValueObjectSetNew(
			pResponse,
			XRT_STR_LITERAL("code"),
			xrtValueInt(200)
		 ) ||
		 !xrtValueObjectSetTake(
			pResponse,
			XRT_STR_LITERAL("tags"),
			&pTags
		 ) ) {
		goto cleanup;
	}

	pCopy = xrtValueClone(pResponse);
	if ( pCopy == NULL ) {
		goto cleanup;
	}
	pMutableTags = xrtValueObjectEdit(
		pCopy,
		XRT_STR_LITERAL("tags")
	);
	if ( (pMutableTags == NULL) ||
		 !xrtValueArrayResolve(pMutableTags, -1, &iLast) ||
		 !xrtValueArraySetNew(
			pMutableTags,
			iLast,
			xrtValueString(XRT_STR_LITERAL("http"))
		 ) ) {
		goto cleanup;
	}

	if ( !xrtValueIterBegin(pCopy, &tIterator) ) {
		goto cleanup;
	}
	while ( (pItem = xrtValueIterNext(&tIterator, &Key)) != NULL ) {
		printf(
			"%.*s: %s\n",
			(int)Key.String.Size,
			Key.String.Data,
			xrtValueTypeName(xrtValueType(pItem))
		);
	}
	xrtValueIterEnd(&tIterator);
	iResult = 0;

cleanup:
	xrtValueRelease(pCopy);
	xrtValueRelease(pTags);
	xrtValueRelease(pResponse);
	return iResult;
}
