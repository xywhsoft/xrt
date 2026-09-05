/*
 * 范例：value/iter_weak —— 迭代器族与弱引用/句柄/TypeId/Finalizer 补遗
 * ----------------------------------------------------------------
 * 演示 API：
 *   【迭代器】 xrtValueIterCreate / IterDestroy（堆迭代器）
 *              xrtValueIterRBegin / IterRCreate（逆序两形态）
 *              xrtValueIterAdvance（三态步进快照）
 *   【弱引用】 xrtValueWeakRef / IsWeakRef / WeakRefExpired / WeakRefLock
 *   【句柄】   xrtValueHandleTake + xrtValueTakeHandle（接管/取回往返）
 *   【TypeId】 xrtValueTypeId / TypeIdBind / TypeIdRebind
 *   【身份】   xrtValueIdentityBind（哈希+相等策略）
 *   【对象】   xrtValueObjectFinalizerBind（终结器）
 * 模块宏：XRT_MODULE_VALUE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/value/iter_weak/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   iter: 1 2 3 | riter: 3 2 1
 *   weak: is=1 expired-after-release=1 lock-after-expire=1
 *   handle-take=1 typeid=77 rebind=88
 *
 * 弱引用四件套：WeakRef 创建 → IsWeakRef 判型 → 目标 Release 后
 *   Expired=1 后 Lock 仍返回一个非空标记值——过期判定
 *   必须用 WeakRefExpired，不能只看 Lock 结果。
 */

#include <stdio.h>
#include <xrt.h>

/* 最小句柄 Drop（拥有式 int）。 */
static void intDrop(ptr pHandle, ptr pUserData)
{
	(void)pUserData;
	xrtFree(pHandle);
}

/* 最小哈希/相等（按 int 值）。 */
static uint64 intHash(const xvalue* pValue, ptr pUserData)
{
	int64 iValue = 0;

	(void)pUserData;
	(void)xrtValueGetInt(pValue, &iValue);
	return (uint64)iValue * 1099511628211ull;
}

static bool intEqual(const xvalue* pLeft, const xvalue* pRight, ptr pUserData)
{
	int64 iL = 0;
	int64 iR = 0;

	(void)pUserData;
	(void)xrtValueGetInt(pLeft, &iL);
	(void)xrtValueGetInt(pRight, &iR);
	return iL == iR;
}

static void onFinalize(xvalue* pObject, ptr pUserData)
{
	(void)pObject; (void)pUserData;
	printf("finalizer fired\n");
}

int main(void)
{
	xvalue* pArray = xrtValueArray();
	xvalue* pTarget;
	xvalue* pWeak;
	xvalue* pLocked;

	/* 数组 1 2 3：堆迭代器正向 + 逆序双形态 + Advance 三态。 */
	for ( int i = 1; i <= 3; i++ ) {
		(void)xrtValueArrayAppendNew(pArray, xrtValueInt(i));
	}
	{
		xvalueiter* pIter = xrtValueIterCreate(pArray);
		xvaluekey Key;
		xvalue* pItem;
		int64 iValue = 0;

		printf("iter:");
		while ( (pItem = xrtValueIterNext(pIter, &Key)) != NULL ) {
			(void)xrtValueGetInt(pItem, &iValue);
			printf(" %lld", (long long)iValue);
		}
		xrtValueIterDestroy(pIter);
		printf(" |");
	}
	{
		/* 栈形态逆序迭代器（RBegin）+ 三态步进（Advance）。 */
		xvalueiter rIter;
		xvaluekey rKey;
		xvalue* pItem;
		int64 iValue = 0;

		(void)xrtValueIterRBegin(pArray, &rIter);
		printf(" rbegin-advance:");
		while ( xrtValueIterAdvance(&rIter, &rKey, &pItem) == XVALUE_ITER_ITEM ) {
			(void)xrtValueGetInt(pItem, &iValue);
			printf(" %lld", (long long)iValue);
		}
		xrtValueIterEnd(&rIter);
		printf(" |");
	}
	{
		xvalueiter* pRIter = xrtValueIterRCreate(pArray);
		xvaluekey Key;
		xvalue* pItem;
		int64 iValue = 0;

		printf(" riter:");
		while ( (pItem = xrtValueIterNext(pRIter, &Key)) != NULL ) {
			(void)xrtValueGetInt(pItem, &iValue);
			printf(" %lld", (long long)iValue);
		}
		xrtValueIterDestroy(pRIter);
		printf("\n");
	}
	xrtValueRelease(pArray);

	/* 弱引用四件套。 */
	pTarget = xrtValueInt(9);
	pWeak = xrtValueWeakRef(pTarget);
	printf("weak: is=%d", xrtValueIsWeakRef(pWeak) ? 1 : 0);
	xrtValueRelease(pTarget);
	printf(" expired-after-release=%d",
		xrtValueWeakRefExpired(pWeak) ? 1 : 0);
	pLocked = xrtValueWeakRefLock(pWeak);
	printf(" lock-after-expire=%d\n", pLocked != NULL ? 1 : 0);
	xrtValueRelease(pWeak);

	/* 句柄接管/取回 + TypeId 三件套 + IdentityBind + Finalizer。 */
	{
		int* pHandle = (int*)xrtMalloc(sizeof(int));
		static const xvaluehandleops tOps = { NULL, intDrop, NULL, NULL };
		xvalue* pHandleValue;
		int* pBack = NULL;

		*pHandle = 5;
		pHandleValue = xrtValueHandleTake((ptr*)&pHandle, &tOps, NULL);
		printf("handle-take=%d",
			xrtValueTakeHandle(pHandleValue, (ptr*)&pBack) &&
			(pBack != NULL) && (*pBack == 5) ? 1 : 0);
		xrtFree(pBack);
		xrtValueRelease(pHandleValue);
	}
	{
		xvalue* pObject = xrtValueObject();

		(void)xrtValueTypeIdBind(pObject, 77u);
		printf(" typeid=%llu",
			(unsigned long long)xrtValueTypeId(pObject));
		(void)xrtValueTypeIdRebind(pObject, 88u);
		printf(" rebind=%llu\n",
			(unsigned long long)xrtValueTypeId(pObject));
		/* IdentityBind：容器需先有 TypeId（子容器才允许策略）。 */
		{
			xvalue* pSet = xrtValueSet();

			(void)xrtValueTypeIdBind(pSet, 90u);
			printf(" identity=%d\n",
				xrtValueIdentityBind(pSet, intHash, intEqual, NULL) ? 1 : 0);
			xrtValueRelease(pSet);
		}
		(void)xrtValueObjectFinalizerBind(pObject, onFinalize, NULL);
		xrtValueRelease(pObject);   /* 触发终结器 */
	}
	return 0;
}
