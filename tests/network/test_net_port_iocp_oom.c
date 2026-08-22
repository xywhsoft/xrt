#include "../test.h"



#if defined(_WIN32) || defined(_WIN64)

/* 可切换故障分配器让既有对象正常释放，只拒绝后续新分配。 */
typedef struct testiocpoom {
	bool Fail;
	size_t MaxSize;
} testiocpoom;



/* 正常阶段转发系统分配，故障阶段拒绝新对象。 */
static ptr testIOCPAlloc(ptr pData, size_t iSize)
{
	testiocpoom* pState = (testiocpoom*)pData;

	if ( iSize > pState->MaxSize ) {
		pState->MaxSize = iSize;
	}
	return pState->Fail ? NULL : malloc(iSize);
}



/* IOCP 测试不调整内存，但仍提供完整分配器契约。 */
static ptr testIOCPRealloc(ptr pData, ptr pMemory, size_t iSize)
{
	testiocpoom* pState = (testiocpoom*)pData;

	if ( iSize > pState->MaxSize ) {
		pState->MaxSize = iSize;
	}
	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放正常阶段已经取得的底层内存。 */
static void testIOCPFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 预热尺寸类后禁止新分配，验证复用命中并保留不同尺寸类的真实 OOM。 */
int main(void)
{
	testiocpoom State;
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
	Allocator.Alloc = testIOCPAlloc;
	Allocator.Realloc = testIOCPRealloc;
	Allocator.Free = testIOCPFree;
	testRequire(xrtSetAllocator(&Allocator),
		"IOCP failure allocator install failed");

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_IOCP;
	Config.OperationLimit = 1024u * 1024u;
	Config.OperationCache = 1;
	pPort = xrtNetPortCreate(&Config);
	testRequire((pPort != NULL) && (State.MaxSize < 65536u),
		"idle IOCP port preallocated its high operation limit");
	Server = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	Client = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	testRequire((Server != NULL) && (Client != NULL),
		"IOCP OOM setup failed");
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Server, &Address) &&
		xrtNetSocketLocal(Server, &Address),
		"IOCP cache UDP bind failed");

	/* 第一次完成把小操作描述符放入缓存。 */
	testRequire(xrtNetPortRecvFrom(pPort, Server, Data, 1, 1, NULL) &&
		(xrtNetSocketSendTo(Client, "A", 1, &iSent, &Address) ==
			XNET_RESULT_OK) && (iSent == 1),
		"IOCP cache warmup submit failed");
	iCount = 0;
	testRequire((xrtNetPortWait(pPort, &Event, 1,
		xrtDeadlineAfter(1000000), &iCount) == XNET_RESULT_OK) &&
		(iCount == 1) && (Event.Id == 1) && (Event.Bytes == 1) &&
		(Data[0] == 'A'), "IOCP cache warmup completion mismatch");

	/* 禁止堆分配后，同尺寸操作只能依靠端口缓存完成。 */
	State.Fail = true;
	testRequire(xrtNetPortRecvFrom(pPort, Server, Data, 1, 2, NULL) &&
		(xrtNetSocketSendTo(Client, "B", 1, &iSent, &Address) ==
			XNET_RESULT_OK) && (iSent == 1),
		"IOCP cached operation requested new memory");
	iCount = 0;
	testRequire((xrtNetPortWait(pPort, &Event, 1,
		xrtDeadlineAfter(1000000), &iCount) == XNET_RESULT_OK) &&
		(iCount == 1) && (Event.Id == 2) && (Event.Bytes == 1) &&
		(Data[0] == 'B'), "IOCP cached operation completion mismatch");

	for ( size_t i = 0; i < 64; i++ ) {
		Spans[i].Data = (bytes)&Data[i];
		Spans[i].Size = 1;
	}

	testRequire(!xrtNetPortRecvFromVec(pPort, Server,
		Spans, 64, 1, NULL),
		"IOCP uncached size class unexpectedly survived OOM");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"IOCP operation OOM error mismatch");
	xrtClearError();
	testRequire((xrtNetPortWait(pPort, &Event, 1,
		xrtDeadlineAfter(0), &iCount) == XNET_RESULT_TIMEOUT) &&
		(iCount == 0), "failed IOCP submit left a ghost completion");

	/* 故障分配器只影响新内存，既有端口和 Socket 仍可完整销毁。 */
	testRequire(xrtNetSocketClose(Client) && xrtNetSocketClose(Server),
		"IOCP OOM socket close failed");
	testRequire(xrtNetPortDestroy(pPort),
		"IOCP OOM port destroy failed");

	/* 零上限必须真正关闭缓存，不能隐式保留后端对象。 */
	State.Fail = false;
	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_IOCP;
	Config.OperationCache = 0;
	pPort = xrtNetPortCreate(&Config);
	Server = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	testRequire((pPort != NULL) && (Server != NULL),
		"IOCP disabled cache setup failed");
	State.Fail = true;
	testRequire(!xrtNetPortRecvFromVec(
		pPort,
		Server,
		Spans,
		64,
		3,
		NULL
	) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"zero IOCP operation cache still retained memory");
	xrtClearError();
	testRequire(xrtNetSocketClose(Server) && xrtNetPortDestroy(pPort),
		"IOCP disabled cache cleanup failed");
	return 0;
}

#else

/* 非 Windows 构建由主 IOCP 测试覆盖 unavailable 契约。 */
int main(void)
{
	return 0;
}

#endif
