#ifndef XRT_TEST_ALLOCATOR_H
#define XRT_TEST_ALLOCATOR_H

#include "test.h"



/* 故障注入分配器拒绝所有动态内存请求。 */
static ptr testFailAlloc(ptr pContext, size_t iSize)
{
	(void)pContext;
	(void)iSize;
	return NULL;
}



/* 故障注入重分配器拒绝所有动态内存请求。 */
static ptr testFailRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	(void)pContext;
	(void)pMemory;
	(void)iSize;
	return NULL;
}



/* 全失败分配器没有实际内存需要释放。 */
static void testFailFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	(void)pMemory;
}



/* 安装拒绝所有请求的分配器，用于验证 OOM 边界。 */
static bool testInstallFailAllocator(void)
{
	xallocator tAllocator;

	tAllocator.Context = NULL;
	tAllocator.Alloc = testFailAlloc;
	tAllocator.Realloc = testFailRealloc;
	tAllocator.Free = testFailFree;
	return xrtSetAllocator(&tAllocator);
}

#endif
