#include "../test.h"
#include "../../../../tests/test_fault_allocator.h"



/* 固定 padding 使故障测试只改变分配结果，不改变线路报文。 */
static bool testSshSessionReaderOomPadding(
	void* pOutput,
	size_t iSize,
	ptr pUserData
)
{
	bytes pBytes = (bytes)pOutput;
	size_t i;

	(void)pUserData;
	for ( i = 0u; i < iSize; ++i ) {
		pBytes[i] = (uint8)(0xa0u + (uint8)i);
	}
	return true;
}



/* 直接提交双方版本，建立接收 peer KEXINIT 所需的 transcript。 */
static void testSshSessionReaderOomVersions(xsshsessiontcp* pSession)
{
	static const char sClient[] = "SSH-2.0-xssh_oom_client";
	static const char sServer[] = "SSH-2.0-xssh_oom_server";
	xsshtransportcore* pCore = &pSession->Transport.Core;

	testRequire((xrtSshSessionCoreVersionPrepare(
		&pSession->Session,
		pCore,
		XSSH_TRANSPORT_LOCAL,
		XRT_STR_LITERAL(sClient)
	) == XSSH_OK) && (xrtSshTransportCoreIdentificationCommit(
		pCore,
		XSSH_TRANSPORT_LOCAL
	) == XSSH_OK) && (xrtSshSessionCoreVersionCommit(
		&pSession->Session,
		pCore
	) == XSSH_OK) && (xrtSshSessionCoreVersionPrepare(
		&pSession->Session,
		pCore,
		XSSH_TRANSPORT_PEER,
		XRT_STR_LITERAL(sServer)
	) == XSSH_OK) && (xrtSshTransportCoreIdentificationCommit(
		pCore,
		XSSH_TRANSPORT_PEER
	) == XSSH_OK) && (xrtSshSessionCoreVersionCommit(
		&pSession->Session,
		pCore
	) == XSSH_OK), "ssh reader OOM versions failed");
}



/* 构建一个完整 plain KEXINIT packet，供 transcript 分配故障重试。 */
static size_t testSshSessionReaderOomWire(
	void* pOutput,
	size_t iCapacity
)
{
	unsigned char arrPayload[8192];
	uint8 arrCookie[XSSH_KEX_COOKIE_SIZE];
	char arrLanguage[4096];
	xsshkexinitconfig Config;
	xsshwriter Payload;
	xsshwriter Wire;
	uint32 iSequence = 0u;
	size_t i;

	for ( i = 0u; i < sizeof(arrCookie); ++i ) {
		arrCookie[i] = (uint8)(0x30u + (uint8)i);
	}
	memset(arrLanguage, 'l', sizeof(arrLanguage));
	testRequire(xrtSshKexInitConfigInit(
		&Config,
		XSSH_ROLE_SERVER,
		true
	), "ssh reader OOM KEXINIT configuration failed");
	Config.LanguagesClientToServer = (xstrview){
		arrLanguage,
		sizeof(arrLanguage)
	};
	testRequire(xrtSshWriterInit(
		&Payload,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshKexInitWrite(
		&Payload,
		(xbytesview){ arrCookie, sizeof(arrCookie) },
		&Config
	) == XSSH_OK) && xrtSshWriterInit(
		&Wire,
		pOutput,
		iCapacity
	) && (xrtSshPacketWrite(
		&Wire,
		(xbytesview){ arrPayload, Payload.Size },
		8u,
		&iSequence,
		testSshSessionReaderOomPadding,
		NULL
	) == XSSH_OK), "ssh reader OOM wire build failed");
	return Wire.Size;
}



/* 通用动态分配失败必须保留事务，并与 host-key 容量请求明确区分。 */
int main(void)
{
	static testfaultallocator State = { 0u, SIZE_MAX, 0u, false };
	unsigned char arrWire[8192];
	xallocator Allocator = testFaultAllocator(&State);
	xnetbufpoolconfig PoolConfig;
	xnetbufpoolinfo PoolInfo;
	xsshsessiontcpconfig Config;
	xsshsessiontcppacket Packet;
	xsshsessionreader Reader;
	xsshsessiontcp Session;
	xsshrekeydecision Decision;
	xnetbufpool* pPool;
	xnetbuf Input;
	xsshcode Code;
	size_t iBaselineLive;
	size_t iWireSize;
	size_t i;

	#if defined(XRT_FEATURE_MEMORY_DEBUG)
	/* 本测试统计底层物理块，不能把内存调试隔离区计作 Reader 存储。 */
	testRequire(xrtMemDebugEnable(false),
		"ssh reader OOM memory debug isolation failed");
	#endif
	testRequire(xrtSetAllocator(&Allocator),
		"ssh reader OOM allocator install failed");
	xrtNetBufPoolConfigInit(&PoolConfig);
	for ( i = 0u; i < XNET_BUFFER_CLASS_COUNT; ++i ) {
		PoolConfig.CacheLimit[i] = 0u;
	}
	PoolConfig.MaxCacheBytes = 1u;
	pPool = xrtNetBufPoolCreate(&PoolConfig);
	testRequire(pPool != NULL, "ssh reader OOM pool creation failed");
	testRequire(xrtSshSessionTcpConfigInit(
		&Config,
		XSSH_ROLE_CLIENT
	), "ssh reader OOM configuration failed");
	testRequire(xrtSshSessionTcpInit(
		&Session,
		pPool,
		&Config,
		0u
	), "ssh reader OOM session initialization failed");
	testRequire(xrtSshSessionReaderInit(
		&Reader,
		pPool,
		&Session
	), "ssh reader OOM reader initialization failed");
	testRequire(xrtNetBufInit(&Input, pPool),
		"ssh reader OOM input initialization failed");
	testSshSessionReaderOomVersions(&Session);
	iWireSize = testSshSessionReaderOomWire(
		arrWire,
		sizeof(arrWire)
	);
	iBaselineLive = State.Live;
	testRequire(xrtNetBufAppend(&Input, arrWire, iWireSize),
		"ssh reader OOM input append failed");

	State.FailAt = State.Calls + 1u;
	State.Hit = false;
	Code = xrtSshSessionReaderPrepare(
		&Reader,
		&Input,
		0u,
		&Packet
	);
	testRequire(State.Hit && (Code == XSSH_ERROR_SPACE) &&
		(xrtSshSessionReaderState(&Reader) ==
		 XSSH_SESSION_READER_RETRY) &&
		(xrtNetBufSize(&Input) == iWireSize) &&
		(xrtSshSessionTcpPhase(&Session) != XSSH_SESSION_FAILED),
		"ssh reader confused generic OOM with host-key capacity");
	xrtClearError();

	State.FailAt = SIZE_MAX;
	testRequire((xrtSshSessionReaderPrepare(
		&Reader,
		&Input,
		0u,
		&Packet
	) == XSSH_OK) &&
		(Packet.Session.Kind == XSSH_SESSION_PACKET_KEXINIT) &&
		(xrtSshSessionReaderCommit(
			&Reader,
			0u,
			&Decision
		) == XSSH_OK) && xrtNetBufEmpty(&Input) &&
		(xrtSshSessionReaderState(&Reader) ==
		 XSSH_SESSION_READER_IDLE),
		"ssh reader did not recover from generic OOM");

	xrtNetBufClear(&Input);
	xrtSshSessionReaderClear(&Reader);
	xrtSshSessionTcpClear(&Session);
	xrtNetBufPoolGet(pPool, &PoolInfo);
	testRequire((PoolInfo.LiveBlocks == 0u) &&
		(PoolInfo.LiveBytes == 0u) && xrtNetBufPoolDestroy(pPool) &&
		(State.Live == iBaselineLive),
		"ssh reader OOM retry leaked storage");
	return 0;
}
