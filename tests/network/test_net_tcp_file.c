#include "../test.h"



typedef struct testtcpfile {
	xnetstream* Client;
	xnetstream* Server;
	xatomic32 Accepted;
	xatomic32 Opened;
	xatomic32 Received;
	xatomic32 Closed;
	bytes Data;
	size_t Capacity;
} testtcpfile;



#define TEST_TCP_FILE_SOURCE_SIZE ((size_t)131072u)
#define TEST_TCP_FILE_OFFSET ((size_t)17u)
#define TEST_TCP_FILE_RANGE ((size_t)100000u)
#define TEST_TCP_FILE_EXPECTED (TEST_TCP_FILE_RANGE + 2u)



/* 在测试截止时间内等待原子计数到达目标。 */
static void testTcpFileWait(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(5000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 等待发送 Worker 发布最终预算扣减，避免把对端接收误作本端完成屏障。 */
static void testTcpFileDrain(xnetstream* pStream)
{
	xdeadline Deadline = xrtDeadlineAfter(5000000u);

	while ( xrtNetStreamPending(pStream) != 0 ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"TCP file send budget did not drain"
		);
		xrtThreadYield();
	}
}



/* 记录客户端和服务端已经发布。 */
static void testTcpFileOpen(xnetstream* pStream, ptr pData)
{
	testtcpfile* pTest = (testtcpfile*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(&pTest->Opened, 1, XMEMORY_RELEASE);
}



/* 服务端完整消费内存、文件和尾部内存组成的有序字节流。 */
static void testTcpFileRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	testtcpfile* pTest = (testtcpfile*)pData;
	uint32 iOffset = xrtAtomic32Load(&pTest->Received, XMEMORY_RELAXED);
	size_t iSize = xrtNetBufSize(pBuffer);

	(void)pStream;
	testRequire(iOffset <= pTest->Capacity,
		"TCP file receive offset overflowed");
	testRequire(iSize <= (pTest->Capacity - iOffset),
		"TCP file receive buffer overflowed");
	testRequire(xrtNetBufRead(
		pBuffer,
		pTest->Data + iOffset,
		iSize
	) == iSize, "TCP file receive consume failed");
	(void)xrtAtomic32FetchAdd(
		&pTest->Received,
		(uint32)iSize,
		XMEMORY_RELEASE
	);
}



/* 记录 Stream 唯一终态。 */
static void testTcpFileClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testtcpfile* pTest = (testtcpfile*)pData;

	(void)pStream;
	if ( (Result != XNET_RESULT_OK) || (pError != NULL) ) {
		fprintf(
			stderr,
			"[DIAG] TCP file close: side=%s result=%d kind=%d operation=%s "
			"message=%s system=%d\n",
			(pStream == pTest->Client) ? "client" : "server",
			(int)Result,
			(pError != NULL) ? (int)xrtErrorKind(pError) : 0,
			(pError != NULL) ? xrtErrorOperation(pError) : "none",
			(pError != NULL) ? xrtErrorMessage(pError) : "none",
			(pError != NULL) ? xrtErrorSystemCode(pError) : 0
		);
	}
	testRequire((Result == XNET_RESULT_OK) && (pError == NULL),
		"TCP file stream closed with error");
	(void)xrtAtomic32FetchAdd(&pTest->Closed, 1, XMEMORY_RELEASE);
}



/* 接管服务端 Stream 并切换到测试事件。 */
static bool testTcpFileAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testtcpfile* pTest = (testtcpfile*)pData;
	xnetstreamevents Events;

	(void)pListener;
	memset(&Events, 0, sizeof(Events));
	Events.Open = testTcpFileOpen;
	Events.Read = testTcpFileRead;
	Events.Close = testTcpFileClose;
	testRequire(xrtNetStreamSetEvents(pStream, &Events, pTest),
		"TCP file accepted event takeover failed");
	pTest->Server = pStream;
	(void)xrtAtomic32FetchAdd(&pTest->Accepted, 1, XMEMORY_RELEASE);
	return true;
}



/* 验证文件区间的顺序、句柄独立、范围、背压和完整回收。 */
int main(void)
{
	static const char sPath[] = "test_net_tcp_file.tmp";
	testtcpfile Test;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetstreamconfig StreamConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents ClientEvents;
	xnetlistener* pListener;
	xnetengine* pEngine;
	xnetaddr Address;
	xfile File;
	bytes pSource;
	size_t iDone = 0;

	memset(&Test, 0, sizeof(Test));
	Test.Capacity = TEST_TCP_FILE_EXPECTED;
	Test.Data = (bytes)xrtMalloc(Test.Capacity);
	pSource = (bytes)xrtMalloc(TEST_TCP_FILE_SOURCE_SIZE);
	testRequire((Test.Data != NULL) && (pSource != NULL),
		"TCP file fixture allocation failed");
	for ( size_t i = 0; i < TEST_TCP_FILE_SOURCE_SIZE; i++ ) {
		pSource[i] = (uint8)('A' + (i % 23u));
	}
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	ListenerEvents.Accept = testTcpFileAccept;
	ClientEvents.Open = testTcpFileOpen;
	ClientEvents.Close = testTcpFileClose;

	File = xrtOpen(sPath, XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE);
	testRequire((File != NULL) && xrtWriteFull(
		File,
		pSource,
		TEST_TCP_FILE_SOURCE_SIZE,
		&iDone
	) && (iDone == TEST_TCP_FILE_SOURCE_SIZE) && xrtClose(File),
		"TCP file fixture creation failed");
	File = xrtOpen(sPath, XFILE_READ);
	testRequire(File != NULL, "TCP file fixture open failed");

	xrtNetEngineConfigInit(&EngineConfig);
	#if defined(TEST_TCP_FILE_BACKEND)
		EngineConfig.Backend = TEST_TCP_FILE_BACKEND;
	#else
		EngineConfig.Backend = XNET_PORT_AUTO;
	#endif
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP file engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP file listener address failed");
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&Test
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address),
		"TCP file listener start failed");

	xrtNetStreamConfigInit(&StreamConfig);
	StreamConfig.WriteHighWater = 32768u;
	StreamConfig.WriteLowWater = 16384u;
	StreamConfig.WriteLimit = TEST_TCP_FILE_EXPECTED;
	Test.Client = xrtNetStreamConnect(
		pEngine,
		&Address,
		1,
		&StreamConfig,
		&ClientEvents,
		&Test
	);
	testRequire(Test.Client != NULL, "TCP file client creation failed");
	testTcpFileWait(&Test.Accepted, 1, "TCP file accept missing");
	testTcpFileWait(&Test.Opened, 2, "TCP file streams did not open");

	testRequire(xrtNetStreamSendFile(
		Test.Client,
		File,
		0,
		TEST_TCP_FILE_EXPECTED + 1u
	) == XNET_RESULT_AGAIN, "TCP file write limit was not enforced");
	xrtClearError();
	testRequire(xrtNetStreamSendFile(
		Test.Client,
		File,
		TEST_TCP_FILE_SOURCE_SIZE - 1u,
		2u
	) == XNET_RESULT_ERROR, "TCP file invalid range was accepted");
	xrtClearError();
	testRequire(xrtNetStreamSend(Test.Client, "A", 1) == XNET_RESULT_OK,
		"TCP file prefix send failed");
	testRequire(xrtNetStreamSendFile(
		Test.Client,
		File,
		TEST_TCP_FILE_OFFSET,
		TEST_TCP_FILE_RANGE
	) == XNET_RESULT_OK, "TCP file range send failed");
	testRequire(xrtClose(File),
		"TCP file source close after submit failed");
	File = NULL;
	testRequire(xrtNetStreamSend(Test.Client, "Z", 1) == XNET_RESULT_OK,
		"TCP file suffix send failed");
	testTcpFileWait(&Test.Received, (uint32)TEST_TCP_FILE_EXPECTED,
		"TCP file range was not received");
	testRequire((Test.Data[0] == 'A') &&
		(memcmp(
			Test.Data + 1u,
			pSource + TEST_TCP_FILE_OFFSET,
			TEST_TCP_FILE_RANGE
		) == 0) &&
		(Test.Data[TEST_TCP_FILE_EXPECTED - 1u] == 'Z'),
		"TCP file range ordering mismatch");
	testTcpFileDrain(Test.Client);

	testRequire(xrtNetStreamClose(Test.Client),
		"TCP file client close failed");
	testRequire(xrtNetStreamClose(Test.Server),
		"TCP file server close failed");
	testTcpFileWait(&Test.Closed, 2, "TCP file streams did not close");
	testRequire(xrtNetListenerClose(pListener),
		"TCP file listener close failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetStreamDestroy(Test.Client);
	xrtNetStreamDestroy(Test.Server);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineStop(pEngine), "TCP file engine stop failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP file engine destroy failed");
	testRequire(remove(sPath) == 0, "TCP file fixture cleanup failed");
	xrtFree(pSource);
	xrtFree(Test.Data);
	printf("[PASS] network TCP file send\n");
	return 0;
}
