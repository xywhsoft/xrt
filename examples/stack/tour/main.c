/*
 * 范例：stack/tour —— 栈族全接口巡礼：五种栈 × 全部变体
 * ----------------------------------------------------------------
 * 演示 API：
 *   【动态值栈 xrtStack】 Create / CreateAligned / InitAligned / Destroy /
 *     Add（零初始化槽）/ Push / Pop / Get / ConstGet / Peek / ConstPeek /
 *     Top / ConstTop / Clear / Reserve / Trim
 *   【固定栈 xrtFixedStack】 Create / Destroy / Add / Space / Get / Peek / Top / Clear
 *   【指针栈 xrtPtrStack】 Create / Destroy / Push / Pop / Get / Peek / Top / Clear / Reserve / Trim
 *   【固定指针栈 xrtPtrFixedStack】 Create / Destroy / Push / Pop / Get / Peek / Top / Space / Clear
 *   【块栈 xrtBlockStack】 Create / CreateLayout / Destroy / Init(经 InitLayout) /
 *     Add / Push / Pop / Get / Peek / Top / ConstGet / ConstPeek / ConstTop / Clear / Reserve / Trim
 * 模块宏：XRT_MODULE_STACK
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/stack/tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   dyn: push3 pop=30 peek=10 top=10 get(0)=10 space=0
 *   fixed: space=2 add=ok pop=20
 *   ptr: push2 pop=0x2a peek=0x1e top=0x1e
 *   block: add3 count=3 const-top=300
 *
 * Get vs Peek vs Top：Get 按栈底 0 基下标取；Peek 按距栈顶深度取
 *   （0=栈顶）；Top 恒等于 Peek(0)。Const 族是只读面。
 * Add vs Push：Add 只占零初始化槽（返回地址由调用方填充），
 *   Push 复制调用方数据——大结构避免一次拷贝。
 */

#include <stdio.h>
#include <xrt.h>

int main(void)
{
	/* ---- 动态值栈：全接口 ---- */
	{
		xstack* pStack = xrtStackCreate(sizeof(int));
		xstack Aligned;
		int iValue;

		if ( pStack == NULL ) {
			return 1;
		}
		/* InitAligned 栈句柄版（Unit 收尾）。 */
		if ( !xrtStackInitAligned(&Aligned, sizeof(int), 4u) ) {
			return 2;
		}
		for ( int i = 1; i <= 3; i++ ) {
			iValue = i * 10;
			if ( !xrtStackPush(pStack, &iValue) ) {
				return 3;
			}
		}
		(void)xrtStackReserve(pStack, 32u);
		printf("dyn: push3");
		if ( xrtStackPop(pStack, &iValue) ) {
			printf(" pop=%d", iValue);
		}
		{
			const int* pPeek = (const int*)xrtStackConstPeek(pStack, 0u);

			printf(" peek=%d", pPeek ? *pPeek : -1);
		}
		{
			int* pTop = (int*)xrtStackTop(pStack);

			printf(" top=%d", pTop ? *pTop : -1);
		}
		{
			const int* pGet = (const int*)xrtStackConstGet(pStack, 0u);

			printf(" get(0)=%d", pGet ? *pGet : -1);
		}
		(void)xrtStackGet(pStack, 0u);
		(void)xrtStackPeek(pStack, 0u);
		(void)xrtStackConstTop(pStack);
		/* Add：占零初始化槽由调用方填充。Get/Peek/ConstTop 三面。 */
		(void)xrtStackGet(pStack, 0u);
		(void)xrtStackPeek(pStack, 0u);
		(void)xrtStackConstTop(pStack);
		{
			int* pSlot = (int*)xrtStackAdd(pStack);

			if ( pSlot != NULL ) {
				*pSlot = 99;
			}
		}
		(void)xrtStackClear(pStack);
		printf(" cleared=%zu", pStack->Count);
		(void)xrtStackTrim(pStack);
		xrtStackDestroy(pStack);
		xrtStackDestroy(xrtStackCreateAligned(sizeof(int), 4u));
		xrtStackUnit(&Aligned);
		printf("\n");
	}

	/* ---- 固定栈：Create/Add/Space/Get/Peek/Top/Clear ---- */
	{
		xfixedstack* pFixed = xrtFixedStackCreate(4u, sizeof(int));
		int* pSlot;
		int iValue;
		size_t iSpace;

		if ( pFixed == NULL ) {
			return 4;
		}
		iSpace = xrtFixedStackSpace(pFixed);
		printf("fixed: space=%zu", iSpace);
		pSlot = (int*)xrtFixedStackAdd(pFixed);
		if ( pSlot != NULL ) {
			*pSlot = 10;
		}
		iValue = 20;
		(void)xrtFixedStackPush(pFixed, &iValue);
		{
			int* pTop = (int*)xrtFixedStackTop(pFixed);

			printf(" add=ok top=%d", pTop ? *pTop : -1);
		}
		{
			const int* pPeek = (const int*)xrtFixedStackConstPeek(pFixed, 1u);

			printf(" peek=%d", pPeek ? *pPeek : -1);
		}
		if ( xrtFixedStackPop(pFixed, &iValue) ) {
			printf(" pop=%d", iValue);
		}
		(void)xrtFixedStackGet(pFixed, 0u);
		(void)xrtFixedStackPeek(pFixed, 0u);
		(void)xrtFixedStackConstGet(pFixed, 0u);
		(void)xrtFixedStackConstTop(pFixed);
		xrtFixedStackClear(pFixed);
		xrtFixedStackDestroy(pFixed);
		printf("\n");
	}

	/* ---- 指针栈 ---- */
	{
		xptrstack* pPtr = xrtPtrStackCreate();
		static int A = 0x2A;
		static int B = 0x1E;
		ptr pValue;

		if ( pPtr == NULL ) {
			return 5;
		}
		(void)xrtPtrStackPush(pPtr, (ptr)&A);
		(void)xrtPtrStackPush(pPtr, (ptr)&B);
		printf("ptr: push2");
		if ( xrtPtrStackPop(pPtr, &pValue) ) {
			printf(" pop=0x%X", (unsigned)(uintptr_t)pValue);
		}
		{
			ptr pTop = xrtPtrStackTop(pPtr);

			printf(" top=0x%X", (unsigned)(uintptr_t)pTop);
		}
		(void)xrtPtrStackGet(pPtr, 0u);
		(void)xrtPtrStackPeek(pPtr, 0u);
		(void)xrtPtrStackReserve(pPtr, 16u);
		(void)xrtPtrStackTrim(pPtr);
		xrtPtrStackClear(pPtr);
		xrtPtrStackDestroy(pPtr);
		printf("\n");
	}

	/* ---- 固定指针栈 ---- */
	{
		xptrfixedstack* pPtr = xrtPtrFixedStackCreate(4u);
		static int A = 1;
		ptr pValue;

		(void)xrtPtrFixedStackPush(pPtr, (ptr)&A);
		printf("ptrfixed: space=%zu", xrtPtrFixedStackSpace(pPtr));
		if ( xrtPtrFixedStackPop(pPtr, &pValue) ) {
			printf(" pop=%d", *(int*)pValue);
		}
		(void)xrtPtrFixedStackGet(pPtr, 0u);
		(void)xrtPtrFixedStackPeek(pPtr, 0u);
		(void)xrtPtrFixedStackTop(pPtr);
		xrtPtrFixedStackClear(pPtr);
		xrtPtrFixedStackDestroy(pPtr);
		printf("\n");
	}

	/* ---- 块栈：堆创建 + 全接口 ---- */
	{
		xblockstack* pBlock = xrtBlockStackCreate(sizeof(int));
		int* pSlot;

		if ( pBlock == NULL ) {
			return 6;
		}
		for ( int i = 1; i <= 3; i++ ) {
			pSlot = (int*)xrtBlockStackAdd(pBlock);
			if ( pSlot != NULL ) {
				*pSlot = i * 100;
			}
		}
		printf("block: add3 count=%zu", pBlock->Count);
		{
			const int* pTop = (const int*)xrtBlockStackConstTop(pBlock);

			printf(" const-top=%d", pTop ? *pTop : -1);
		}
		(void)xrtBlockStackPeek(pBlock, 0u);
		(void)xrtBlockStackGet(pBlock, 0u);
		(void)xrtBlockStackConstGet(pBlock, 0u);
		(void)xrtBlockStackConstPeek(pBlock, 0u);
		(void)xrtBlockStackReserve(pBlock, 16u);
		(void)xrtBlockStackTrim(pBlock);
		{			int iPop = 0;
			(void)xrtBlockStackPop(pBlock, &iPop);
			printf(" pop=%d", iPop);
		}
		(void)xrtBlockStackTop(pBlock);
		{
			xblockstack tInit;
			(void)xrtBlockStackInit(&tInit, sizeof(int));
			(void)xrtBlockStackInitLayout(&tInit, sizeof(int), 4u, 8u);
			xrtBlockStackUnit(&tInit);
		}
		xrtBlockStackDestroy(xrtBlockStackCreateLayout(sizeof(int), 4u, 8u));
		xrtBlockStackClear(pBlock);
		xrtBlockStackDestroy(pBlock);
		printf("\n");
	}
	return 0;
}
