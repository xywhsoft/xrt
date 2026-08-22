#include "../test.h"

#include "../../../../src/internal/xrt_tcp.h"
#include "../../src/internal/xrt_websocket.h"



#ifndef TEST_WS_CONNECTION_BACKEND
	#define TEST_WS_CONNECTION_BACKEND XNET_PORT_SELECT
#endif

#define TEST_WS_CONNECTION_MESSAGE_LIMIT ((size_t)262144u)
#define TEST_WS_CONNECTION_BACKPRESSURE_SIZE ((size_t)65536u)
#define TEST_WS_CONNECTION_OOM_SIZE ((size_t)32768u)
#define TEST_WS_CONNECTION_OOM_BLOCKS ((size_t)65536u)
#define TEST_WS_CONNECTION_SEND_LIMIT ((size_t)65536u)



#ifndef TEST_WS_CONNECTION_CLOSE_CODE
	#define TEST_WS_CONNECTION_CLOSE_CODE XWS_CLOSE_NORMAL
#endif

#ifndef TEST_WS_CONNECTION_CLOSE_REASON
	#define TEST_WS_CONNECTION_CLOSE_REASON "done"
#endif

#ifndef TEST_WS_CONNECTION_CLOSE_REASON_SIZE
	#define TEST_WS_CONNECTION_CLOSE_REASON_SIZE ((size_t)4u)
#endif



typedef struct test_ws_connection test_ws_connection;



typedef struct test_ws_endpoint {
	test_ws_connection* Test;
	xwsrole Role;
	xatomic32 Begun;
	xatomic32 Messages;
	xatomic32 Ping;
	xatomic32 Pong;
	xatomic32 Errors;
	xatomic32 Closed;
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		xatomic32 WriterText;
		xatomic32 WriterBinary;
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
		xatomic32 WriterCompressedText;
		xatomic32 WriterCompressedBinary;
	#endif
	xwsopcode Opcode;
	bool Compressed;
	size_t Size;
	uint8 Data[64];
	xwsconnclose Close;
} test_ws_endpoint;



struct test_ws_connection {
	xnetengine* Engine;
	xnetlistener* Listener;
	xatomic32 FailAlloc;
	xatomic64 FailThread;
	xatomic64 FailAttempts;
	xatomic64 AllocAttempts;
	xatomicptr Client;
	xatomicptr Server;
	xatomic32 ListenerClosed;
	xatomic32 ListenerErrors;
	xatomic32 PauseSeen;
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF)
		xatomic32 ClientRefRelease;
		xatomic32 ServerRefRelease;
		xatomic32 RefTaskDone;
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		xatomic32 WriterTaskDone;
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_REF)
		xatomic32 WriterRefRelease;
		xatomic32 WriterReentrantRelease;
	#endif
	test_ws_endpoint ClientEndpoint;
	test_ws_endpoint ServerEndpoint;
	xwsconnevents Events;
};



/* 正常阶段转发分配，只拒绝指定测试线程的请求。 */
static ptr testWsConnAlloc(ptr pData, size_t iSize)
{
	test_ws_connection* pTest =
		(test_ws_connection*)pData;

	(void)xrtAtomic64FetchAdd(
		&pTest->AllocAttempts,
		1,
		XMEMORY_RELAXED
	);
	if ( xrtAtomic32Load(
		&pTest->FailAlloc,
		XMEMORY_ACQUIRE
	) && (xrtAtomic64Load(
		&pTest->FailThread,
		XMEMORY_ACQUIRE
	) == xrtThreadCurrentId()) ) {
		(void)xrtAtomic64FetchAdd(
			&pTest->FailAttempts,
			1,
			XMEMORY_RELAXED
		);
		return NULL;
	}
	return malloc(iSize);
}



/* 故障线程拒绝重分配，其他线程保持 C 运行库语义。 */
static ptr testWsConnRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	test_ws_connection* pTest =
		(test_ws_connection*)pData;

	(void)xrtAtomic64FetchAdd(
		&pTest->AllocAttempts,
		1,
		XMEMORY_RELAXED
	);
	if ( xrtAtomic32Load(
		&pTest->FailAlloc,
		XMEMORY_ACQUIRE
	) && (xrtAtomic64Load(
		&pTest->FailThread,
		XMEMORY_ACQUIRE
	) == xrtThreadCurrentId()) ) {
		(void)xrtAtomic64FetchAdd(
			&pTest->FailAttempts,
			1,
			XMEMORY_RELAXED
		);
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放已经成功取得的底层内存。 */
static void testWsConnFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF)
/* 记录引用负载的唯一释放，并归还 XRT 分配。 */
static void testWsConnRefRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	xatomic32* pRelease = (xatomic32*)pContext;

	(void)iSize;
	(void)xrtAtomic32FetchAdd(
		pRelease,
		1,
		XMEMORY_RELEASE
	);
	xrtFree((ptr)pData);
}



/* 仅记录拒绝路径是否错误转移所有权，不释放借用的边界地址。 */
static void testWsConnRefCountRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	xatomic32* pRelease = (xatomic32*)pContext;

	(void)pData;
	(void)iSize;
	(void)xrtAtomic32FetchAdd(
		pRelease,
		1,
		XMEMORY_RELEASE
	);
}
#endif



#if defined(XWS_FEATURE_WEBSOCKET_WRITER_REF)
typedef struct test_ws_writer_release {
	xwswriter* Writer;
	xatomic32* Released;
} test_ws_writer_release;



/* 同步释放回调销毁当前 Writer，验证实现会把释放推迟到状态提交之后。 */
static void testWsConnWriterReentrantRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	test_ws_writer_release* pRelease =
		(test_ws_writer_release*)pContext;

	(void)iSize;
	xrtFree((ptr)pData);
	(void)xrtAtomic32FetchAdd(
		pRelease->Released,
		1,
		XMEMORY_RELEASE
	);
	xrtWsWriterDestroy(pRelease->Writer);
}
#endif



/* 在测试截止时间前等待原子值达到下限。 */
static void testWsConnWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(10000000)
	);

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) < iExpected ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 开始一条流式消息，并重置当前消息暂存。 */
static void testWsConnMessageBegin(
	xwsconn* pConnection,
	const xwsmessageinfo* pInfo,
	ptr pData
)
{
	test_ws_endpoint* pEndpoint =
		(test_ws_endpoint*)pData;

	testRequire(
		(pInfo != NULL) &&
		xrtNetWorkerIsCurrent(
			xrtWsConnWorker(pConnection)
		),
		"WebSocket message callback worker mismatch"
	);
	pEndpoint->Opcode = (xwsopcode)pInfo->Opcode;
	pEndpoint->Compressed =
		(pInfo->Flags & XWS_MESSAGE_COMPRESSED) != 0;
	pEndpoint->Size = 0;
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Begun,
		1,
		XMEMORY_RELEASE
	);
}



/* 复制同步借用的数据分块，验证网络分块不会改变消息边界。 */
static void testWsConnMessageData(
	xwsconn* pConnection,
	xbytesview Data,
	ptr pData
)
{
	test_ws_endpoint* pEndpoint =
		(test_ws_endpoint*)pData;

	(void)pConnection;
	testRequire(
		Data.Size <=
			(sizeof(pEndpoint->Data) -
			 pEndpoint->Size),
		"WebSocket test message overflow"
	);
	if ( Data.Size != 0 ) {
		memcpy(
			pEndpoint->Data + pEndpoint->Size,
			Data.Data,
			Data.Size
		);
	}
	pEndpoint->Size += Data.Size;
}



/* 完成消息；服务端用统一 API 回送 Binary。 */
static void testWsConnMessageEnd(
	xwsconn* pConnection,
	ptr pData
)
{
	test_ws_endpoint* pEndpoint =
		(test_ws_endpoint*)pData;

	testRequire(
		(pEndpoint->Opcode == XWS_OPCODE_TEXT) ||
		(pEndpoint->Opcode == XWS_OPCODE_BINARY),
		"WebSocket test message opcode mismatch"
	);
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		if ( (pEndpoint->Role == XWS_ROLE_SERVER) &&
			(pEndpoint->Opcode == XWS_OPCODE_TEXT) &&
			(pEndpoint->Size == 7) &&
			(memcmp(
				pEndpoint->Data,
				"wr-\xE2\x82\xAC!",
				7
			 ) == 0) ) {
			(void)xrtAtomic32FetchAdd(
				&pEndpoint->WriterText,
				1,
				XMEMORY_RELEASE
			);
		}
		if ( (pEndpoint->Role == XWS_ROLE_CLIENT) &&
			(pEndpoint->Opcode == XWS_OPCODE_BINARY) &&
			(pEndpoint->Size == 5) &&
			(pEndpoint->Data[0] == UINT8_C(0x10)) &&
			(pEndpoint->Data[1] == UINT8_C(0x20)) &&
			(pEndpoint->Data[2] == UINT8_C(0x30)) &&
			(pEndpoint->Data[3] == UINT8_C(0x40)) &&
			(pEndpoint->Data[4] == UINT8_C(0x50)) ) {
			(void)xrtAtomic32FetchAdd(
				&pEndpoint->WriterBinary,
				1,
				XMEMORY_RELEASE
			);
		}
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
		if ( pEndpoint->Compressed &&
			(pEndpoint->Role == XWS_ROLE_SERVER) &&
			(pEndpoint->Opcode == XWS_OPCODE_TEXT) &&
			(pEndpoint->Size == 17u) &&
			(memcmp(
				pEndpoint->Data,
				"compressed-writer",
				17u
			 ) == 0) ) {
			(void)xrtAtomic32FetchAdd(
				&pEndpoint->WriterCompressedText,
				1,
				XMEMORY_RELEASE
			);
		}
		if ( pEndpoint->Compressed &&
			(pEndpoint->Opcode == XWS_OPCODE_BINARY) &&
			(((pEndpoint->Role == XWS_ROLE_SERVER) &&
			  (pEndpoint->Size == 7u) &&
			  (memcmp(pEndpoint->Data, "recover", 7u) == 0)) ||
			 ((pEndpoint->Role == XWS_ROLE_CLIENT) &&
			  (pEndpoint->Size == 5u) &&
			  (memcmp(pEndpoint->Data, "zippy", 5u) == 0))) ) {
			(void)xrtAtomic32FetchAdd(
				&pEndpoint->WriterCompressedBinary,
				1,
				XMEMORY_RELEASE
			);
		}
	#endif
	if ( pEndpoint->Role == XWS_ROLE_SERVER ) {
		testRequire(
			xrtWsConnBinary(
				pConnection,
				(xbytesview) {
					pEndpoint->Data,
					pEndpoint->Size
				}
			) == XNET_RESULT_OK,
			"WebSocket server echo failed"
		);
	}
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Messages,
		1,
		XMEMORY_RELEASE
	);
	if ( (pEndpoint->Role == XWS_ROLE_SERVER) &&
		(pEndpoint->Opcode == XWS_OPCODE_TEXT) &&
		(pEndpoint->Size == 5) &&
		(memcmp(pEndpoint->Data, "pause", 5) == 0) ) {
		xrtWsConnPause(pConnection);
		xrtAtomic32Store(
			&pEndpoint->Test->PauseSeen,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 记录 Ping；服务端关闭自动响应时使用公开 API 原样回复 Pong。 */
static void testWsConnPing(
	xwsconn* pConnection,
	xbytesview Payload,
	ptr pData
)
{
	test_ws_endpoint* pEndpoint =
		(test_ws_endpoint*)pData;

	(void)pConnection;
	testRequire(
		(Payload.Size == 5) &&
		(memcmp(Payload.Data, "probe", 5) == 0),
		"WebSocket Ping payload mismatch"
	);
	if ( pEndpoint->Role == XWS_ROLE_SERVER ) {
		testRequire(
			xrtWsConnPong(
				pConnection,
				Payload
			) == XNET_RESULT_OK,
			"WebSocket manual Pong failed"
		);
	}
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Ping,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录自动 Pong 的原始借用负载。 */
static void testWsConnPong(
	xwsconn* pConnection,
	xbytesview Payload,
	ptr pData
)
{
	test_ws_endpoint* pEndpoint =
		(test_ws_endpoint*)pData;

	(void)pConnection;
	testRequire(
		(Payload.Size == 5) &&
		(memcmp(Payload.Data, "probe", 5) == 0),
		"WebSocket Pong payload mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Pong,
		1,
		XMEMORY_RELEASE
	);
}



/* 测试路径不允许发布结构化错误。 */
static void testWsConnError(
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_ws_endpoint* pEndpoint =
		(test_ws_endpoint*)pData;

	(void)pConnection;
	testRequire(
		pError != NULL,
		"WebSocket error callback omitted its error"
	);
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 保存 Close 快照；Reason 已经复制到 Connection 内部。 */
static void testWsConnCloseEvent(
	xwsconn* pConnection,
	const xwsconnclose* pClose,
	ptr pData
)
{
	test_ws_endpoint* pEndpoint =
		(test_ws_endpoint*)pData;

	testRequire(
		pClose != NULL,
		"WebSocket Close snapshot is null"
	);
	testRequire(
		pClose->Transport == XNET_RESULT_OK,
		"WebSocket Close transport result mismatch"
	);
	testRequire(
		(pClose->Flags & XWS_CONN_CLOSE_SENT) != 0,
		"WebSocket Close sent flag missing"
	);
	testRequire(
		(pClose->Flags & XWS_CONN_CLOSE_RECEIVED) != 0,
		"WebSocket Close received flag missing"
	);
	testRequire(
		(pClose->Flags & XWS_CONN_CLOSE_CLEAN) != 0,
		"WebSocket clean Close flag missing"
	);
	testRequire(
		pClose->LocalCode == TEST_WS_CONNECTION_CLOSE_CODE,
		"WebSocket local Close code mismatch"
	);
	testRequire(
		pClose->RemoteCode == TEST_WS_CONNECTION_CLOSE_CODE,
		"WebSocket remote Close code mismatch"
	);
	pEndpoint->Close = *pClose;
	testRequire(
		xrtWsConnState(pConnection) ==
			XWS_CONN_CLOSED,
		"WebSocket Close event state mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 初始化客户端或服务端的统一已建立会话。 */
static xwsconn* testWsConnAttach(
	xnetstream* pStream,
	test_ws_endpoint* pEndpoint
)
{
	uint8 ConfigStorage[sizeof(xwsconnconfig) + 2u];
	uint8 EventsStorage[sizeof(xwsconnevents) + 2u];
	xwsconnconfig Config;
	xwsconnevents Events;
	xwsconn* pConnection;
	char Protocol[] = "chat.v1";

	xrtWsConnConfigInit(&Config);
	Config.Role = pEndpoint->Role;
	Config.Protocol = (xstrview) {
		Protocol,
		sizeof(Protocol) - 1u
	};
	Config.MessageLimit = TEST_WS_CONNECTION_MESSAGE_LIMIT;
	Config.FrameLimit = TEST_WS_CONNECTION_MESSAGE_LIMIT;
	Config.SendLimit = TEST_WS_CONNECTION_SEND_LIMIT;
	Config.ControlReserve = 512;
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
		xrtWsDeflateInit(&Config.Deflate);
		Config.DeflateEnabled = true;
	#endif
	if ( pEndpoint->Role == XWS_ROLE_SERVER ) {
		Config.AutoPong = false;
	}
	Events = pEndpoint->Test->Events;
	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	memset(EventsStorage, 0xA5, sizeof(EventsStorage));
	memcpy(ConfigStorage + 1u, &Config, sizeof(Config));
	memcpy(EventsStorage + 1u, &Events, sizeof(Events));
	pConnection = xrtWsConnAttach(
		pStream,
		(const xwsconnconfig*)(const void*)(ConfigStorage + 1u),
		(const xwsconnevents*)(const void*)(EventsStorage + 1u),
		pEndpoint
	);
	if ( pConnection != NULL ) {
		testRequire(
			(xrtWsConnBinary(
				pConnection,
				(xbytesview) {
					(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
					4u
				}
			 ) == XNET_RESULT_ERROR) &&
			(xrtErrorCode(xrtGetError()) ==
			 XWS_CONN_ERROR_ARGUMENT) &&
			(xrtWsConnPing(
				pConnection,
				(xbytesview) {
					(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
					4u
				}
			 ) == XNET_RESULT_ERROR) &&
			(xrtErrorCode(xrtGetError()) ==
			 XWS_CONN_ERROR_ARGUMENT) &&
			(xrtWsConnState(pConnection) == XWS_CONN_OPEN),
			"WebSocket send accepted a wrapping payload range"
		);
		xrtClearError();
	}
	memset(Protocol, 'x', sizeof(Protocol) - 1u);
	memset(ConfigStorage + 1u, 0, sizeof(Config));
	memset(EventsStorage + 1u, 0, sizeof(Events));
	testRequire(
		(ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5) &&
		(EventsStorage[0] == 0xA5) &&
		(EventsStorage[sizeof(EventsStorage) - 1u] == 0xA5),
		"WebSocket attach wrote outside unaligned input storage"
	);
	return pConnection;
}



/* Connection 分配失败不得接管或改变开放的 TCP Stream。 */
static void testWsConnRejectAttachOom(
	test_ws_connection* pTest,
	xnetstream* pStream
)
{
	ptr Held[4096];
	xwsconnconfig Config;
	size_t iHeld = 0;
	uint64 iBefore;
	xwsconn* pConnection;

	xrtWsConnConfigInit(&Config);
	xrtClearError();
	xrtAtomic64Store(
		&pTest->FailThread,
		xrtThreadCurrentId(),
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pTest->FailAlloc,
		1,
		XMEMORY_RELEASE
	);
	while ( iHeld < (sizeof(Held) / sizeof(Held[0])) ) {
		Held[iHeld] = xrtMalloc(sizeof(xwsconn));
		if ( Held[iHeld] == NULL ) {
			break;
		}
		iHeld++;
	}
	testRequire(
		iHeld < (sizeof(Held) / sizeof(Held[0])),
		"WebSocket attach OOM fixture could not exhaust its size class"
	);
	iBefore = xrtAtomic64Load(
		&pTest->FailAttempts,
		XMEMORY_ACQUIRE
	);
	pConnection = xrtWsConnAttach(
		pStream,
		&Config,
		NULL,
		NULL
	);
	xrtAtomic32Store(
		&pTest->FailAlloc,
		0,
		XMEMORY_RELEASE
	);
	for ( size_t i = 0; i < iHeld; i++ ) {
		xrtFree(Held[i]);
	}
	testRequire(
		(pConnection == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtAtomic64Load(
			&pTest->FailAttempts,
			XMEMORY_ACQUIRE
		 ) > iBefore) &&
		(xrtNetStreamState(pStream) == XNET_STREAM_OPEN),
		"WebSocket attach OOM changed the TCP Stream"
	);
	xrtClearError();
}



/* 非法配置必须在接管 Stream 事件和引用之前失败。 */
static void testWsConnRejectConfigs(xnetstream* pStream)
{
	xwsconnconfig Config;
	xnetstreamevents Events;
	ptr pData;
	size_t iWriteLimit;

	testRequire(
		(xrtWsConnAttach(
			pStream,
			(const xwsconnconfig*)(uintptr_t)(UINTPTR_MAX - 1u),
			NULL,
			NULL
		 ) == NULL) &&
		(xrtErrorCode(xrtGetError()) == XWS_CONN_ERROR_ARGUMENT) &&
		(xrtNetStreamState(pStream) == XNET_STREAM_OPEN),
		"WebSocket accepted a wrapping connection config"
	);
	xrtWsConnConfigInit(&Config);
	testRequire(
		(xrtWsConnAttach(
			pStream,
			&Config,
			(const xwsconnevents*)(uintptr_t)(UINTPTR_MAX - 1u),
			NULL
		 ) == NULL) &&
		(xrtErrorCode(xrtGetError()) == XWS_CONN_ERROR_ARGUMENT) &&
		(xrtNetStreamState(pStream) == XNET_STREAM_OPEN),
		"WebSocket accepted a wrapping connection event table"
	);
	xrtWsConnConfigInit(&Config);
	Config.Role = (xwsrole)99;
	testRequire(
		(xrtWsConnAttach(pStream, &Config, NULL, NULL) == NULL) &&
		(xrtErrorCode(xrtGetError()) == XWS_CONN_ERROR_CONFIG),
		"WebSocket accepted an invalid role"
	);
	xrtWsConnConfigInit(&Config);
	Config.Protocol = (xstrview) { NULL, 1 };
	testRequire(
		(xrtWsConnAttach(pStream, &Config, NULL, NULL) == NULL) &&
		(xrtErrorCode(xrtGetError()) == XWS_CONN_ERROR_CONFIG),
		"WebSocket accepted an invalid protocol view"
	);
	xrtWsConnConfigInit(&Config);
	Config.Protocol = (xstrview) {
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u),
		4u
	};
	testRequire(
		(xrtWsConnAttach(pStream, &Config, NULL, NULL) == NULL) &&
		(xrtErrorCode(xrtGetError()) == XWS_CONN_ERROR_CONFIG) &&
		(xrtNetStreamState(pStream) == XNET_STREAM_OPEN),
		"WebSocket accepted a wrapping protocol view"
	);
	xrtWsConnConfigInit(&Config);
	Config.MessageLimit = 0;
	testRequire(
		(xrtWsConnAttach(pStream, &Config, NULL, NULL) == NULL) &&
		(xrtErrorCode(xrtGetError()) == XWS_CONN_ERROR_CONFIG),
		"WebSocket accepted a zero message limit"
	);
	xrtWsConnConfigInit(&Config);
	Config.FrameLimit = 0;
	testRequire(
		(xrtWsConnAttach(pStream, &Config, NULL, NULL) == NULL) &&
		(xrtErrorCode(xrtGetError()) == XWS_CONN_ERROR_CONFIG),
		"WebSocket accepted a zero frame limit"
	);
	xrtWsConnConfigInit(&Config);
	Config.FrameLimit = XWS_FRAME_PAYLOAD_MAX + UINT64_C(1);
	testRequire(
		(xrtWsConnAttach(pStream, &Config, NULL, NULL) == NULL) &&
		(xrtErrorCode(xrtGetError()) == XWS_CONN_ERROR_CONFIG),
		"WebSocket accepted an unrepresentable frame limit"
	);
	xrtWsConnConfigInit(&Config);
	Config.ControlReserve =
		((2u + XWS_CLOSE_PAYLOAD_MAX) * 3u) - 1u;
	testRequire(
		(xrtWsConnAttach(pStream, &Config, NULL, NULL) == NULL) &&
		(xrtErrorCode(xrtGetError()) == XWS_CONN_ERROR_CONFIG),
		"WebSocket accepted an undersized control reserve"
	);
	xrtWsConnConfigInit(&Config);
	Config.SendLimit = Config.ControlReserve - 1u;
	testRequire(
		(xrtWsConnAttach(pStream, &Config, NULL, NULL) == NULL) &&
		(xrtErrorCode(xrtGetError()) == XWS_CONN_ERROR_CONFIG) &&
		(xrtNetStreamState(pStream) == XNET_STREAM_OPEN),
		"WebSocket invalid config changed the TCP Stream"
	);
	xrtWsConnConfigInit(&Config);
	Config.SendLimit = Config.ControlReserve;
	testRequire(
		(xrtWsConnAttach(pStream, &Config, NULL, NULL) == NULL) &&
		(xrtErrorCode(xrtGetError()) == XWS_CONN_ERROR_CONFIG) &&
		(xrtNetStreamState(pStream) == XNET_STREAM_OPEN),
		"WebSocket accepted no data-frame capacity"
	);
	xrtWsConnConfigInit(&Config);
	Events = pStream->Events;
	pData = xrtAtomicPtrLoad(&pStream->Data, XMEMORY_ACQUIRE);
	iWriteLimit = pStream->Config.WriteLimit;
	pStream->Config.WriteLimit = Config.ControlReserve + 1u;
	testRequire(
		(xrtWsConnAttach(pStream, &Config, NULL, NULL) == NULL) &&
		(xrtErrorCode(xrtGetError()) == XWS_CONN_ERROR_CONFIG) &&
		(xrtNetStreamState(pStream) == XNET_STREAM_OPEN) &&
		(memcmp(&pStream->Events, &Events, sizeof(Events)) == 0) &&
		(xrtAtomicPtrLoad(
			&pStream->Data,
			XMEMORY_ACQUIRE
		 ) == pData),
		"WebSocket accepted a permanently unwritable TCP transport"
	);
	pStream->Config.WriteLimit = iWriteLimit;
	xrtClearError();
}



/* 接管服务端 Stream 引用。 */
static bool testWsConnAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_ws_connection* pTest =
		(test_ws_connection*)pData;
	xwsconn* pConnection;

	(void)pListener;
	pConnection = testWsConnAttach(
		pStream,
		&pTest->ServerEndpoint
	);
	testRequire(
		pConnection != NULL,
		"WebSocket server attach failed"
	);
	xrtAtomicPtrStore(
		&pTest->Server,
		pConnection,
		XMEMORY_RELEASE
	);
	return true;
}



/* 客户端 TCP Open 后把调用方 Stream 引用交给 WebSocket。 */
static void testWsConnClientOpen(
	xnetstream* pStream,
	ptr pData
)
{
	test_ws_connection* pTest =
		(test_ws_connection*)pData;
	xwsconn* pConnection;

	testWsConnRejectAttachOom(pTest, pStream);
	testWsConnRejectConfigs(pStream);
	pConnection = testWsConnAttach(
		pStream,
		&pTest->ClientEndpoint
	);
	testRequire(
		pConnection != NULL,
		"WebSocket client attach failed"
	);
	xrtAtomicPtrStore(
		&pTest->Client,
		pConnection,
		XMEMORY_RELEASE
	);
}



/* Listener 错误必须独立于 Connection 错误。 */
static void testWsConnListenerError(
	xnetlistener* pListener,
	const xerror* pError,
	ptr pData
)
{
	test_ws_connection* pTest =
		(test_ws_connection*)pData;

	(void)pListener;
	testRequire(
		pError != NULL,
		"WebSocket listener error omitted its cause"
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->ListenerErrors,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Listener 唯一关闭终态。 */
static void testWsConnListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_ws_connection* pTest =
		(test_ws_connection*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pTest->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 构建并发送一个掩码数据帧，用于覆盖分片线路输入。 */
static void testWsConnRawMasked(
	xnetstream* pStream,
	xwsopcode Opcode,
	bool bFinal,
	cbytes pData,
	size_t iSize
)
{
	static const uint8 Mask[XWS_MASK_SIZE] = {
		0x11, 0x22, 0x33, 0x44
	};
	xwsframe Frame;
	uint8 Output[64];
	size_t iHead = 0;

	xrtWsFrameInit(&Frame);
	Frame.Opcode = (uint8)Opcode;
	Frame.PayloadSize = iSize;
	Frame.Flags = XWS_FRAME_MASKED;
	if ( bFinal ) {
		Frame.Flags |= XWS_FRAME_FIN;
	}
	memcpy(Frame.Mask, Mask, sizeof(Mask));
	testRequire(
		xrtWsFrameWrite(
			&Frame,
			NULL,
			Output,
			sizeof(Output),
			&iHead
		) &&
		(iSize <= (sizeof(Output) - iHead)),
		"WebSocket raw frame header failed"
	);
	memcpy(Output + iHead, pData, iSize);
	testRequire(
		xrtWsMask(
			Output + iHead,
			iSize,
			Mask,
			0
		) &&
		(xrtNetStreamSend(
			pStream,
			Output,
			iHead + iSize
		 ) == XNET_RESULT_OK),
		"WebSocket raw masked frame send failed"
	);
}



/* 永远超出硬发送容量的帧必须在随机数、分配和复制之前失败。 */
static void testWsConnRejectCapacity(
	test_ws_connection* pTest,
	xwsconn* pConnection
)
{
	static const uint8 Payload[
		TEST_WS_CONNECTION_BACKPRESSURE_SIZE
	] = { 0 };
	uint64 iBefore = xrtAtomic64Load(
		&pTest->FailAttempts,
		XMEMORY_ACQUIRE
	);
	size_t iPending = xrtWsConnPending(pConnection);
	xnetresult Result;

	xrtAtomic64Store(
		&pTest->FailThread,
		xrtThreadCurrentId(),
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pTest->FailAlloc,
		1,
		XMEMORY_RELEASE
	);
	Result = xrtWsConnBinary(
		pConnection,
		(xbytesview) {
			Payload,
			sizeof(Payload)
		}
	);
	xrtAtomic32Store(
		&pTest->FailAlloc,
		0,
		XMEMORY_RELEASE
	);
	testRequire(
		(Result == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_LIMIT),
		"WebSocket permanent over-budget send was treated as retryable"
	);
	testRequire(
		xrtAtomic64Load(
			&pTest->FailAttempts,
			XMEMORY_ACQUIRE
		) == iBefore,
		"WebSocket over-budget send reached the allocator"
	);
	testRequire(
		(xrtWsConnState(pConnection) == XWS_CONN_OPEN) &&
		(xrtWsConnPending(pConnection) == iPending) &&
		(xrtWsConnError(pConnection) == NULL),
		"WebSocket over-budget send changed connection state"
	);
	xrtClearError();
}



/* 验证数据、手动控制、自动 Pong 和 Close 的三级硬预留。 */
static void testWsConnControlReserve(xwsconn* pConnection)
{
	xnetstream* pStream = xrtWsConnTcp(pConnection);
	size_t iControlSlot = 2u + XWS_CLOSE_PAYLOAD_MAX +
		(pConnection->Config.Role == XWS_ROLE_CLIENT ?
			XWS_MASK_SIZE : 0u);
	size_t iLimit = pConnection->Config.SendLimit;
	size_t iTransport;
	uint64 iOutput;
	size_t iWire = 0;

	testRequire(
		pStream != NULL,
		"WebSocket control reserve transport is unavailable"
	);
	if ( xrtNetStreamWriteLimit(pStream) < iLimit ) {
		iLimit = xrtNetStreamWriteLimit(pStream);
	}
	iTransport = xrtNetStreamPending(pStream);
	iOutput = xrtAtomic64Load(
		&pConnection->OutputBytes,
		XMEMORY_ACQUIRE
	);
	testRequire(
		(iOutput == 0) &&
		(iLimit > pConnection->Config.ControlReserve) &&
		(iTransport <= (iLimit - pConnection->Config.ControlReserve)),
		"WebSocket control reserve test entered with pending output"
	);

	xrtAtomic64Store(
		&pConnection->OutputBytes,
		(uint64)(iLimit - pConnection->Config.ControlReserve -
			iTransport),
		XMEMORY_RELEASE
	);
	testRequire(
		(__xrtWsConnFrameBudget(
			pConnection,
			XWS_CLOSE_PAYLOAD_MAX,
			__XRT_WS_SEND_DATA,
			&iWire
		 ) == XNET_RESULT_AGAIN) &&
		(__xrtWsConnFrameBudget(
			pConnection,
			XWS_CLOSE_PAYLOAD_MAX,
			__XRT_WS_SEND_CONTROL,
			&iWire
		 ) == XNET_RESULT_OK) &&
		(iWire == iControlSlot),
		"WebSocket manual control frame did not enter its reserve"
	);

	xrtAtomic64Store(
		&pConnection->OutputBytes,
		(uint64)(iLimit - (iControlSlot * 2u) - iTransport),
		XMEMORY_RELEASE
	);
	testRequire(
		(__xrtWsConnFrameBudget(
			pConnection,
			XWS_CLOSE_PAYLOAD_MAX,
			__XRT_WS_SEND_CONTROL,
			&iWire
		 ) == XNET_RESULT_AGAIN) &&
		(__xrtWsConnFrameBudget(
			pConnection,
			XWS_CLOSE_PAYLOAD_MAX,
			__XRT_WS_SEND_AUTO_PONG,
			&iWire
		 ) == XNET_RESULT_OK),
		"WebSocket automatic Pong did not retain its private slot"
	);

	xrtAtomic64Store(
		&pConnection->OutputBytes,
		(uint64)(iLimit - iControlSlot - iTransport),
		XMEMORY_RELEASE
	);
	testRequire(
		(__xrtWsConnFrameBudget(
			pConnection,
			XWS_CLOSE_PAYLOAD_MAX,
			__XRT_WS_SEND_AUTO_PONG,
			&iWire
		 ) == XNET_RESULT_AGAIN) &&
		(__xrtWsConnFrameBudget(
			pConnection,
			XWS_CLOSE_PAYLOAD_MAX,
			__XRT_WS_SEND_CLOSE,
			&iWire
		 ) == XNET_RESULT_OK),
		"WebSocket Close did not retain the final control slot"
	);
	xrtAtomic64Store(
		&pConnection->OutputBytes,
		iOutput,
		XMEMORY_RELEASE
	);
	pConnection->Backpressured = false;
	xrtClearError();
}



/* 单帧分配失败必须保留连接状态和全部发送预算。 */
static void testWsConnRejectSendOom(
	test_ws_connection* pTest,
	xwsconn* pConnection
)
{
	static const uint8 OomPayload[
		TEST_WS_CONNECTION_OOM_SIZE
	] = { 0 };
	uint64 iBefore = xrtAtomic64Load(
		&pTest->FailAttempts,
		XMEMORY_ACQUIRE
	);
	size_t iPending = xrtWsConnPending(pConnection);
	size_t iWritable = xrtWsConnWritable(pConnection);
	xnetresult Result;

	testRequire(
		xrtWsConnError(pConnection) == NULL,
		"WebSocket entered OOM test with a session error"
	);
	xrtAtomic64Store(
		&pTest->FailThread,
		xrtThreadCurrentId(),
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pTest->FailAlloc,
		1,
		XMEMORY_RELEASE
	);
	Result = xrtWsConnBinary(
		pConnection,
		(xbytesview) {
			OomPayload,
			sizeof(OomPayload)
		}
	);
	xrtAtomic32Store(
		&pTest->FailAlloc,
		0,
		XMEMORY_RELEASE
	);
	testRequire(
		Result == XNET_RESULT_ERROR,
		"WebSocket send OOM returned the wrong result"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"WebSocket send OOM omitted its thread error"
	);
	testRequire(
		xrtAtomic64Load(
			&pTest->FailAttempts,
			XMEMORY_ACQUIRE
		) > iBefore,
		"WebSocket send OOM did not reach the allocator"
	);
	testRequire(
		(xrtWsConnState(pConnection) == XWS_CONN_OPEN) &&
		(xrtWsConnPending(pConnection) == iPending) &&
		(xrtWsConnWritable(pConnection) == iWritable),
		"WebSocket send OOM changed connection state or budget"
	);
	testRequire(
		xrtWsConnError(pConnection) == NULL,
		"WebSocket send OOM occupied the session error slot"
	);
	xrtClearError();
}



/* 调用方错误必须可恢复，并且不得占用 Connection 的终态错误槽。 */
static void testWsConnRejectCall(
	xwsconn* pConnection,
	xnetresult Result,
	xwsconnerror Code,
	cstr sMessage
)
{
	testRequire(
		(Result == XNET_RESULT_ERROR) &&
		(xrtErrorCode(xrtGetError()) == (int32)Code) &&
		(xrtWsConnError(pConnection) == NULL),
		sMessage
	);
	xrtClearError();
}



/* 覆盖发送、UTF-8、控制帧和 Close 的可恢复调用边界。 */
static void testWsConnRejectOperations(xwsconn* pConnection)
{
	static const uint8 InvalidUtf8[] = {
		UINT8_C(0xC3), UINT8_C(0x28)
	};
	uint8 OversizedControl[126] = { 0 };

	testWsConnRejectCall(
		pConnection,
		xrtWsConnSend(
			pConnection,
			XWS_OPCODE_CLOSE,
			XRT_BYTES_LITERAL("")
		),
		XWS_CONN_ERROR_ARGUMENT,
		"WebSocket accepted a message control opcode"
	);
	testWsConnRejectCall(
		pConnection,
		xrtWsConnBinary(
			pConnection,
			(xbytesview) { NULL, 1 }
		),
		XWS_CONN_ERROR_ARGUMENT,
		"WebSocket accepted an invalid binary view"
	);
	testWsConnRejectCall(
		pConnection,
		xrtWsConnText(
			pConnection,
			(xstrview) {
				(const char*)InvalidUtf8,
				sizeof(InvalidUtf8)
			}
		),
		XWS_CONN_ERROR_MESSAGE,
		"WebSocket accepted invalid UTF-8 text"
	);
	testWsConnRejectCall(
		pConnection,
		xrtWsConnBinary(
			pConnection,
			(xbytesview) {
				(cbytes)"x",
				TEST_WS_CONNECTION_MESSAGE_LIMIT + 1u
			}
		),
		XWS_CONN_ERROR_LIMIT,
		"WebSocket accepted an oversized message"
	);
	testWsConnRejectCall(
		pConnection,
		xrtWsConnPing(
			pConnection,
			(xbytesview) {
				OversizedControl,
				sizeof(OversizedControl)
			}
		),
		XWS_CONN_ERROR_LIMIT,
		"WebSocket accepted an oversized Ping"
	);
	testWsConnRejectCall(
		pConnection,
		xrtWsConnPong(
			pConnection,
			(xbytesview) {
				OversizedControl,
				sizeof(OversizedControl)
			}
		),
		XWS_CONN_ERROR_LIMIT,
		"WebSocket accepted an oversized Pong"
	);
	testWsConnRejectCall(
		pConnection,
		xrtWsConnClose(
			pConnection,
			XWS_CLOSE_NO_STATUS,
			XRT_STR_LITERAL("")
		),
		XWS_CONN_ERROR_MESSAGE,
		"WebSocket accepted a reserved Close code"
	);
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF)
/* 客户端掩码路径必须复制后立即、且仅一次释放来源引用。 */
static void testWsConnClientRef(
	test_ws_connection* pTest,
	xwsconn* pConnection
)
{
	static const uint8 InvalidUtf8[] = {
		UINT8_C(0xC3), UINT8_C(0x28)
	};
	uint8 RefStorage[sizeof(xnetref) + 2u];
	xatomic32 BoundaryRelease;
	xatomic32 UnalignedRelease;
	xnetref Invalid;
	xnetref Ref;
	bytes pPayload;
	xnetresult Result;

	xrtAtomic32Init(&BoundaryRelease, 0);
	Invalid = (xnetref) {
		(cbytes)"x",
		1,
		testWsConnRefCountRelease,
		&BoundaryRelease
	};
	testRequire(
		(xrtWsConnBinaryRef(
			(xwsconn*)(uintptr_t)(UINTPTR_MAX - 1u),
			&Invalid
		 ) == XNET_RESULT_ERROR) &&
		(xrtWsConnBinaryRef(pConnection, NULL) ==
		 XNET_RESULT_ERROR) &&
		(xrtWsConnBinaryRef(
			pConnection,
			(const xnetref*)(uintptr_t)(UINTPTR_MAX - 1u)
		 ) == XNET_RESULT_ERROR) &&
		(xrtWsConnBinaryRef(
			pConnection,
			(const xnetref*)(const void*)pConnection
		 ) == XNET_RESULT_ERROR),
		"WebSocket reference accepted an invalid object range"
	);
	Invalid.Data = (cbytes)(uintptr_t)(UINTPTR_MAX - 1u);
	Invalid.Size = 4u;
	testRequire(
		xrtWsConnBinaryRef(pConnection, &Invalid) ==
			XNET_RESULT_ERROR,
		"WebSocket reference accepted a wrapping data range"
	);
	Invalid.Data = (cbytes)(const void*)pConnection;
	Invalid.Size = 1u;
	testRequire(
		(xrtWsConnBinaryRef(pConnection, &Invalid) ==
		 XNET_RESULT_ERROR) &&
		(xrtAtomic32Load(
			&BoundaryRelease,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		(xrtWsConnState(pConnection) == XWS_CONN_OPEN),
		"WebSocket reference accepted connection-backed ownership"
	);
	xrtClearError();

	xrtAtomic32Init(&UnalignedRelease, 0);
	pPayload = (bytes)xrtMalloc(9);
	testRequire(
		pPayload != NULL,
		"WebSocket unaligned reference allocation failed"
	);
	memcpy(pPayload, "unaligned", 9);
	Ref = (xnetref) {
		pPayload,
		9,
		testWsConnRefRelease,
		&UnalignedRelease
	};
	memset(RefStorage, 0xA5, sizeof(RefStorage));
	memcpy(RefStorage + 1u, &Ref, sizeof(Ref));
	Result = xrtWsConnBinaryRef(
		pConnection,
		(const xnetref*)(const void*)(RefStorage + 1u)
	);
	testRequire(
		(Result == XNET_RESULT_OK) &&
		(xrtAtomic32Load(
			&UnalignedRelease,
			XMEMORY_ACQUIRE
		 ) == 1) &&
		(RefStorage[0] == UINT8_C(0xA5)) &&
		(RefStorage[sizeof(RefStorage) - 1u] == UINT8_C(0xA5)),
		"WebSocket unaligned reference snapshot failed"
	);

	pPayload = (bytes)xrtMalloc(10);
	testRequire(
		pPayload != NULL,
		"WebSocket client reference allocation failed"
	);
	memcpy(pPayload, "client-ref", 10);
	Ref = (xnetref) {
		pPayload,
		10,
		testWsConnRefRelease,
		&pTest->ClientRefRelease
	};
	Result = xrtWsConnTextRef(pConnection, &Ref);
	testRequire(
		(Result == XNET_RESULT_OK) &&
		(xrtAtomic32Load(
			&pTest->ClientRefRelease,
			XMEMORY_ACQUIRE
		 ) == 1),
		"WebSocket client reference was not released after copy"
	);

	pPayload = (bytes)xrtMalloc(sizeof(InvalidUtf8));
	testRequire(
		pPayload != NULL,
		"WebSocket invalid UTF-8 reference allocation failed"
	);
	memcpy(pPayload, InvalidUtf8, sizeof(InvalidUtf8));
	Ref = (xnetref) {
		pPayload,
		sizeof(InvalidUtf8),
		testWsConnRefRelease,
		&pTest->ClientRefRelease
	};
	Result = xrtWsConnTextRef(pConnection, &Ref);
	testRequire(
		(Result == XNET_RESULT_ERROR) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_MESSAGE) &&
		(xrtAtomic32Load(
			&pTest->ClientRefRelease,
			XMEMORY_ACQUIRE
		 ) == 1),
		"WebSocket invalid UTF-8 reference transferred ownership"
	);
	xrtClearError();
	xrtFree(pPayload);

	pPayload = (bytes)xrtMalloc(1);
	testRequire(
		pPayload != NULL,
		"WebSocket empty reference allocation failed"
	);
	Ref = (xnetref) {
		pPayload,
		0,
		testWsConnRefRelease,
		&pTest->ClientRefRelease
	};
	Result = xrtWsConnBinaryRef(pConnection, &Ref);
	testRequire(
		(Result == XNET_RESULT_ERROR) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_ARGUMENT) &&
		(xrtAtomic32Load(
			&pTest->ClientRefRelease,
			XMEMORY_ACQUIRE
		 ) == 1),
		"WebSocket empty reference transferred ownership"
	);
	xrtClearError();
	xrtFree(pPayload);
}



/* 服务端明文路径覆盖零复制、Take、OOM、永久容量和瞬时背压。 */
static void testWsConnServerRefTask(
	xnetworker* pWorker,
	ptr pData
)
{
	test_ws_connection* pTest =
		(test_ws_connection*)pData;
	xwsconn* pConnection = (xwsconn*)xrtAtomicPtrLoad(
		&pTest->Server,
		XMEMORY_ACQUIRE
	);
	xatomic32 RejectedRelease;
	xnetref Ref;
	bytes pPayload;
	ptr* pExhausted;
	size_t iExhausted = 0;
	uint64 iAllocBefore;
	uint64 iOutputBefore;
	xnetresult Result;

	xrtAtomic32Init(&RejectedRelease, 0);

	pPayload = (bytes)xrtMalloc(8);
	testRequire(
		pPayload != NULL,
		"WebSocket reference OOM payload allocation failed"
	);
	memset(pPayload, 0x11, 8);
	Ref = (xnetref) {
		pPayload,
		8,
		testWsConnRefRelease,
		&RejectedRelease
	};
	pExhausted = (ptr*)malloc(
		TEST_WS_CONNECTION_OOM_BLOCKS *
			sizeof(*pExhausted)
	);
	testRequire(
		pExhausted != NULL,
		"WebSocket reference OOM fixture allocation failed"
	);
	/* 先持有这一尺寸类的全部缓存节点，确保随后覆盖真实冷分配失败。 */
	xrtAtomic64Store(
		&pTest->FailThread,
		xrtThreadCurrentId(),
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pTest->FailAlloc,
		1,
		XMEMORY_RELEASE
	);
	while ( iExhausted <
		TEST_WS_CONNECTION_OOM_BLOCKS ) {
		ptr pBlock = xrtNetWorkerAlloc(
			pWorker,
			XWS_FRAME_HEAD_MAX + sizeof(ptr)
		);

		if ( pBlock == NULL ) {
			break;
		}
		pExhausted[iExhausted++] = pBlock;
	}
	testRequire(
		iExhausted < TEST_WS_CONNECTION_OOM_BLOCKS,
		"WebSocket reference OOM fixture could not exhaust the header class"
	);
	xrtClearError();
	Result = xrtWsConnBinaryRef(pConnection, &Ref);
	xrtAtomic32Store(
		&pTest->FailAlloc,
		0,
		XMEMORY_RELEASE
	);
	for ( size_t i = 0; i < iExhausted; i++ ) {
		xrtNetWorkerFree(
			pWorker,
			pExhausted[i],
			XWS_FRAME_HEAD_MAX + sizeof(ptr)
		);
	}
	free(pExhausted);
	testRequire(
		Result == XNET_RESULT_ERROR,
		"WebSocket reference header OOM returned the wrong result"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"WebSocket reference header OOM lost its memory error"
	);
	testRequire(
		xrtAtomic32Load(
			&RejectedRelease,
			XMEMORY_ACQUIRE
		) == 0,
		"WebSocket reference header OOM transferred ownership"
	);
	xrtClearError();
	xrtFree(pPayload);

	pPayload = (bytes)xrtMalloc(1);
	testRequire(
		pPayload != NULL,
		"WebSocket oversized reference allocation failed"
	);
	Ref = (xnetref) {
		pPayload,
		TEST_WS_CONNECTION_MESSAGE_LIMIT + 1u,
		testWsConnRefRelease,
		&RejectedRelease
	};
	Result = xrtWsConnBinaryRef(pConnection, &Ref);
	testRequire(
		(Result == XNET_RESULT_ERROR) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_LIMIT) &&
		(xrtAtomic32Load(
			&RejectedRelease,
			XMEMORY_ACQUIRE
		 ) == 0),
		"WebSocket oversized reference transferred ownership"
	);
	xrtClearError();
	xrtFree(pPayload);

	pPayload = (bytes)xrtMalloc(6);
	testRequire(
		pPayload != NULL,
		"WebSocket server reference allocation failed"
	);
	memcpy(pPayload, "server", 6);
	Ref = (xnetref) {
		pPayload,
		6,
		testWsConnRefRelease,
		&pTest->ServerRefRelease
	};
	testRequire(
		xrtWsConnBinaryRef(
			pConnection,
			&Ref
		) == XNET_RESULT_OK,
		"WebSocket server reference send failed"
	);

	pPayload = (bytes)xrtMalloc(5);
	testRequire(
		pPayload != NULL,
		"WebSocket server Take allocation failed"
	);
	memcpy(pPayload, "taken", 5);
	testRequire(
		xrtWsConnTextTake(
			pConnection,
			(str)pPayload,
			5
		) == XNET_RESULT_OK,
		"WebSocket server Take send failed"
	);

	pPayload = (bytes)xrtMalloc(4096);
	testRequire(
		pPayload != NULL,
		"WebSocket retained reference allocation failed"
	);
	memset(pPayload, 0x22, 4096);
	Ref = (xnetref) {
		pPayload,
		4096,
		testWsConnRefRelease,
		&RejectedRelease
	};
	iAllocBefore = xrtAtomic64Load(
		&pTest->AllocAttempts,
		XMEMORY_ACQUIRE
	);
	iOutputBefore = xrtAtomic64Load(
		&pConnection->OutputBytes,
		XMEMORY_ACQUIRE
	);
	xrtAtomic64Store(
		&pConnection->OutputBytes,
		pConnection->Config.SendLimit,
		XMEMORY_RELEASE
	);
	Result = xrtWsConnBinaryRef(pConnection, &Ref);
	xrtAtomic64Store(
		&pConnection->OutputBytes,
		iOutputBefore,
		XMEMORY_RELEASE
	);
	testRequire(
		Result == XNET_RESULT_AGAIN,
		"WebSocket transient reference pressure returned the wrong result"
	);
	testRequire(
		xrtAtomic32Load(
			&RejectedRelease,
			XMEMORY_ACQUIRE
		) == 0,
		"WebSocket transient reference pressure transferred ownership"
	);
	testRequire(
		xrtAtomic64Load(
			&pTest->AllocAttempts,
			XMEMORY_ACQUIRE
		) == iAllocBefore,
		"WebSocket transient reference pressure reached the allocator"
	);
	xrtFree(pPayload);

	(void)xrtAtomic32FetchAdd(
		&pTest->RefTaskDone,
		1,
		XMEMORY_RELEASE
	);
}
#endif



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/* 窄预算仍应允许实际线路长度小于最大帧头的压缩短消息。 */
static void testWsConnCompressedNarrowBudget(
	xwsconn* pConnection
)
{
	const size_t iAvailableTarget = XWS_FRAME_HEAD_MAX;
	uint64 iOutput = xrtAtomic64Load(
		&pConnection->OutputBytes,
		XMEMORY_ACQUIRE
	);
	size_t iWritable = xrtWsConnWritable(pConnection);

	testRequire(
		(xrtWsConnRole(pConnection) == XWS_ROLE_CLIENT) &&
		(iWritable > XWS_FRAME_HEAD_MAX),
		"WebSocket compressed narrow-budget fixture is invalid"
	);
	xrtAtomic64Store(
		&pConnection->OutputBytes,
		iOutput + (iWritable - iAvailableTarget),
		XMEMORY_RELEASE
	);
	testRequire(
		xrtWsConnTextCompressed(
			pConnection,
			XRT_STR_LITERAL("")
		) == XNET_RESULT_OK,
		"WebSocket compressed short message ignored exact writable budget"
	);
	xrtAtomic64Store(
		&pConnection->OutputBytes,
		iOutput,
		XMEMORY_RELEASE
	);
}
#endif



#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
/* 客户端 Writer 覆盖背压回滚、分块 UTF-8、排他和控制帧穿插。 */
static void testWsConnClientWriter(
	test_ws_connection* pTest,
	xwsconn* pConnection
)
{
	static const uint8 TextA[] = {
		'w', 'r', '-', UINT8_C(0xE2)
	};
	static const uint8 TextB[] = {
		UINT8_C(0x82)
	};
	static const uint8 TextC[] = {
		UINT8_C(0xAC), '!'
	};
	static const uint8 Incomplete[] = {
		UINT8_C(0xE2)
	};
	static const uint8 Oversized[] = {
		UINT8_C(0x7F)
	};
	static const uint8 OomPayload[
		TEST_WS_CONNECTION_OOM_SIZE
	] = { 0 };
	xwswriter* pWriter;
	uint64 iOutput;
	uint64 iFailBefore;

	/* Writer 状态复用 Connection 内存，开始消息不能触发底层分配。 */
	iFailBefore = xrtAtomic64Load(
		&pTest->FailAttempts,
		XMEMORY_ACQUIRE
	);
	xrtAtomic64Store(
		&pTest->FailThread,
		xrtThreadCurrentId(),
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pTest->FailAlloc,
		1,
		XMEMORY_RELEASE
	);
	xrtClearError();
	pWriter = xrtWsConnBeginText(pConnection);
	xrtAtomic32Store(
		&pTest->FailAlloc,
		0,
		XMEMORY_RELEASE
	);
	testRequire(
		(pWriter != NULL) &&
		(xrtGetError() == NULL) &&
		(xrtAtomic64Load(
			&pTest->FailAttempts,
			XMEMORY_ACQUIRE
		 ) == iFailBefore),
		"WebSocket Writer begin unexpectedly allocated memory"
	);
	xrtClearError();
	testRequire(
		(xrtWsWriterWrite(
			pWriter,
			(xbytesview) {
				(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
				4u
			}
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		!xrtWsWriterIsFinished(pWriter),
		"WebSocket Writer accepted a wrapping data range"
	);
	xrtClearError();
	testRequire(
		(xrtWsWriterWrite(
			pWriter,
			(xbytesview) {
				(cbytes)(const void*)pWriter,
				1u
			}
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		!xrtWsWriterIsFinished(pWriter),
		"WebSocket Writer accepted data overlapping its state"
	);
	xrtClearError();
	{
		xwswriter* pWrapping =
			(xwswriter*)(uintptr_t)(UINTPTR_MAX - 1u);

		testRequire(
			(xrtWsWriterWrite(
				pWrapping,
				XRT_BYTES_LITERAL("x")
			 ) == XNET_RESULT_ERROR) &&
			(xrtWsWriterFinish(
				pWrapping,
				XRT_BYTES_LITERAL("x")
			 ) == XNET_RESULT_ERROR) &&
			!xrtWsWriterIsFinished(pWrapping),
			"WebSocket Writer accepted a wrapping object range"
		);
		xrtWsWriterDestroy(pWrapping);
		testRequire(
			xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
			"WebSocket Writer wrapping range error mismatch"
		);
	}
	xrtClearError();
	testRequire(
		(xrtWsConnBeginBinary(pConnection) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_AGAIN),
		"WebSocket accepted a second active Writer"
	);
	xrtClearError();
	testRequire(
		xrtWsConnBinary(
			pConnection,
			XRT_BYTES_LITERAL("blocked")
		) == XNET_RESULT_AGAIN,
		"WebSocket Writer did not exclude a complete data message"
	);

	/* 瞬时预算不足不能推进首帧和 UTF-8 前缀状态。 */
	iOutput = xrtAtomic64Load(
		&pConnection->OutputBytes,
		XMEMORY_ACQUIRE
	);
	xrtAtomic64Store(
		&pConnection->OutputBytes,
		pConnection->Config.SendLimit,
		XMEMORY_RELEASE
	);
	testRequire(
		xrtWsWriterWrite(
			pWriter,
			(xbytesview) {
				TextA,
				sizeof(TextA)
			}
		) == XNET_RESULT_AGAIN,
		"WebSocket Writer pressure returned the wrong result"
	);
	xrtAtomic64Store(
		&pConnection->OutputBytes,
		iOutput,
		XMEMORY_RELEASE
	);
	testRequire(
		xrtWsWriterWrite(
			pWriter,
			(xbytesview) {
				TextA,
				sizeof(TextA)
			}
		) == XNET_RESULT_OK,
		"WebSocket Writer could not retry the pressured fragment"
	);
	testRequire(
		(xrtWsConnPing(
			pConnection,
			XRT_BYTES_LITERAL("probe")
		 ) == XNET_RESULT_OK) &&
		(xrtWsWriterWrite(
			pWriter,
			(xbytesview) {
				TextB,
				sizeof(TextB)
			}
		 ) == XNET_RESULT_OK),
		"WebSocket control frame did not interleave with Writer"
	);
	testRequire(
		(xrtWsWriterFinish(
			pWriter,
			(xbytesview) {
				TextC,
				sizeof(TextC)
			}
		 ) == XNET_RESULT_OK) &&
		xrtWsWriterIsFinished(pWriter),
		"WebSocket Writer split UTF-8 finish failed"
	);
	testRequire(
		xrtWsWriterFinish(
			pWriter,
			XRT_BYTES_LITERAL("")
		) == XNET_RESULT_ERROR,
		"WebSocket finished Writer accepted another fragment"
	);
	xrtClearError();
	xrtWsWriterDestroy(pWriter);

	/* 永久容量与发送 OOM 都不能提交首帧，随后仍可结束空消息。 */
	pWriter = xrtWsConnBeginBinary(pConnection);
	testRequire(
		pWriter != NULL,
		"WebSocket capacity rollback Writer begin failed"
	);
	testRequire(
		(xrtWsWriterWrite(
			pWriter,
			(xbytesview) {
				Oversized,
				TEST_WS_CONNECTION_SEND_LIMIT
			}
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		!xrtWsWriterIsFinished(pWriter),
		"WebSocket Writer committed a permanently oversized frame"
	);
	xrtClearError();
	iFailBefore = xrtAtomic64Load(
		&pTest->FailAttempts,
		XMEMORY_ACQUIRE
	);
	xrtAtomic32Store(
		&pTest->FailAlloc,
		1,
		XMEMORY_RELEASE
	);
	testRequire(
		(xrtWsWriterWrite(
			pWriter,
			(xbytesview) {
				OomPayload,
				sizeof(OomPayload)
			}
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtAtomic64Load(
			&pTest->FailAttempts,
			XMEMORY_ACQUIRE
		 ) > iFailBefore) &&
		!xrtWsWriterIsFinished(pWriter),
		"WebSocket Writer send OOM advanced its state"
	);
	xrtAtomic32Store(
		&pTest->FailAlloc,
		0,
		XMEMORY_RELEASE
	);
	xrtClearError();
	testRequire(
		xrtWsWriterFinish(
			pWriter,
			(xbytesview) { NULL, 0 }
		) == XNET_RESULT_OK,
		"WebSocket Writer did not recover after send OOM"
	);
	xrtWsWriterDestroy(pWriter);

	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
		/* 压缩 Writer 的 AGAIN 与 OOM 都必须发生在压缩状态推进之前。 */
		testRequire(
			(xrtWsConnBeginCompressed(
				pConnection,
				XWS_OPCODE_CLOSE
			 ) == NULL) &&
			(xrtErrorCode(xrtGetError()) ==
			 XWS_CONN_ERROR_ARGUMENT) &&
			(xrtWsConnState(pConnection) == XWS_CONN_OPEN),
			"WebSocket compressed Writer accepted a control opcode"
		);
		xrtClearError();
		pWriter = xrtWsConnBeginTextCompressed(pConnection);
		testRequire(
			pWriter != NULL,
			"WebSocket compressed Text Writer begin failed"
		);
		iOutput = xrtAtomic64Load(
			&pConnection->OutputBytes,
			XMEMORY_ACQUIRE
		);
		xrtAtomic64Store(
			&pConnection->OutputBytes,
			pConnection->Config.SendLimit,
			XMEMORY_RELEASE
		);
		testRequire(
			xrtWsWriterWrite(
				pWriter,
				XRT_BYTES_LITERAL("compressed-")
			) == XNET_RESULT_AGAIN,
			"WebSocket compressed Writer pressure mismatch"
		);
		xrtAtomic64Store(
			&pConnection->OutputBytes,
			iOutput,
			XMEMORY_RELEASE
		);
		testRequire(
			(xrtWsWriterWrite(
				pWriter,
				XRT_BYTES_LITERAL("compressed-")
			 ) == XNET_RESULT_OK) &&
			(xrtWsWriterFinish(
				pWriter,
				XRT_BYTES_LITERAL("writer")
			 ) == XNET_RESULT_OK) &&
			xrtWsWriterIsFinished(pWriter),
			"WebSocket compressed Text Writer retry failed"
		);
		xrtWsWriterDestroy(pWriter);

		pWriter = xrtWsConnBeginBinaryCompressed(pConnection);
		testRequire(
			pWriter != NULL,
			"WebSocket compressed Binary Writer begin failed"
		);
		iFailBefore = xrtAtomic64Load(
			&pTest->FailAttempts,
			XMEMORY_ACQUIRE
		);
		xrtAtomic32Store(
			&pTest->FailAlloc,
			1,
			XMEMORY_RELEASE
		);
		testRequire(
			(xrtWsWriterWrite(
				pWriter,
				(xbytesview) {
					OomPayload,
					sizeof(OomPayload)
				}
			 ) == XNET_RESULT_ERROR) &&
			(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
			(xrtAtomic64Load(
				&pTest->FailAttempts,
				XMEMORY_ACQUIRE
			 ) > iFailBefore) &&
			!xrtWsWriterIsFinished(pWriter),
			"WebSocket compressed Writer OOM advanced its state"
		);
		xrtAtomic32Store(
			&pTest->FailAlloc,
			0,
			XMEMORY_RELEASE
		);
		xrtClearError();
		testRequire(
			(xrtWsWriterFinish(
				pWriter,
				XRT_BYTES_LITERAL("recover")
			 ) == XNET_RESULT_OK) &&
			xrtWsWriterIsFinished(pWriter),
			"WebSocket compressed Writer did not recover after OOM"
		);
		xrtWsWriterDestroy(pWriter);
	#endif

	/* 无效最终 UTF-8 不能污染下一次有效重试。 */
	pWriter = xrtWsConnBeginText(pConnection);
	testRequire(
		pWriter != NULL,
		"WebSocket UTF-8 rollback Writer begin failed"
	);
	testRequire(
		(xrtWsWriterFinish(
			pWriter,
			(xbytesview) {
				Incomplete,
				sizeof(Incomplete)
			}
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_MESSAGE) &&
		!xrtWsWriterIsFinished(pWriter),
		"WebSocket Writer committed invalid final UTF-8"
	);
	xrtClearError();
	testRequire(
		xrtWsWriterFinish(
			pWriter,
			XRT_BYTES_LITERAL("valid")
		) == XNET_RESULT_OK,
		"WebSocket Writer did not recover after invalid UTF-8"
	);
	xrtWsWriterDestroy(pWriter);

	/* 累计消息上限失败同样不能推进 Writer。 */
	pWriter = xrtWsConnBeginBinary(pConnection);
	testRequire(
		pWriter != NULL,
		"WebSocket limit rollback Writer begin failed"
	);
	testRequire(
		(xrtWsWriterWrite(
			pWriter,
			(xbytesview) {
				Oversized,
				TEST_WS_CONNECTION_MESSAGE_LIMIT + 1u
			}
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_LIMIT) &&
		!xrtWsWriterIsFinished(pWriter),
		"WebSocket Writer committed an oversized fragment"
	);
	xrtClearError();
	testRequire(
		xrtWsWriterFinish(
			pWriter,
			XRT_BYTES_LITERAL("")
		) == XNET_RESULT_OK,
		"WebSocket Writer did not recover after its message limit"
	);
	xrtWsWriterDestroy(pWriter);

	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_REF)
		/* 客户端掩码路径会同步复制并释放来源，覆盖释放回调重入销毁。 */
		pWriter = xrtWsConnBeginBinary(pConnection);
		testRequire(
			pWriter != NULL,
			"WebSocket reentrant Writer begin failed"
		);
		{
			bytes pPayload = (bytes)xrtMalloc(9);
			uint8 RefStorage[sizeof(xnetref) + 2u];
			test_ws_writer_release Release;
			xnetref Invalid;
			xnetref Ref;
			xnetresult Result;

			testRequire(
				pPayload != NULL,
				"WebSocket reentrant Writer allocation failed"
			);
			memcpy(pPayload, "reentrant", 9);
			Release.Writer = pWriter;
			Release.Released =
				&pTest->WriterReentrantRelease;
			Invalid = (xnetref) {
				(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
				4u,
				testWsConnWriterReentrantRelease,
				&Release
			};
			testRequire(
				(xrtWsWriterWriteRef(
					pWriter,
					(const xnetref*)(uintptr_t)(UINTPTR_MAX - 1u)
				 ) == XNET_RESULT_ERROR) &&
				(xrtWsWriterWriteRef(
					pWriter,
					(const xnetref*)(const void*)pWriter
				 ) == XNET_RESULT_ERROR) &&
				(xrtWsWriterWriteRef(
					pWriter,
					&Invalid
				 ) == XNET_RESULT_ERROR) &&
				(xrtAtomic32Load(
					&pTest->WriterReentrantRelease,
					XMEMORY_ACQUIRE
				 ) == 0) &&
				!xrtWsWriterIsFinished(pWriter),
				"WebSocket Writer accepted invalid Ref ranges"
			);
			xrtClearError();
			Ref = (xnetref) {
				pPayload,
				9,
				testWsConnWriterReentrantRelease,
				&Release
			};
			memset(RefStorage, 0xA5, sizeof(RefStorage));
			memcpy(RefStorage + 1u, &Ref, sizeof(Ref));
			Result = xrtWsWriterFinishRef(
				pWriter,
				(const xnetref*)(const void*)(RefStorage + 1u)
			);
			if ( Result != XNET_RESULT_OK ) {
				xrtFree(pPayload);
				xrtWsWriterDestroy(pWriter);
			}
			testRequire(
				(Result == XNET_RESULT_OK) &&
				(RefStorage[0] == UINT8_C(0xA5)) &&
				(RefStorage[sizeof(RefStorage) - 1u] == UINT8_C(0xA5)) &&
				(xrtAtomic32Load(
					&pTest->WriterReentrantRelease,
					XMEMORY_ACQUIRE
				 ) == 1),
				"WebSocket Writer reentrant release failed"
			);
		}
	#endif
}



/* 服务端 Writer 覆盖复制分片，以及可选的 Ref 和 Take 所有权分片。 */
static void testWsConnServerWriterTask(
	xnetworker* pWorker,
	ptr pData
)
{
	static const uint8 DataA[] = {
		UINT8_C(0x10), UINT8_C(0x20)
	};
	static const uint8 DataB[] = {
		UINT8_C(0x30), UINT8_C(0x40), UINT8_C(0x50)
	};
	test_ws_connection* pTest =
		(test_ws_connection*)pData;
	xwsconn* pConnection = (xwsconn*)xrtAtomicPtrLoad(
		&pTest->Server,
		XMEMORY_ACQUIRE
	);
	xwswriter* pWriter = xrtWsConnBeginBinary(pConnection);

	(void)pWorker;
	testRequire(
		pWriter != NULL,
		"WebSocket server Writer begin failed"
	);
	testRequire(
		xrtWsConnText(
			pConnection,
			XRT_STR_LITERAL("blocked")
		) == XNET_RESULT_AGAIN,
		"WebSocket server Writer did not exclude complete data"
	);
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_REF)
		{
			bytes pDataA = (bytes)xrtMalloc(sizeof(DataA));
			bytes pDataB = (bytes)xrtMalloc(sizeof(DataB));
			xnetref RefA;
			xnetref RefB;
			xnetresult WriteResult;
			xnetresult FinishResult;

			testRequire(
				(pDataA != NULL) && (pDataB != NULL),
				"WebSocket Writer Ref allocation failed"
			);
			memcpy(pDataA, DataA, sizeof(DataA));
			memcpy(pDataB, DataB, sizeof(DataB));
			RefA = (xnetref) {
				pDataA,
				sizeof(DataA),
				testWsConnRefRelease,
				&pTest->WriterRefRelease
			};
			RefB = (xnetref) {
				pDataB,
				sizeof(DataB),
				testWsConnRefRelease,
				&pTest->WriterRefRelease
			};
			WriteResult = xrtWsWriterWriteRef(
				pWriter,
				&RefA
			);
			FinishResult = xrtWsWriterFinishRef(
				pWriter,
				&RefB
			);
			if ( WriteResult != XNET_RESULT_OK ) {
				xrtFree(pDataA);
			}
			if ( FinishResult != XNET_RESULT_OK ) {
				xrtFree(pDataB);
			}
			testRequire(
				(WriteResult == XNET_RESULT_OK) &&
				(FinishResult == XNET_RESULT_OK),
				"WebSocket server Writer Ref send failed"
			);
		}
	#else
		testRequire(
			(xrtWsWriterWrite(
				pWriter,
				(xbytesview) {
					DataA,
					sizeof(DataA)
				}
			 ) == XNET_RESULT_OK) &&
			(xrtWsWriterFinish(
				pWriter,
				(xbytesview) {
					DataB,
					sizeof(DataB)
				}
			 ) == XNET_RESULT_OK),
			"WebSocket server Writer copy send failed"
		);
	#endif
	testRequire(
		xrtWsWriterIsFinished(pWriter),
		"WebSocket server Writer did not finish"
	);
	xrtWsWriterDestroy(pWriter);

	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_REF)
		pWriter = xrtWsConnBeginBinary(pConnection);
		testRequire(
			pWriter != NULL,
			"WebSocket server Writer Take begin failed"
		);
		{
			bytes pDataA = (bytes)xrtMalloc(sizeof(DataA));
			bytes pDataB = (bytes)xrtMalloc(sizeof(DataB));
			xnetresult WriteResult;
			xnetresult FinishResult;

			testRequire(
				(pDataA != NULL) && (pDataB != NULL),
				"WebSocket Writer Take allocation failed"
			);
			memcpy(pDataA, DataA, sizeof(DataA));
			memcpy(pDataB, DataB, sizeof(DataB));
			WriteResult = xrtWsWriterWriteTake(
				pWriter,
				pDataA,
				sizeof(DataA)
			);
			FinishResult = xrtWsWriterFinishTake(
				pWriter,
				pDataB,
				sizeof(DataB)
			);
			if ( WriteResult != XNET_RESULT_OK ) {
				xrtFree(pDataA);
			}
			if ( FinishResult != XNET_RESULT_OK ) {
				xrtFree(pDataB);
			}
			testRequire(
				(WriteResult == XNET_RESULT_OK) &&
				(FinishResult == XNET_RESULT_OK),
				"WebSocket server Writer Take send failed"
			);
		}
		xrtWsWriterDestroy(pWriter);
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
		pWriter = xrtWsConnBeginBinaryCompressed(pConnection);
		testRequire(
			(pWriter != NULL) &&
			(xrtWsWriterWrite(
				pWriter,
				XRT_BYTES_LITERAL("zip")
			 ) == XNET_RESULT_OK) &&
			(xrtWsWriterFinish(
				pWriter,
				XRT_BYTES_LITERAL("py")
			 ) == XNET_RESULT_OK) &&
			xrtWsWriterIsFinished(pWriter),
			"WebSocket server compressed Writer failed"
		);
		xrtWsWriterDestroy(pWriter);
	#endif

	(void)xrtAtomic32FetchAdd(
		&pTest->WriterTaskDone,
		1,
		XMEMORY_RELEASE
	);
}
#endif



/* 在客户端 Worker 上发送普通、分片和 Ping 三条独立线路序列。 */
static void testWsConnSendTask(
	xnetworker* pWorker,
	ptr pData
)
{
	test_ws_connection* pTest =
		(test_ws_connection*)pData;
	xwsconn* pConnection = (xwsconn*)xrtAtomicPtrLoad(
		&pTest->Client,
		XMEMORY_ACQUIRE
	);
	xnetstream* pStream;

	(void)pWorker;
	testWsConnControlReserve(pConnection);
	testWsConnRejectCapacity(pTest, pConnection);
	testWsConnRejectSendOom(pTest, pConnection);
	testWsConnRejectOperations(pConnection);
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF)
		testWsConnClientRef(pTest, pConnection);
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		testWsConnCompressedNarrowBudget(pConnection);
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		testWsConnClientWriter(pTest, pConnection);
	#endif
	testRequire(
		(pConnection != NULL) &&
		(xrtWsConnText(
			pConnection,
			XRT_STR_LITERAL("hello")
		 ) == XNET_RESULT_OK) &&
		(xrtWsConnPing(
			pConnection,
			XRT_BYTES_LITERAL("probe")
		 ) == XNET_RESULT_OK),
		"WebSocket client message or Ping send failed"
	);
	pStream = xrtWsConnTcp(pConnection);
	testRequire(
		pStream != NULL,
		"WebSocket TCP transport query failed"
	);
	testWsConnRawMasked(
		pStream,
		XWS_OPCODE_TEXT,
		false,
		(cbytes)"frag",
		4
	);
	testWsConnRawMasked(
		pStream,
		XWS_OPCODE_CONTINUATION,
		true,
		(cbytes)"ment",
		4
	);
}



/* 连续发送两条消息，验证服务端暂停后会把后一条留在接收缓冲中。 */
static void testWsConnPauseTask(
	xnetworker* pWorker,
	ptr pData
)
{
	test_ws_connection* pTest =
		(test_ws_connection*)pData;
	xwsconn* pConnection = (xwsconn*)xrtAtomicPtrLoad(
		&pTest->Client,
		XMEMORY_ACQUIRE
	);

	(void)pWorker;
	testRequire(
		(pConnection != NULL) &&
		(xrtWsConnText(
			pConnection,
			XRT_STR_LITERAL("pause")
		 ) == XNET_RESULT_OK) &&
		(xrtWsConnText(
			pConnection,
			XRT_STR_LITERAL("after")
		 ) == XNET_RESULT_OK),
		"WebSocket pause sequence send failed"
	);
}



/* 在客户端 Worker 上发起标准 1000 Close。 */
static void testWsConnCloseTask(
	xnetworker* pWorker,
	ptr pData
)
{
	test_ws_connection* pTest =
		(test_ws_connection*)pData;
	xwsconn* pConnection = (xwsconn*)xrtAtomicPtrLoad(
		&pTest->Client,
		XMEMORY_ACQUIRE
	);
	(void)pWorker;
	#if defined(TEST_WS_CONNECTION_ABANDON_WRITER)
		{
			xwswriter* pWriter =
				xrtWsConnBeginBinary(pConnection);

			testRequire(
				(pWriter != NULL) &&
				(xrtWsWriterWrite(
					pWriter,
					XRT_BYTES_LITERAL("partial")
				 ) == XNET_RESULT_OK),
				"WebSocket abandoned Writer could not start"
			);
			xrtWsWriterDestroy(pWriter);
		}
	#else
		testRequire(
			xrtWsConnClose(
				pConnection,
				XWS_CLOSE_NORMAL,
				XRT_STR_LITERAL("done")
			) == XNET_RESULT_OK,
			"WebSocket client Close send failed"
		);
		testRequire(
			(xrtWsConnClose(
				pConnection,
				XWS_CLOSE_NORMAL,
				XRT_STR_LITERAL("again")
			 ) == XNET_RESULT_CLOSED) &&
			(xrtWsConnError(pConnection) == NULL),
			"WebSocket duplicate Close changed the session error"
		);
		#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF)
			xatomic32 Release;
			xnetref Ref;
			bytes pPayload;

			xrtAtomic32Init(&Release, 0);
			pPayload = (bytes)xrtMalloc(1);
			testRequire(
				pPayload != NULL,
				"WebSocket closed reference allocation failed"
			);
			Ref = (xnetref) {
				pPayload,
				1,
				testWsConnRefRelease,
				&Release
			};
			testRequire(
				(xrtWsConnBinaryRef(
					pConnection,
					&Ref
				 ) == XNET_RESULT_CLOSED) &&
				(xrtAtomic32Load(
					&Release,
					XMEMORY_ACQUIRE
				 ) == 0),
				"WebSocket closed reference transferred ownership"
			);
			xrtFree(pPayload);
		#endif
	#endif
}



/* 验证统一 TCP 会话的流式消息、掩码、控制帧和 Close 生命周期。 */
int main(void)
{
	test_ws_connection Test;
	uint8 CloseStorage[sizeof(xwsconnclose) + 2u];
	xallocator Allocator;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents ClientEvents;
	xnetstreamconfig StreamConfig;
	xnetaddr Address;
	xnetstream* pClientStream;
	xnetstream* pClientTransport;
	xnetstream* pServerTransport;
	xwsconn* pClient;
	xwsconn* pServer;
	xwsconnclose OpenClose;
	xdeadline AttachDeadline;
	uint32 iExpectedMessages = 2;
	uint32 iExpectedClientMessages;

	memset(&Test, 0, sizeof(Test));
	xrtAtomic32Init(&Test.FailAlloc, 0);
	xrtAtomic64Init(&Test.FailThread, 0);
	xrtAtomic64Init(&Test.FailAttempts, 0);
	xrtAtomic64Init(&Test.AllocAttempts, 0);
	Allocator.Context = &Test;
	Allocator.Alloc = testWsConnAlloc;
	Allocator.Realloc = testWsConnRealloc;
	Allocator.Free = testWsConnFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"WebSocket OOM allocator install failed"
	);
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	xrtAtomicPtrInit(&Test.Client, NULL);
	xrtAtomicPtrInit(&Test.Server, NULL);
	xrtAtomic32Init(&Test.ListenerClosed, 0);
	xrtAtomic32Init(&Test.ListenerErrors, 0);
	xrtAtomic32Init(&Test.PauseSeen, 0);
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF)
		xrtAtomic32Init(&Test.ClientRefRelease, 0);
		xrtAtomic32Init(&Test.ServerRefRelease, 0);
		xrtAtomic32Init(&Test.RefTaskDone, 0);
		iExpectedMessages += 2u;
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		xrtAtomic32Init(&Test.WriterTaskDone, 0);
		iExpectedMessages += 4;
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
		iExpectedMessages += 2;
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_REF)
		xrtAtomic32Init(&Test.WriterRefRelease, 0);
		xrtAtomic32Init(
			&Test.WriterReentrantRelease,
			0
		);
		iExpectedMessages++;
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		iExpectedMessages++;
	#endif
	iExpectedClientMessages = iExpectedMessages;
	Test.ClientEndpoint.Test = &Test;
	Test.ClientEndpoint.Role = XWS_ROLE_CLIENT;
	Test.ServerEndpoint.Test = &Test;
	Test.ServerEndpoint.Role = XWS_ROLE_SERVER;
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		xrtAtomic32Init(&Test.ClientEndpoint.WriterText, 0);
		xrtAtomic32Init(&Test.ClientEndpoint.WriterBinary, 0);
		xrtAtomic32Init(&Test.ServerEndpoint.WriterText, 0);
		xrtAtomic32Init(&Test.ServerEndpoint.WriterBinary, 0);
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
		xrtAtomic32Init(
			&Test.ClientEndpoint.WriterCompressedText,
			0
		);
		xrtAtomic32Init(
			&Test.ClientEndpoint.WriterCompressedBinary,
			0
		);
		xrtAtomic32Init(
			&Test.ServerEndpoint.WriterCompressedText,
			0
		);
		xrtAtomic32Init(
			&Test.ServerEndpoint.WriterCompressedBinary,
			0
		);
	#endif
	Test.Events.MessageBegin = testWsConnMessageBegin;
	Test.Events.MessageData = testWsConnMessageData;
	Test.Events.MessageEnd = testWsConnMessageEnd;
	Test.Events.Ping = testWsConnPing;
	Test.Events.Pong = testWsConnPong;
	Test.Events.Error = testWsConnError;
	Test.Events.Close = testWsConnCloseEvent;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_WS_CONNECTION_BACKEND;
	EngineConfig.Workers = 2;
	Test.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(Test.Engine != NULL) &&
		xrtNetEngineStart(Test.Engine),
		"WebSocket connection engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"WebSocket listener address setup failed"
	);
	ListenConfig.Stream.ReadSize = 2;
	ListenConfig.Stream.ReadLimit = 256;
	ListenConfig.Stream.WriteLimit =
		TEST_WS_CONNECTION_SEND_LIMIT;
	ListenConfig.Stream.WriteHighWater = 32768;
	ListenConfig.Stream.WriteLowWater = 8192;
	ListenerEvents.Accept = testWsConnAccept;
	ListenerEvents.Error = testWsConnListenerError;
	ListenerEvents.Close = testWsConnListenerClose;
	Test.Listener = xrtNetListen(
		Test.Engine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&Test
	);
	testRequire(
		(Test.Listener != NULL) &&
		xrtNetListenerLocal(Test.Listener, &Address),
		"WebSocket listener start failed"
	);

	xrtNetStreamConfigInit(&StreamConfig);
	StreamConfig.ReadSize = 2;
	StreamConfig.ReadLimit = 256;
	StreamConfig.WriteLimit =
		TEST_WS_CONNECTION_SEND_LIMIT;
	StreamConfig.WriteHighWater = 32768;
	StreamConfig.WriteLowWater = 8192;
	ClientEvents.Open = testWsConnClientOpen;
	pClientStream = xrtNetStreamConnect(
		Test.Engine,
		&Address,
		1,
		&StreamConfig,
		&ClientEvents,
		&Test
	);
	testRequire(
		pClientStream != NULL,
		"WebSocket client TCP connect failed"
	);
	AttachDeadline = xrtDeadlineAfter(
		UINT64_C(10000000)
	);
	while ( ((pClient = (xwsconn*)xrtAtomicPtrLoad(
		&Test.Client,
		XMEMORY_ACQUIRE
	)) == NULL) || ((pServer = (xwsconn*)xrtAtomicPtrLoad(
		&Test.Server,
		XMEMORY_ACQUIRE
	)) == NULL) ) {
		testRequire(
			!xrtDeadlineExpired(AttachDeadline),
			"WebSocket connections were not attached"
		);
		xrtThreadYield();
	}
	testRequire(
		(xrtWsConnRole(pClient) == XWS_ROLE_CLIENT) &&
		(xrtWsConnRole(pServer) == XWS_ROLE_SERVER) &&
		(xrtWsConnProtocol(pClient).Size == 7) &&
		(memcmp(
			xrtWsConnProtocol(pClient).Data,
			"chat.v1",
			7
		 ) == 0) &&
		(xrtWsConnProtocol(pServer).Size == 7) &&
		(memcmp(
			xrtWsConnProtocol(pServer).Data,
			"chat.v1",
			7
		 ) == 0) &&
		(xrtWsConnTcp(pClient) == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_STATE) &&
		(xrtWsConnError(pClient) == NULL),
		"WebSocket borrowed transport escaped its worker"
	);
	xrtClearError();
	pClientTransport = xrtWsConnTcpRef(pClient);
	pServerTransport = xrtWsConnTcpRef(pServer);
	memset(CloseStorage, 0xA5, sizeof(CloseStorage));
	testRequire(
		(pClientTransport != NULL) &&
		(pServerTransport != NULL) &&
		xrtWsConnCloseInfo(
			pClient,
			(xwsconnclose*)(void*)(CloseStorage + 1u)
		) &&
		(CloseStorage[0] == 0xA5) &&
		(CloseStorage[sizeof(CloseStorage) - 1u] == 0xA5),
		"WebSocket Close query rejected unaligned output"
	);
	memcpy(&OpenClose, CloseStorage + 1u, sizeof(OpenClose));
	testRequire(
		(OpenClose.Flags == 0) &&
		(OpenClose.Transport == XNET_RESULT_OK),
		"WebSocket attached role or transport mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtWsConnCloseInfo(
			pClient,
			(xwsconnclose*)(uintptr_t)(UINTPTR_MAX - 1u)
		) &&
		(xrtErrorCode(xrtGetError()) == XWS_CONN_ERROR_ARGUMENT) &&
		!xrtWsConnCloseInfo(
			pClient,
			(xwsconnclose*)(void*)pClient
		) &&
		(xrtWsConnState(pClient) == XWS_CONN_OPEN) &&
		(xrtWsConnProtocol(pClient).Size == 7u),
		"WebSocket Close query accepted an invalid output range"
	);
	xrtClearError();
	xrtNetStreamDestroy(pClientTransport);
	xrtNetStreamDestroy(pServerTransport);
	testWsConnRejectCall(
		pClient,
		xrtWsConnText(
			pClient,
			XRT_STR_LITERAL("off-worker")
		),
		XWS_CONN_ERROR_STATE,
		"WebSocket accepted an off-worker send"
	);
	testWsConnRejectCall(
		pClient,
		xrtWsConnPong(
			pClient,
			XRT_BYTES_LITERAL("off-worker")
		),
		XWS_CONN_ERROR_STATE,
		"WebSocket accepted an off-worker Pong"
	);
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF)
		{
			xatomic32 Release;
			bytes pPayload = (bytes)xrtMalloc(1);
			xnetref Ref;

			testRequire(
				pPayload != NULL,
				"WebSocket off-worker reference allocation failed"
			);
			xrtAtomic32Init(&Release, 0);
			Ref = (xnetref) {
				pPayload,
				1,
				testWsConnRefRelease,
				&Release
			};
			testWsConnRejectCall(
				pClient,
				xrtWsConnBinaryRef(pClient, &Ref),
				XWS_CONN_ERROR_STATE,
				"WebSocket accepted an off-worker reference"
			);
			testRequire(
				xrtAtomic32Load(
					&Release,
					XMEMORY_ACQUIRE
				) == 0,
				"WebSocket off-worker reference transferred ownership"
			);
			xrtFree(pPayload);
		}
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		xrtClearError();
		testRequire(
			(xrtWsConnBeginText(pClient) == NULL) &&
			(xrtErrorCode(xrtGetError()) ==
			 XWS_CONN_ERROR_STATE),
			"WebSocket accepted an off-worker Writer"
		);
		xrtClearError();
	#endif
	testRequire(
		xrtNetEnginePost(
			Test.Engine,
			xrtNetWorkerIndex(
				xrtWsConnWorker(pClient)
			),
			testWsConnSendTask,
			&Test
		),
		"WebSocket send task post failed"
	);
	testWsConnWait(
		&Test.ServerEndpoint.Messages,
		iExpectedMessages,
		"WebSocket server messages missing"
	);
	testWsConnWait(
		&Test.ClientEndpoint.Messages,
		iExpectedMessages,
		"WebSocket client echoes missing"
	);
	testWsConnWait(
		&Test.ServerEndpoint.Ping,
		1,
		"WebSocket server Ping event missing"
	);
	testWsConnWait(
		&Test.ClientEndpoint.Pong,
		1,
		"WebSocket client Pong event missing"
	);
	testRequire(
		(xrtAtomic32Load(
			&Test.ServerEndpoint.Begun,
			XMEMORY_ACQUIRE
		 ) == iExpectedMessages) &&
		(xrtAtomic32Load(
			&Test.ClientEndpoint.Begun,
			XMEMORY_ACQUIRE
		 ) == iExpectedMessages) &&
		(Test.ServerEndpoint.Size == 8) &&
		(memcmp(
			Test.ServerEndpoint.Data,
			"fragment",
			8
		 ) == 0),
		"WebSocket fragmented message reconstruction mismatch"
	);
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		testWsConnWait(
			&Test.ServerEndpoint.WriterText,
			1,
			"WebSocket client Writer message was not reconstructed"
		);
		testRequire(
			xrtNetEnginePost(
				Test.Engine,
				xrtNetWorkerIndex(
					xrtWsConnWorker(pServer)
				),
				testWsConnServerWriterTask,
				&Test
			),
			"WebSocket server Writer task post failed"
		);
		testWsConnWait(
			&Test.WriterTaskDone,
			1,
			"WebSocket server Writer task did not finish"
		);
		testWsConnWait(
			&Test.ClientEndpoint.WriterBinary,
			#if defined(XWS_FEATURE_WEBSOCKET_WRITER_REF)
				2,
			#else
				1,
			#endif
			"WebSocket server Writer message was not reconstructed"
		);
		#if defined(XWS_FEATURE_WEBSOCKET_WRITER_REF)
			iExpectedClientMessages += 2u;
		#else
			iExpectedClientMessages++;
		#endif
		#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
			testWsConnWait(
				&Test.ServerEndpoint.WriterCompressedText,
				1,
				"WebSocket compressed Text Writer message was not reconstructed"
			);
			testWsConnWait(
				&Test.ServerEndpoint.WriterCompressedBinary,
				1,
				"WebSocket compressed Binary Writer message was not reconstructed"
			);
			testWsConnWait(
				&Test.ClientEndpoint.WriterCompressedBinary,
				1,
				"WebSocket server compressed Writer message was not reconstructed"
			);
			iExpectedClientMessages++;
		#endif
		#if defined(XWS_FEATURE_WEBSOCKET_WRITER_REF)
			testWsConnWait(
				&Test.WriterRefRelease,
				2,
				"WebSocket Writer references were not released"
			);
			testRequire(
				xrtAtomic32Load(
					&Test.WriterRefRelease,
					XMEMORY_ACQUIRE
				) == 2,
				"WebSocket Writer references were not released exactly once"
			);
		#endif
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF)
		AttachDeadline = xrtDeadlineAfter(
			UINT64_C(10000000)
		);
		while ( xrtWsConnPending(pServer) != 0 ) {
			testRequire(
				!xrtDeadlineExpired(AttachDeadline),
				"WebSocket server reference precondition did not drain"
			);
			xrtThreadYield();
		}
		testRequire(
			xrtNetEnginePost(
				Test.Engine,
				xrtNetWorkerIndex(
					xrtWsConnWorker(pServer)
				),
				testWsConnServerRefTask,
				&Test
			),
			"WebSocket server reference task post failed"
		);
		testWsConnWait(
			&Test.RefTaskDone,
			1,
			"WebSocket server reference task did not finish"
		);
		testWsConnWait(
			&Test.ServerRefRelease,
			1,
			"WebSocket server reference was not released"
		);
		testWsConnWait(
			&Test.ClientEndpoint.Messages,
			iExpectedClientMessages + 2u,
			"WebSocket server ownership messages were not delivered"
		);
		testRequire(
			(xrtAtomic32Load(
				&Test.ServerRefRelease,
				XMEMORY_ACQUIRE
			 ) == 1) &&
			(Test.ClientEndpoint.Size == 5) &&
			(memcmp(
				Test.ClientEndpoint.Data,
				"taken",
				5
			 ) == 0),
			"WebSocket server reference release or delivery mismatch"
		);
		iExpectedClientMessages += 2u;
	#endif
	testRequire(
		xrtNetEnginePost(
			Test.Engine,
			xrtNetWorkerIndex(
				xrtWsConnWorker(pClient)
			),
			testWsConnPauseTask,
			&Test
		),
		"WebSocket pause task post failed"
	);
	testWsConnWait(
		&Test.PauseSeen,
		1,
		"WebSocket server did not enter the paused state"
	);
	testRequire(
		xrtWsConnPaused(pServer) &&
		(xrtAtomic32Load(
			&Test.ServerEndpoint.Messages,
			XMEMORY_ACQUIRE
		 ) == (iExpectedMessages + 1u)),
		"WebSocket pause crossed the next message boundary"
	);
	testRequire(
		xrtWsConnResume(pServer) &&
		!xrtWsConnPaused(pServer),
		"WebSocket cross-thread resume failed"
	);
	testWsConnWait(
		&Test.ServerEndpoint.Messages,
		iExpectedMessages + 2u,
		"WebSocket resumed server message missing"
	);
	testWsConnWait(
		&Test.ClientEndpoint.Messages,
		iExpectedClientMessages + 2u,
		"WebSocket resumed client echoes missing"
	);
	testRequire(
		(Test.ServerEndpoint.Size == 5) &&
		(memcmp(
			Test.ServerEndpoint.Data,
			"after",
			5
		 ) == 0),
		"WebSocket resumed message ordering mismatch"
	);
	xrtWsConnPause(pClient);
	testRequire(
		xrtWsConnPaused(pClient),
		"WebSocket client did not pause before Close"
	);
	testRequire(
		xrtNetEnginePost(
			Test.Engine,
			xrtNetWorkerIndex(
				xrtWsConnWorker(pClient)
			),
			testWsConnCloseTask,
			&Test
		),
		"WebSocket Close task post failed"
	);
	testWsConnWait(
		&Test.ClientEndpoint.Closed,
		1,
		"WebSocket client Close event missing"
	);
	testWsConnWait(
		&Test.ServerEndpoint.Closed,
		1,
		"WebSocket server Close event missing"
	);
	testRequire(
		(xrtAtomic32Load(
			&Test.ClientEndpoint.Errors,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		(xrtAtomic32Load(
			&Test.ServerEndpoint.Errors,
		 XMEMORY_ACQUIRE
		 ) == 0) &&
		!xrtWsConnPaused(pClient) &&
		(xrtAtomic32Load(
			&Test.ListenerErrors,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		((Test.ClientEndpoint.Close.Flags &
		  XWS_CONN_CLOSE_REMOTE) == 0) &&
		((Test.ServerEndpoint.Close.Flags &
		  XWS_CONN_CLOSE_REMOTE) != 0),
		"WebSocket lifecycle error or Close initiator mismatch"
	);
	testRequire(
		xrtWsConnCloseInfo(pClient, &OpenClose) &&
		((OpenClose.Flags & XWS_CONN_CLOSE_CLEAN) != 0) &&
		(OpenClose.LocalCode ==
		 TEST_WS_CONNECTION_CLOSE_CODE) &&
		(OpenClose.RemoteCode ==
		 TEST_WS_CONNECTION_CLOSE_CODE) &&
		(OpenClose.Reason.Size ==
		 TEST_WS_CONNECTION_CLOSE_REASON_SIZE) &&
		(memcmp(
			OpenClose.Reason.Data,
			TEST_WS_CONNECTION_CLOSE_REASON,
			TEST_WS_CONNECTION_CLOSE_REASON_SIZE
		 ) == 0),
		"WebSocket terminal Close snapshot mismatch"
	);
	testRequire(
		!xrtWsConnAbort(pClient) &&
		!xrtWsConnAbort(pServer) &&
		(xrtWsConnState(pClient) == XWS_CONN_CLOSED) &&
		(xrtWsConnState(pServer) == XWS_CONN_CLOSED),
		"WebSocket Abort regressed a terminal connection state"
	);
	testRequire(
		xrtNetListenerClose(Test.Listener),
		"WebSocket listener close failed"
	);
	testWsConnWait(
		&Test.ListenerClosed,
		1,
		"WebSocket listener Close event missing"
	);
	xrtWsConnDestroy(pClient);
	xrtWsConnDestroy(pServer);
	xrtNetListenerDestroy(Test.Listener);
	testRequire(
		xrtNetEngineDestroy(Test.Engine),
		"WebSocket connection engine destroy failed"
	);
	printf(
		"[PASS] WebSocket established TCP connection\n"
	);
	return 0;
}
