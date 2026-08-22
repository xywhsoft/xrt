#include "../test.h"



/* 可切换分配器用于向缓冲扩容路径注入失败。 */
typedef struct testbufferoomstate {
	bool Fail;
} testbufferoomstate;



/* 在允许状态下转发到底层 C 分配器。 */
static ptr testBufferOomAlloc(ptr pContext, size_t iSize)
{
	testbufferoomstate* pState = (testbufferoomstate*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 在允许状态下转发到底层 C 重分配器。 */
static ptr testBufferOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testbufferoomstate* pState = (testbufferoomstate*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器取得的内存。 */
static void testBufferOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 在底层分配已关闭时持有并耗尽缓冲结构尺寸类。 */
static size_t testBufferOomExhaust(xbuffer** pBuffers, size_t iCapacity)
{
	size_t iCount = 0;

	while ( iCount < iCapacity ) {
		xbuffer* pBuffer = xrtBufferCreate();

		if ( pBuffer == NULL ) {
			break;
		}
		pBuffers[iCount++] = pBuffer;
	}
	testRequire(iCount < iCapacity, "buffer OOM structure cache did not exhaust");
	return iCount;
}



/* 释放耗尽测试持有的全部缓冲结构。 */
static void testBufferOomRelease(xbuffer** pBuffers, size_t iCount)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		xrtBufferDestroy(pBuffers[i]);
	}
}



/* 验证所有分配失败路径保持缓冲和来源所有权不变。 */
int main(void)
{
	testbufferoomstate tState = { false };
	xallocator tAllocator;
	xbuffer tBuffer;
	xbuffer* pCreated;
	xbuffer* pHeld[512];
	bytes pData;
	bytes pOwned;
	size_t iSize;
	size_t iCapacity;
	size_t iHeld;
	unsigned char pLarge[256];

	tAllocator.Context = &tState;
	tAllocator.Alloc = testBufferOomAlloc;
	tAllocator.Realloc = testBufferOomRealloc;
	tAllocator.Free = testBufferOomFree;
	testRequire(
		xrtSetAllocator(&tAllocator),
		"failed to install buffer OOM allocator"
	);
	memset(pLarge, 0x5a, sizeof(pLarge));
	testRequire(xrtBufferInit(&tBuffer), "OOM buffer init failed");
	testRequire(
		xrtBufferAppend(&tBuffer, XRT_BYTES_LITERAL("12345678")),
		"OOM buffer setup failed"
	);
	testRequire(xrtBufferTrim(&tBuffer), "OOM buffer trim failed");
	pData = tBuffer.Data;
	iSize = tBuffer.Size;
	iCapacity = tBuffer.Capacity;

	tState.Fail = true;
	testRequire(
		!xrtBufferAppend(&tBuffer, XRT_BYTES_LITERAL("x")),
		"buffer append should fail under OOM"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"buffer append OOM error mismatch"
	);
	testRequire(
		(tBuffer.Data == pData) && (tBuffer.Size == iSize) &&
		(tBuffer.Capacity == iCapacity) &&
		(memcmp(tBuffer.Data, "12345678", 8) == 0),
		"buffer append OOM changed state"
	);

	testRequire(
		!xrtBufferWrite(
			&tBuffer,
			128,
			(xbytesview){ pLarge, sizeof(pLarge) }
		),
		"sparse buffer write should fail under OOM"
	);
	testRequire(
		(tBuffer.Data == pData) && (tBuffer.Size == iSize) &&
		(tBuffer.Capacity == iCapacity) &&
		(memcmp(tBuffer.Data, "12345678", 8) == 0),
		"sparse buffer write OOM changed state"
	);
	testRequire(
		!xrtBufferAssign(
			&tBuffer,
			(xbytesview){ pLarge, sizeof(pLarge) }
		),
		"buffer assign should fail under OOM"
	);
	testRequire(
		(tBuffer.Data == pData) && (tBuffer.Size == iSize) &&
		(tBuffer.Capacity == iCapacity),
		"buffer assign OOM changed state"
	);
	iHeld = testBufferOomExhaust(pHeld, 512);
	pCreated = xrtBufferCreate();
	testRequire(pCreated == NULL, "exhausted buffer create should fail under OOM");
	testBufferOomRelease(pHeld, iHeld);

	tState.Fail = false;
	pOwned = (bytes)xrtMalloc(16);
	testRequire(pOwned != NULL, "buffer create take setup failed");
	memset(pOwned, 0x7c, 16);
	tState.Fail = true;
	iHeld = testBufferOomExhaust(pHeld, 512);
	pCreated = xrtBufferCreateTake(&pOwned, 8, 16);
	testRequire(pCreated == NULL, "buffer create take should fail under OOM");
	testRequire(pOwned != NULL, "failed buffer create take consumed source");
	tState.Fail = false;
	testBufferOomRelease(pHeld, iHeld);
	xrtFree(pOwned);
	xrtBufferUnit(&tBuffer);
	printf("[PASS] buffer OOM\n");
	return 0;
}
