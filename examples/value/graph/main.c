#include <stdio.h>
#include <xrt.h>

/*
 * 范例：value/graph —— 深克隆的 DAG 身份保持与改写隔离
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtValueDeepClone   深克隆：复制全部层级，但保留内部共享身份
 *   xrtValueEqual       结构相等（逐层比较值，不比指针）
 *   xrtValueArrayGet / ArrayEdit   只读借用 vs 可写入口（COW）
 * 模块宏：XRT_MODULE_VALUE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c  *       examples/value/graph/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   equal=0 shared=0 source=1 copy=2
 *
 * DAG 身份保持：源数组两个元素是同一对象（Append 两次同一 pChild）
 *   ——深克隆后副本内两元素仍同一（克隆时记住了共享关系），
 *   不会膨胀成两份独立拷贝。
 * 读数解释（printf 时点在改写之后）：
 *   shared=0 —— ArrayEdit(副本,0) 触发 COW：被编辑的元素分裂出
 *               新外壳，与元素 1 不再同指针（改谁分裂谁）；
 *   source=1 copy=2 —— 改写只落在副本，源图的 count 保持 1；
 *   equal=0  —— 结构已不同（副本 count=2），证明完全隔离。
 */





/* 展示深克隆保留 DAG 身份，同时与来源图完全隔离。 */
int main(void)
{
	xvalue* pRoot = xrtValueArray();
	xvalue* pChild = xrtValueObject();
	xvalue* pCopy;
	xvalue* pCopyChild;
	int64 iSource = 0;
	int64 iCopy = 0;

	if ( (pRoot == NULL) || (pChild == NULL) ||
		 !xrtValueObjectSetNew(
			pChild,
			XRT_STR_LITERAL("count"),
			xrtValueInt(1)
		 ) ||
		 !xrtValueArrayAppend(pRoot, pChild) ||
		 !xrtValueArrayAppend(pRoot, pChild) ) {
		xrtValueRelease(pChild);
		xrtValueRelease(pRoot);
		return 1;
	}
	pCopy = xrtValueDeepClone(pRoot);
	if ( (pCopy == NULL) ||
		 !xrtValueEqual(pRoot, pCopy) ||
		 (xrtValueArrayGet(pCopy, 0) !=
		  xrtValueArrayGet(pCopy, 1)) ) {
		xrtValueRelease(pCopy);
		xrtValueRelease(pChild);
		xrtValueRelease(pRoot);
		return 2;
	}
	pCopyChild = xrtValueArrayEdit(pCopy, 0);
	if ( (pCopyChild == NULL) ||
		 !xrtValueObjectSetNew(
			pCopyChild,
			XRT_STR_LITERAL("count"),
			xrtValueInt(2)
		 ) ||
		 !xrtValueGetInt(
			xrtValueObjectGet(
				pChild,
				XRT_STR_LITERAL("count")
			),
			&iSource
		 ) ||
		 !xrtValueGetInt(
			xrtValueObjectGet(
				pCopyChild,
				XRT_STR_LITERAL("count")
			),
			&iCopy
		 ) ) {
		xrtValueRelease(pCopy);
		xrtValueRelease(pChild);
		xrtValueRelease(pRoot);
		return 3;
	}
	printf(
		"equal=%d shared=%d source=%lld copy=%lld\n",
		xrtValueEqual(pRoot, pCopy) ? 1 : 0,
		xrtValueArrayGet(pCopy, 0) ==
			xrtValueArrayGet(pCopy, 1) ? 1 : 0,
		(long long)iSource,
		(long long)iCopy
	);
	xrtValueRelease(pCopy);
	xrtValueRelease(pChild);
	xrtValueRelease(pRoot);
	return 0;
}
