/*
 * 范例：core/allocator_tour —— 进程级分配器与调用位置族
 * ----------------------------------------------------------------
 * 演示 API：
 *   【分配器】    xrtGetAllocator（复制当前分配器）
 *                 xrtSetAllocator（首次分配前替换——计数演示）
 *   【位置族】    xrtMallocAt / CallocAt / ReallocAt / FreeAt /
 *                 MemDupAt（携带 __FILE__/__LINE__ 的调试入口）
 * 模块宏：XRT_MODULE_MEMORY_DEBUG
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/core/allocator_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   allocator: swap -> at-family alloc/calloc/dup/realloc ok
 *   allocator: locked after first alloc -> swap rejected ok
 *
 * SetAllocator 只允许在首次分配之前调用——main 一进来就换；
 *   At 族即 xrtMalloc 等宏展开后的底层函数，记录调用位置。
 *   首次分配之后分配器永久锁定（再次替换返回状态错误），
 *   本例的计数分配器驻留到进程结束。
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xrt.h>

/* 计数分配器：仍路由系统堆，便于验证替换生效。 */
typedef struct examplecount {
	size_t iAllocs;
	size_t iFrees;
} examplecount;

static ptr exampleAlloc(ptr pContext, size_t iSize)
{
	++((examplecount*)pContext)->iAllocs;
	return (ptr)malloc(iSize);
}

static ptr exampleRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	(void)pContext;
	return (ptr)realloc(pMemory, iSize);
}

static void exampleFree(ptr pContext, ptr pMemory)
{
	++((examplecount*)pContext)->iFrees;
	free(pMemory);
}

int main(void)
{
	xallocator Default;
	xallocator Custom;
	examplecount Count;
	uint8* pBlock;
	uint8* pDup;
	int iResult = 1;

	/* ---- 必须在任何 XRT 分配前完成替换。 ---- */
	xrtGetAllocator(&Default);
	memset(&Count, 0, sizeof(Count));
	Custom.Context = (ptr)&Count;
	Custom.Alloc = exampleAlloc;
	Custom.Realloc = exampleRealloc;
	Custom.Free = exampleFree;
	if ( !xrtSetAllocator(&Custom) ) {
		goto Cleanup;
	}

	/* ---- At 族五接口：位置参数不影响堆行为。 ---- */
	pBlock = (uint8*)xrtMallocAt(4u, __FILE__, __LINE__);
	if ( (pBlock == NULL) ||
		((pDup = (uint8*)xrtMemDupAt("hi", 2u, __FILE__,
			__LINE__)) == NULL) ||
		(memcmp(pDup, "hi", 2u) != 0) ||
		((pBlock = (uint8*)xrtReallocAt(pBlock, 16u, __FILE__,
			__LINE__)) == NULL) ||
		(xrtCallocAt(2u, 4u, __FILE__, __LINE__) ==
			NULL) ) {
		goto Cleanup;
	}
	/* Calloc 清零验证单独做——保留指针用于释放。 */
	{
		uint8* pZero = (uint8*)xrtCallocAt(2u, 4u, __FILE__,
			__LINE__);

		if ( (pZero == NULL) || (pZero[0] != 0u) ||
			(pZero[7] != 0u) ) {
			goto Cleanup;
		}
		xrtFreeAt(pZero, __FILE__, __LINE__);
	}
	xrtFreeAt(pDup, __FILE__, __LINE__);
	xrtFreeAt(pBlock, __FILE__, __LINE__);
	printf("allocator: swap -> at-family alloc/calloc/dup/realloc ok\n");

	/* ---- 首次分配后分配器永久锁定：再次替换必须失败，
	 * 且 GetAllocator 仍报告已安装的计数分配器。 ---- */
	/* At 族分配可能由内存池层服务——只有穿透到 backing 的
	 * 分配才经过自定义 Alloc；计数只需 >= 1 即证明替换生效。 */
	if ( xrtSetAllocator(&Default) ||
		(Count.iAllocs < 1u) ) {
		goto Cleanup;
	}
	{
		xallocator Current;

		xrtGetAllocator(&Current);
		if ( (Current.Alloc != Custom.Alloc) ||
			(Current.Free != Custom.Free) ) {
			goto Cleanup;
		}
	}
	printf("allocator: locked after first alloc -> swap rejected ok\n");
	iResult = 0;

Cleanup:
	return iResult;
}
