#include "../test.h"



#if defined(__linux__)

typedef struct testuringoom {
	bool Fail;
	size_t Count;
	size_t FailAt;
	size_t MaxSize;
} testuringoom;



/* 正常阶段转发系统分配，故障阶段拒绝全部新对象。 */
static ptr testUringAlloc(ptr pData, size_t iSize)
{
	testuringoom* pState = (testuringoom*)pData;

	pState->Count++;
	if ( iSize > pState->MaxSize ) {
		pState->MaxSize = iSize;
	}
	if ( pState->Fail ||
		 ((pState->FailAt != 0) && (pState->Count == pState->FailAt)) ) {
		return NULL;
	}
	return malloc(iSize);
}



/* 完成故障分配器契约，但本场景不依赖调整已有内存。 */
static ptr testUringRealloc(ptr pData, ptr pMemory, size_t iSize)
{
	testuringoom* pState = (testuringoom*)pData;

	pState->Count++;
	if ( iSize > pState->MaxSize ) {
		pState->MaxSize = iSize;
	}
	if ( pState->Fail ||
		 ((pState->FailAt != 0) && (pState->Count == pState->FailAt)) ) {
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放故障开启前已经取得的底层内存。 */
static void testUringFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 操作缓存复用与分配失败都不能发布幽灵 SQE 或遗留活动 ID。 */
int main(void)
{
	testuringoom State;
	xallocator Allocator;
	xnetportconfig Config;
	xnetportevent Event;
	xnetwspan Spans[64];
	xnetport* pPort;
	xnetsocket Server;
	xnetsocket Client;
	xnetaddr Address;
	char Data[64];
	size_t iCount;
	size_t iSent;

	memset(&State, 0, sizeof(State));
	Allocator.Context = &State;
	Allocator.Alloc = testUringAlloc;
	Allocator.Realloc = testUringRealloc;
	Allocator.Free = testUringFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"io_uring failure allocator install failed"
	);

	/* 逐分配点验证端口、后端上下文与桶表失败都能完整回滚。 */
	for ( size_t i = 1; i <= 16; i++ ) {
		State.Count = 0;
		State.FailAt = i;
		xrtNetPortConfigInit(&Config);
		Config.Backend = XNET_PORT_URING;
		pPort = xrtNetPortCreate(&Config);
		if ( pPort != NULL ) {
			State.FailAt = 0;
			testRequire(
				xrtNetPortDestroy(pPort),
				"io_uring OOM sweep cleanup failed"
			);
			pPort = NULL;
			break;
		}
		testRequire(
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
			"io_uring initialization OOM error mismatch"
		);
		xrtClearError();
	}
	testRequire(
		(State.FailAt == 0) && (pPort == NULL),
		"io_uring initialization OOM sweep did not reach success"
	);

	State.Count = 0;
	State.FailAt = 0;
	State.MaxSize = 0;
	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_URING;
	Config.OperationLimit = 32768u;
	Config.OperationCache = 1;
	pPort = xrtNetPortCreate(&Config);
	testRequire(
		(pPort != NULL) && (State.MaxSize < 65536u),
		"idle io_uring port preallocated its high operation index"
	);
	Server = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	Client = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(
		(Server != NULL) && (Client != NULL),
		"io_uring OOM setup failed"
	);
	testRequire(
		xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Server, &Address) &&
		xrtNetSocketLocal(Server, &Address),
		"io_uring cache UDP bind failed"
	);

	/* 第一次完成把小操作描述符放入缓存。 */
	testRequire(
		xrtNetPortRecvFrom(pPort, Server, Data, 1, 1, NULL) &&
		(xrtNetSocketSendTo(Client, "A", 1, &iSent, &Address) ==
			XNET_RESULT_OK) && (iSent == 1),
		"io_uring cache warmup submit failed"
	);
	iCount = 0;
	testRequire(
		(xrtNetPortWait(
			pPort,
			&Event,
			1,
			xrtDeadlineAfter(1000000),
			&iCount
		) == XNET_RESULT_OK) &&
		(iCount == 1) && (Event.Id == 1) && (Event.Bytes == 1) &&
		(Data[0] == 'A'),
		"io_uring cache warmup completion mismatch"
	);

	/* 禁止新分配后，同尺寸操作只能依靠端口缓存完成。 */
	State.Fail = true;
	testRequire(
		xrtNetPortRecvFrom(pPort, Server, Data, 1, 2, NULL) &&
		(xrtNetSocketSendTo(Client, "B", 1, &iSent, &Address) ==
			XNET_RESULT_OK) && (iSent == 1),
		"io_uring cached operation requested new memory"
	);
	iCount = 0;
	testRequire(
		(xrtNetPortWait(
			pPort,
			&Event,
			1,
			xrtDeadlineAfter(1000000),
			&iCount
		) == XNET_RESULT_OK) &&
		(iCount == 1) && (Event.Id == 2) && (Event.Bytes == 1) &&
		(Data[0] == 'B'),
		"io_uring cached operation completion mismatch"
	);

	for ( size_t i = 0; i < 64; i++ ) {
		Spans[i].Data = (bytes)&Data[i];
		Spans[i].Size = 1;
	}

	testRequire(
		!xrtNetPortRecvFromVec(pPort, Server, Spans, 64, 3, NULL),
		"io_uring uncached size class unexpectedly survived OOM"
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"io_uring operation OOM error mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtNetPortWait(
			pPort,
			&Event,
			1,
			xrtDeadlineAfter(0),
			&iCount
		) == XNET_RESULT_TIMEOUT) &&
		(iCount == 0),
		"failed io_uring submit left a ghost completion"
	);

	testRequire(
		xrtNetSocketClose(Client) &&
		xrtNetSocketClose(Server) &&
		xrtNetPortDestroy(pPort),
		"io_uring OOM cleanup failed"
	);

	/* 零上限必须真正关闭缓存。 */
	State.Fail = false;
	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_URING;
	Config.OperationCache = 0;
	pPort = xrtNetPortCreate(&Config);
	Server = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(
		(pPort != NULL) && (Server != NULL),
		"io_uring disabled cache setup failed"
	);
	State.Fail = true;
	testRequire(
		!xrtNetPortRecvFromVec(
			pPort,
			Server,
			Spans,
			64,
			4,
			NULL
		) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"zero io_uring operation cache still retained memory"
	);
	xrtClearError();
	testRequire(
		xrtNetSocketClose(Server) && xrtNetPortDestroy(pPort),
		"io_uring disabled cache cleanup failed"
	);
	return 0;
}

#else

/* 非 Linux 构建由主测试覆盖 unavailable 契约。 */
int main(void)
{
	testRequire(true, "non-Linux io_uring OOM placeholder");
	return 0;
}

#endif
