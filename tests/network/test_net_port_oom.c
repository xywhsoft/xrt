#if !defined(TEST_PORT_BACKEND)
	#define TEST_PORT_BACKEND XNET_PORT_SELECT
	#define TEST_PORT_AVAILABLE 1
#endif



#if TEST_PORT_AVAILABLE

#include "../test.h"



/* 可切换故障分配器用于验证端口失败原子性。 */
typedef struct testportoom {
	bool Fail;
	ptr Blocks[16384];
	size_t BlockCount;
	size_t MaxSize;
} testportoom;



/* 正常阶段转发系统分配，故障阶段拒绝新对象。 */
static ptr testPortOomAlloc(ptr pData, size_t iSize)
{
	testportoom* pState = (testportoom*)pData;

	if ( iSize > pState->MaxSize ) {
		pState->MaxSize = iSize;
	}
	return pState->Fail ? NULL : malloc(iSize);
}



/* 端口测试不依赖重分配，但仍实现完整分配器契约。 */
static ptr testPortOomRealloc(ptr pData, ptr pMemory, size_t iSize)
{
	testportoom* pState = (testportoom*)pData;

	if ( iSize > pState->MaxSize ) {
		pState->MaxSize = iSize;
	}
	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放正常阶段创建的全部对象。 */
static void testPortOomFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 耗尽小对象堆前八个尺寸类，使后续节点分配真正到达故障分配器。 */
static void testPortOomExhaust(testportoom* pState)
{
	for ( size_t iSize = 16; iSize <= 128; iSize += 16 ) {
		for ( ;; ) {
			ptr pMemory;

			testRequire(pState->BlockCount <
				(sizeof(pState->Blocks) / sizeof(pState->Blocks[0])),
				"network port OOM exhaustion storage is too small");
			pMemory = xrtMalloc(iSize);
			if ( pMemory == NULL ) {
				xrtClearError();
				break;
			}
			pState->Blocks[pState->BlockCount++] = pMemory;
		}
	}
}



/* 释放用于阻塞尺寸类的测试对象。 */
static void testPortOomRelease(testportoom* pState)
{
	while ( pState->BlockCount != 0 ) {
		xrtFree(pState->Blocks[--pState->BlockCount]);
	}
}



/* 创建、观察和投递 OOM 均不得留下可见对象或幽灵事件。 */
int main(void)
{
	testportoom State;
	xallocator Allocator;
	xnetportconfig Config;
	xnetportevent Event;
	xnetport* pPort;
	xnetsocket Socket;
	size_t iCount = 9;

	memset(&State, 0, sizeof(State));
	Allocator.Context = &State;
	Allocator.Alloc = testPortOomAlloc;
	Allocator.Realloc = testPortOomRealloc;
	Allocator.Free = testPortOomFree;
	testRequire(xrtSetAllocator(&Allocator),
		"network port OOM allocator install failed");
	xrtNetPortConfigInit(&Config);
	Config.Backend = TEST_PORT_BACKEND;

	State.Fail = true;
	testRequire(xrtNetPortCreate(&Config) == NULL,
		"network port create unexpectedly survived OOM");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"network port create OOM error mismatch");
	xrtClearError();

	State.Fail = false;
	State.MaxSize = 0;
	Config.WatchLimit = 1024u * 1024u;
	pPort = xrtNetPortCreate(&Config);
	Socket = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	testRequire((pPort != NULL) && (Socket != NULL),
		"network port OOM setup failed");
	testRequire(State.MaxSize < (256u * 1024u),
		"network port preallocated its full watch index");

	State.Fail = true;
	testPortOomExhaust(&State);
	testRequire(!xrtNetPortWatch(pPort, Socket,
		1, XNET_POLL_READ, NULL),
		"network port watch unexpectedly survived OOM");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"network port watch OOM error mismatch");
	xrtClearError();
	testPortOomExhaust(&State);
	testRequire(!xrtNetPortPost(pPort, 7, NULL),
		"network port post unexpectedly survived OOM");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"network port post OOM error mismatch");
	xrtClearError();
	testRequire((xrtNetPortWait(pPort, &Event, 1,
		xrtDeadlineAfter(0), &iCount) == XNET_RESULT_TIMEOUT) &&
		(iCount == 0), "failed network port post left a ghost event");

	State.Fail = false;
	testPortOomRelease(&State);
	testRequire(xrtNetPortWatch(pPort, Socket,
		1, XNET_POLL_READ, NULL),
		"network port watch did not recover after OOM");
	testRequire(xrtNetPortUnwatch(pPort, Socket),
		"network port unwatch after OOM failed");
	testRequire(xrtNetSocketClose(Socket),
		"network port OOM socket close failed");
	testRequire(xrtNetPortDestroy(pPort),
		"network port destroy after OOM failed");
	return 0;
}

#else

/* 当前平台不提供目标后端时不执行故障注入。 */
int main(void)
{
	return 0;
}

#endif
