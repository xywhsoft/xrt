#include "../test.h"



/* 可切换分配器记录底层分配调用，并在启用时全部拒绝。 */
typedef struct testbuffercodecoom {
	bool Fail;
	size_t Calls;
} testbuffercodecoom;



/* 在允许状态下转发到底层 C 分配器。 */
static ptr testBufferCodecOomAlloc(ptr pContext, size_t iSize)
{
	testbuffercodecoom* pState = (testbuffercodecoom*)pContext;

	pState->Calls++;
	return pState->Fail ? NULL : malloc(iSize);
}



/* 在允许状态下转发到底层 C 重分配器。 */
static ptr testBufferCodecOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testbuffercodecoom* pState = (testbuffercodecoom*)pContext;

	pState->Calls++;
	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器取得的底层内存。 */
static void testBufferCodecOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 预热缓冲结构尺寸类，使失败只落在大块结果分配。 */
static void testBufferCodecWarmStructure(void)
{
	xbuffer* pBuffer = xrtBufferCreate();

	testRequire(pBuffer != NULL, "buffer codec OOM warmup failed");
	xrtBufferDestroy(pBuffer);
}



/* 验证失败构造器已经把先取得的缓冲结构归还小块堆。 */
static void testBufferCodecReusedStructure(
	testbuffercodecoom* pState,
	size_t iCalls
)
{
	xbuffer* pBuffer = xrtBufferCreate();

	testRequire(
		pBuffer != NULL,
		"buffer codec OOM did not return structure to heap"
	);
	testRequire(
		pState->Calls == iCalls,
		"buffer codec structure reuse reached backing allocator"
	);
	xrtBufferDestroy(pBuffer);
}



/* 验证编码构造器大块结果分配失败时回收已经创建的缓冲结构。 */
int main(void)
{
	testbuffercodecoom tState = { false, 0 };
	xallocator tAllocator;
	xbuffer* pBuffer;
	size_t iCalls;

	tAllocator.Context = &tState;
	tAllocator.Alloc = testBufferCodecOomAlloc;
	tAllocator.Realloc = testBufferCodecOomRealloc;
	tAllocator.Free = testBufferCodecOomFree;
	testRequire(
		xrtSetAllocator(&tAllocator),
		"failed to install buffer codec OOM allocator"
	);
	testBufferCodecWarmStructure();

	#if defined(XRT_FEATURE_BUFFER_HEX)
		char sHex[4096];

		memset(sHex, '0', sizeof(sHex));
		tState.Fail = true;
		iCalls = tState.Calls;
		pBuffer = xrtBufferFromHex(
			(xstrview){ sHex, sizeof(sHex) },
			0
		);
		testRequire(pBuffer == NULL, "HEX buffer data allocation should fail");
		testRequire(
			tState.Calls == iCalls + 1u,
			"HEX buffer OOM did not reach one large allocation"
		);
		testBufferCodecReusedStructure(&tState, tState.Calls);
	#endif

	#if defined(XRT_FEATURE_BUFFER_BASE64)
		char sBase64[2048];

		memset(sBase64, 'A', sizeof(sBase64));
		tState.Fail = true;
		iCalls = tState.Calls;
		pBuffer = xrtBufferFromBase64(
			(xstrview){ sBase64, sizeof(sBase64) },
			NULL
		);
		testRequire(
			pBuffer == NULL,
			"Base64 buffer data allocation should fail"
		);
		testRequire(
			tState.Calls == iCalls + 1u,
			"Base64 buffer OOM did not reach one large allocation"
		);
		testBufferCodecReusedStructure(&tState, tState.Calls);
	#endif

	printf("[PASS] buffer codec OOM\n");
	return 0;
}
