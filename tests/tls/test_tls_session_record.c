#include "../test.h"
#include "../../src/internal/xrt_tls_session.h"



static const uint8 TestTlsSessionKey[16] = {
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};

static const uint8 TestTlsSessionIv13[12] = {
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
	0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B
};

static const uint8 TestTlsSessionIv12[4] = {
	0x30, 0x31, 0x32, 0x33
};



/* 创建使用最小合法队列上限的共享上下文。 */
static xtlscontext* testTlsSessionRecordContext(void)
{
	xtlscontextconfig Config;

	xrtTlsContextConfigInit(&Config);
	Config.Limits.FeedLimit = XTLS_RECORD_HEADER_SIZE +
		XTLS12_RECORD_CIPHERTEXT_MAX;
	Config.Limits.SendLimit = Config.Limits.FeedLimit;
	Config.Limits.PlainLimit = XTLS_RECORD_PLAINTEXT_MAX;
	return xrtTlsContextCreate(&Config);
}



/* 创建一个没有固定缓冲预分配的测试会话。 */
static xtlssession* testTlsSessionRecordCreate(
	const xtlscontext* pContext,
	xtlsrole Role
)
{
	xtlssession* pSession = __xrtTlsSessionCreate(
		pContext, NULL, Role
	);

	testRequire(pSession != NULL, "TLS record session creation failed");
	return pSession;
}



/* 按给定小分片把全部待发密文搬运到对端。 */
static size_t testTlsSessionRecordMove(
	xtlssession* pSource,
	xtlssession* pTarget,
	size_t iChunk
)
{
	xnetspan Span;
	size_t iMoved = 0;

	testRequire(iChunk != 0, "TLS record move chunk is zero");
	while ( xrtTlsSessionSendSize(pSource) != 0 ) {
		size_t iCopy;

		testRequire(xrtTlsSessionSendFront(pSource, &Span) &&
			(Span.Size != 0), "TLS record send front failed");
		iCopy = Span.Size < iChunk ? Span.Size : iChunk;
		testRequire(xrtTlsSessionFeed(
			pTarget, Span.Data, iCopy
		) == XTLS_OK, "TLS fragmented feed failed");
		testRequire(xrtTlsSessionSendConsume(
			pSource, iCopy
		), "TLS fragmented send consumption failed");
		iMoved += iCopy;
	}
	return iMoved;
}



/* 安装一对 TLS 1.3 AES-128-GCM 发送和接收密钥。 */
static void testTlsSessionRecordKeys13(
	xtlssession* pSender,
	xtlssession* pReceiver
)
{
	testRequire(__xrtTlsSessionWriteKey(
		pSender, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		(xbytesview) { TestTlsSessionKey, sizeof(TestTlsSessionKey) },
		(xbytesview) { TestTlsSessionIv13, sizeof(TestTlsSessionIv13) }
	) && __xrtTlsSessionReadKey(
		pReceiver, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		(xbytesview) { TestTlsSessionKey, sizeof(TestTlsSessionKey) },
		(xbytesview) { TestTlsSessionIv13, sizeof(TestTlsSessionIv13) }
	), "TLS 1.3 session record keys failed");
}



/* 把测试会话按公开生命周期推进到可读写状态。 */
static void testTlsSessionRecordReady(xtlssession* pSession)
{
	testRequire(__xrtTlsSessionSetState(
		pSession, XTLS_STATE_HANDSHAKE
	) && __xrtTlsSessionSetState(
		pSession, XTLS_STATE_READY
	), "TLS record session ready transition failed");
}



/* 初始明文握手记录必须支持任意小分片和稳定 AGAIN。 */
static void testTlsSessionRecordPlainFragment(void)
{
	xtlscontext* pContext = testTlsSessionRecordContext();
	xtlssession* pSender;
	xtlssession* pReceiver;
	xtlssessionrecord Record;
	xnetspan Span;
	xerror* pMarker;

	testRequire(pContext != NULL, "TLS plaintext record context failed");
	pSender = testTlsSessionRecordCreate(pContext, XTLS_CLIENT);
	pReceiver = testTlsSessionRecordCreate(pContext, XTLS_SERVER);
	testRequire(__xrtTlsSessionRecordPlain(
		pSender, XTLS_RECORD_HANDSHAKE, UINT16_C(0x0303),
		XRT_BYTES_LITERAL("client-hello")
	) == XTLS_OK, "TLS plaintext handshake queue failed");
	testRequire(xrtTlsSessionSendFront(pSender, &Span) &&
		(Span.Size > 3u), "TLS plaintext record span failed");
	testRequire(xrtTlsSessionFeed(
		pReceiver, Span.Data, 3u
	) == XTLS_OK && xrtTlsSessionSendConsume(pSender, 3u),
		"TLS plaintext prefix move failed");

	pMarker = xrtErrorCreate(XERR_VALUE, "test.tls", 101, "marker");
	testRequire(pMarker != NULL, "TLS fragment marker failed");
	xrtSetError(pMarker);
	testRequire(__xrtTlsSessionRecordNext(
		pReceiver, &Record
	) == XTLS_AGAIN && (xrtGetError() == pMarker),
		"TLS fragmented record changed AGAIN error state");
	testRequire(testTlsSessionRecordMove(
		pSender, pReceiver, 1u
	) != 0, "TLS byte fragmented record move failed");
	testRequire(__xrtTlsSessionRecordNext(
		pReceiver, &Record
	) == XTLS_OK && !Record.Protected &&
		(Record.Type == XTLS_RECORD_HANDSHAKE) &&
		(Record.Data.Size == 12u) &&
		(memcmp(Record.Data.Data, "client-hello", 12u) == 0),
		"TLS fragmented plaintext record mismatch");
	testRequire(__xrtTlsSessionRecordFinish(
		pReceiver, false
	) == XTLS_OK && (xrtTlsSessionFeedSize(pReceiver) == 0),
		"TLS plaintext record finish failed");

	xrtClearError();
	xrtErrorFree(pMarker);
	xrtTlsSessionDestroy(pReceiver);
	xrtTlsSessionDestroy(pSender);
	xrtTlsContextRelease(pContext);
}



/* TLS 1.3 应用记录必须从 Scratch 零拷贝移入明文队列。 */
static void testTlsSessionRecordProtected13(void)
{
	xtlscontext* pContext = testTlsSessionRecordContext();
	xtlssession* pSender;
	xtlssession* pReceiver;
	xtlssessionrecord Record;
	char sPlain[16] = { 0 };
	size_t iRead = 0;

	testRequire(pContext != NULL, "TLS 1.3 record context failed");
	pSender = testTlsSessionRecordCreate(pContext, XTLS_CLIENT);
	pReceiver = testTlsSessionRecordCreate(pContext, XTLS_SERVER);
	testTlsSessionRecordKeys13(pSender, pReceiver);
	testRequire(__xrtTlsSessionRecordProtect(
		pSender, XTLS_RECORD_APPLICATION_DATA,
		XRT_BYTES_LITERAL("protected"), 7u
	) == XTLS_OK && (pSender->WriteKey.Sequence == 1u),
		"TLS 1.3 protected record queue failed");
	testRequire(testTlsSessionRecordMove(
		pSender, pReceiver, 2u
	) != 0, "TLS 1.3 protected record move failed");
	testRequire(__xrtTlsSessionRecordNext(
		pReceiver, &Record
	) == XTLS_OK && Record.Protected &&
		(Record.Type == XTLS_RECORD_APPLICATION_DATA) &&
		(Record.Data.Size == 9u) &&
		(memcmp(Record.Data.Data, "protected", 9u) == 0) &&
		(pReceiver->ReadKey.Sequence == 1u) &&
		(xrtTlsSessionPlainSize(pReceiver) == 0),
		"TLS 1.3 protected record open failed");
	testRequire(__xrtTlsSessionRecordFinish(
		pReceiver, true
	) == XTLS_OK && (xrtTlsSessionPlainSize(pReceiver) == 9u),
		"TLS 1.3 application record publication failed");
	testRequire(xrtTlsSessionRead(
		pReceiver, sPlain, sizeof(sPlain), &iRead
	) == XTLS_OK && (iRead == 9u) &&
		(memcmp(sPlain, "protected", 9u) == 0),
		"TLS 1.3 application plaintext read failed");

	xrtTlsSessionDestroy(pReceiver);
	xrtTlsSessionDestroy(pSender);
	xrtTlsContextRelease(pContext);
}



/* 明文队列满时挂起记录、输入和序列号必须保持稳定。 */
static void testTlsSessionRecordPlainBackpressure(void)
{
	xtlscontext* pContext = testTlsSessionRecordContext();
	const xtlslimits* pLimits;
	xtlssession* pSender;
	xtlssession* pReceiver;
	xtlssessionrecord First;
	xtlssessionrecord Again;
	xerror* pMarker;
	uint8* pFill;
	char sPlain[8] = { 0 };
	size_t iRead = 0;
	size_t iFeed;

	testRequire(pContext != NULL, "TLS plaintext backpressure context failed");
	pLimits = xrtTlsContextLimits(pContext);
	pFill = (uint8*)xrtMalloc(pLimits->PlainLimit);
	testRequire(pFill != NULL, "TLS plaintext backpressure fill failed");
	memset(pFill, 0x5A, pLimits->PlainLimit);
	pSender = testTlsSessionRecordCreate(pContext, XTLS_CLIENT);
	pReceiver = testTlsSessionRecordCreate(pContext, XTLS_SERVER);
	testTlsSessionRecordKeys13(pSender, pReceiver);
	testRequire(__xrtTlsSessionPlain(
		pReceiver, pFill, pLimits->PlainLimit
	) == XTLS_OK, "TLS plaintext queue fill failed");
	testRequire(__xrtTlsSessionRecordProtect(
		pSender, XTLS_RECORD_APPLICATION_DATA,
		XRT_BYTES_LITERAL("blocked"), 0
	) == XTLS_OK, "TLS blocked application record queue failed");
	testRequire(testTlsSessionRecordMove(
		pSender, pReceiver, 13u
	) != 0, "TLS blocked application record move failed");
	iFeed = xrtTlsSessionFeedSize(pReceiver);
	testRequire(__xrtTlsSessionRecordNext(
		pReceiver, &First
	) == XTLS_OK && First.Protected &&
		(First.Data.Size == 7u), "TLS blocked record open failed");

	pMarker = xrtErrorCreate(XERR_VALUE, "test.tls", 102, "marker");
	testRequire(pMarker != NULL, "TLS backpressure marker failed");
	xrtSetError(pMarker);
	testRequire(__xrtTlsSessionRecordFinish(
		pReceiver, true
	) == XTLS_AGAIN && (xrtGetError() == pMarker) &&
		(xrtTlsSessionFeedSize(pReceiver) == iFeed) &&
		(pReceiver->ReadKey.Sequence == 1u),
		"TLS plaintext backpressure changed pending state");
	testRequire(__xrtTlsSessionRecordNext(
		pReceiver, &Again
	) == XTLS_OK && (Again.Data.Data == First.Data.Data) &&
		(Again.Data.Size == First.Data.Size) &&
		(pReceiver->ReadKey.Sequence == 1u),
		"TLS pending record was opened twice");
	testRequire(xrtTlsSessionPlainConsume(
		pReceiver, pLimits->PlainLimit
	), "TLS plaintext backpressure release failed");
	testRequire(__xrtTlsSessionRecordFinish(
		pReceiver, true
	) == XTLS_OK && (xrtTlsSessionFeedSize(pReceiver) == 0),
		"TLS pending record did not resume");
	testRequire(xrtTlsSessionRead(
		pReceiver, sPlain, sizeof(sPlain), &iRead
	) == XTLS_OK && (iRead == 7u) &&
		(memcmp(sPlain, "blocked", 7u) == 0),
		"TLS resumed plaintext mismatch");

	xrtClearError();
	xrtErrorFree(pMarker);
	xrtTlsSessionDestroy(pReceiver);
	xrtTlsSessionDestroy(pSender);
	xrtFree(pFill);
	xrtTlsContextRelease(pContext);
}



/* 发送队列满和密钥替换失败都不得消耗序列号。 */
static void testTlsSessionRecordSendAtomic(void)
{
	xtlscontext* pContext = testTlsSessionRecordContext();
	const xtlslimits* pLimits;
	xtlssession* pSession;
	xerror* pMarker;
	uint8* pFill;
	uint8 BadKey[15] = { 0 };

	testRequire(pContext != NULL, "TLS send atomic context failed");
	pLimits = xrtTlsContextLimits(pContext);
	pFill = (uint8*)xrtMalloc(pLimits->SendLimit);
	testRequire(pFill != NULL, "TLS send atomic fill failed");
	memset(pFill, 0xA5, pLimits->SendLimit);
	pSession = testTlsSessionRecordCreate(pContext, XTLS_CLIENT);
	testRequire(__xrtTlsSessionWriteKey(
		pSession, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		(xbytesview) { TestTlsSessionKey, sizeof(TestTlsSessionKey) },
		(xbytesview) { TestTlsSessionIv13, sizeof(TestTlsSessionIv13) }
	), "TLS send atomic key failed");
	testRequire(__xrtTlsSessionSend(
		pSession, pFill, pLimits->SendLimit
	) == XTLS_OK, "TLS send queue fill failed");

	pMarker = xrtErrorCreate(XERR_VALUE, "test.tls", 103, "marker");
	testRequire(pMarker != NULL, "TLS send atomic marker failed");
	xrtSetError(pMarker);
	testRequire(__xrtTlsSessionRecordProtect(
		pSession, XTLS_RECORD_APPLICATION_DATA,
		XRT_BYTES_LITERAL("again"), 0
	) == XTLS_AGAIN && (xrtGetError() == pMarker) &&
		(pSession->WriteKey.Sequence == 0),
		"TLS send backpressure consumed a sequence");
	testRequire(xrtTlsSessionSendConsume(
		pSession, pLimits->SendLimit
	), "TLS send queue release failed");
	testRequire(__xrtTlsSessionRecordProtect(
		pSession, XTLS_RECORD_APPLICATION_DATA,
		XRT_BYTES_LITERAL("again"), 0
	) == XTLS_OK && (pSession->WriteKey.Sequence == 1u),
		"TLS send queue did not recover");

	xrtClearError();
	testRequire(!__xrtTlsSessionWriteKey(
		pSession, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		(xbytesview) { BadKey, sizeof(BadKey) },
		(xbytesview) { TestTlsSessionIv13, sizeof(TestTlsSessionIv13) }
	) && pSession->WriteKey.Ready &&
		(pSession->WriteKey.Sequence == 1u),
		"TLS failed key replacement changed the active epoch");

	xrtClearError();
	xrtErrorFree(pMarker);
	xrtTlsSessionDestroy(pSession);
	xrtFree(pFill);
	xrtTlsContextRelease(pContext);
}



/* 密文认证失败不得消费输入、发布明文或前移序列号。 */
static void testTlsSessionRecordTamper(void)
{
	xtlscontext* pContext = testTlsSessionRecordContext();
	xtlssession* pSender;
	xtlssession* pReceiver;
	xtlssessionrecord Record;
	uint8 Encoded[128];
	size_t iSize;

	testRequire(pContext != NULL, "TLS tamper context failed");
	pSender = testTlsSessionRecordCreate(pContext, XTLS_CLIENT);
	pReceiver = testTlsSessionRecordCreate(pContext, XTLS_SERVER);
	testTlsSessionRecordKeys13(pSender, pReceiver);
	testRequire(__xrtTlsSessionRecordProtect(
		pSender, XTLS_RECORD_APPLICATION_DATA,
		XRT_BYTES_LITERAL("tamper"), 0
	) == XTLS_OK, "TLS tamper record queue failed");
	iSize = xrtTlsSessionSendSize(pSender);
	testRequire((iSize <= sizeof(Encoded)) &&
		(xrtNetBufPeek(&pSender->Send, 0, Encoded, iSize) == iSize),
		"TLS tamper record copy failed");
	Encoded[iSize - 1u] ^= 0x01u;
	testRequire(xrtTlsSessionFeed(
		pReceiver, Encoded, iSize
	) == XTLS_OK, "TLS tamper record feed failed");
	xrtClearError();
	testRequire(__xrtTlsSessionRecordNext(
		pReceiver, &Record
	) == XTLS_ERROR && (pReceiver->ReadKey.Sequence == 0) &&
		(xrtTlsSessionFeedSize(pReceiver) == iSize) &&
		xrtNetBufEmpty(&pReceiver->Scratch) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_CIPHER),
		"TLS tamper failure changed record state");

	xrtTlsSessionDestroy(pReceiver);
	xrtTlsSessionDestroy(pSender);
	xrtTlsContextRelease(pContext);
}



/* TLS 1.2 受保护应用记录与已装密钥后的明文 CCS 必须共存。 */
static void testTlsSessionRecordTls12AndCcs(void)
{
	xtlscontext* pContext = testTlsSessionRecordContext();
	xtlssession* pSender;
	xtlssession* pReceiver;
	xtlssessionrecord Record;
	const uint8 Ccs = 1;
	char sPlain[8] = { 0 };
	size_t iRead = 0;

	testRequire(pContext != NULL, "TLS 1.2 record context failed");
	pSender = testTlsSessionRecordCreate(pContext, XTLS_CLIENT);
	pReceiver = testTlsSessionRecordCreate(pContext, XTLS_SERVER);
	testRequire(__xrtTlsSessionWriteKey(
		pSender, XTLS_VERSION_12,
		XTLS_ECDHE_RSA_AES_128_GCM_SHA256,
		(xbytesview) { TestTlsSessionKey, sizeof(TestTlsSessionKey) },
		(xbytesview) { TestTlsSessionIv12, sizeof(TestTlsSessionIv12) }
	) && __xrtTlsSessionReadKey(
		pReceiver, XTLS_VERSION_12,
		XTLS_ECDHE_RSA_AES_128_GCM_SHA256,
		(xbytesview) { TestTlsSessionKey, sizeof(TestTlsSessionKey) },
		(xbytesview) { TestTlsSessionIv12, sizeof(TestTlsSessionIv12) }
	), "TLS 1.2 session record keys failed");
	testRequire(__xrtTlsSessionRecordProtect(
		pSender, XTLS_RECORD_APPLICATION_DATA,
		XRT_BYTES_LITERAL("tls12"), 0
	) == XTLS_OK, "TLS 1.2 protected record queue failed");
	testRequire(testTlsSessionRecordMove(
		pSender, pReceiver, 3u
	) != 0, "TLS 1.2 protected record move failed");
	testRequire(__xrtTlsSessionRecordNext(
		pReceiver, &Record
	) == XTLS_OK && Record.Protected &&
		(Record.Type == XTLS_RECORD_APPLICATION_DATA) &&
		(Record.Data.Size == 5u) &&
		__xrtTlsSessionRecordFinish(pReceiver, true) == XTLS_OK,
		"TLS 1.2 protected record open failed");
	testRequire(xrtTlsSessionRead(
		pReceiver, sPlain, sizeof(sPlain), &iRead
	) == XTLS_OK && (iRead == 5u) &&
		(memcmp(sPlain, "tls12", 5u) == 0),
		"TLS 1.2 application plaintext mismatch");

	testRequire(__xrtTlsSessionRecordPlain(
		pSender, XTLS_RECORD_CHANGE_CIPHER_SPEC,
		UINT16_C(0x0303), (xbytesview) { &Ccs, 1u }
	) == XTLS_OK, "TLS CCS queue failed");
	testRequire(testTlsSessionRecordMove(
		pSender, pReceiver, 1u
	) != 0, "TLS CCS move failed");
	testRequire(__xrtTlsSessionRecordNext(
		pReceiver, &Record
	) == XTLS_OK && !Record.Protected &&
		(Record.Type == XTLS_RECORD_CHANGE_CIPHER_SPEC) &&
		(Record.Data.Size == 1u) && (Record.Data.Data[0] == 1u) &&
		__xrtTlsSessionRecordFinish(pReceiver, false) == XTLS_OK,
		"TLS CCS was incorrectly decrypted");

	xrtTlsSessionDestroy(pReceiver);
	xrtTlsSessionDestroy(pSender);
	xrtTlsContextRelease(pContext);
}



/* 未受保护的应用记录不得进入公开明文队列。 */
static void testTlsSessionRecordRejectPlainApplication(void)
{
	xtlscontext* pContext = testTlsSessionRecordContext();
	xtlssession* pSender;
	xtlssession* pReceiver;
	xtlssessionrecord Record;

	testRequire(pContext != NULL, "TLS plaintext application context failed");
	pSender = testTlsSessionRecordCreate(pContext, XTLS_CLIENT);
	pReceiver = testTlsSessionRecordCreate(pContext, XTLS_SERVER);
	testRequire(__xrtTlsSessionRecordPlain(
		pSender, XTLS_RECORD_APPLICATION_DATA,
		UINT16_C(0x0303), XRT_BYTES_LITERAL("plain")
	) == XTLS_OK, "TLS plaintext application queue failed");
	testRequire(testTlsSessionRecordMove(
		pSender, pReceiver, 7u
	) != 0, "TLS plaintext application move failed");
	testRequire(__xrtTlsSessionRecordNext(
		pReceiver, &Record
	) == XTLS_OK && !Record.Protected,
		"TLS plaintext application parse failed");
	xrtClearError();
	testRequire(__xrtTlsSessionRecordFinish(
		pReceiver, true
	) == XTLS_ERROR &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_RECORD_TYPE) &&
		(xrtTlsSessionPlainSize(pReceiver) == 0) &&
		(xrtTlsSessionFeedSize(pReceiver) != 0),
		"TLS plaintext application escaped protection check");
	testRequire(__xrtTlsSessionRecordFinish(
		pReceiver, false
	) == XTLS_OK && (xrtTlsSessionFeedSize(pReceiver) == 0),
		"TLS rejected plaintext application could not be discarded");

	xrtTlsSessionDestroy(pReceiver);
	xrtTlsSessionDestroy(pSender);
	xrtTlsContextRelease(pContext);
}



/* 大应用写入必须按记录切分，并在发送硬上限处成功短写。 */
static void testTlsSessionRecordWritePartial(void)
{
	static uint8 Data[40000];
	xtlscontext* pContext = testTlsSessionRecordContext();
	xtlssession* pSender;
	xtlssession* pReceiver;
	xtlssessionrecord Record;
	uint8 Output[XTLS_RECORD_PLAINTEXT_MAX];
	size_t iWritten = 0;
	size_t iRead = 0;

	testRequire(pContext != NULL, "TLS partial write context failed");
	for ( size_t i = 0; i < sizeof(Data); i++ ) {
		Data[i] = (uint8)i;
	}
	pSender = testTlsSessionRecordCreate(pContext, XTLS_CLIENT);
	pReceiver = testTlsSessionRecordCreate(pContext, XTLS_SERVER);
	testTlsSessionRecordKeys13(pSender, pReceiver);
	testTlsSessionRecordReady(pSender);
	testTlsSessionRecordReady(pReceiver);
	testRequire(xrtTlsSessionWrite(
		pSender, Data, sizeof(Data), &iWritten
	) == XTLS_OK && (iWritten == XTLS_RECORD_PLAINTEXT_MAX) &&
		(pSender->WriteKey.Sequence == 1u) &&
		(xrtTlsSessionWait(pSender) == XTLS_WAIT_OUTPUT),
		"TLS large write did not stop at send backpressure");
	testRequire(testTlsSessionRecordMove(
		pSender, pReceiver, 31u
	) != 0, "TLS partial write first record move failed");
	testRequire(__xrtTlsSessionRecordNext(
		pReceiver, &Record
	) == XTLS_OK && (Record.Data.Size == sizeof(Output)) &&
		__xrtTlsSessionRecordFinish(pReceiver, true) == XTLS_OK,
		"TLS partial write first record open failed");
	testRequire(xrtTlsSessionRead(
		pReceiver, Output, sizeof(Output), &iRead
	) == XTLS_OK && (iRead == sizeof(Output)) &&
		(memcmp(Output, Data, sizeof(Output)) == 0),
		"TLS partial write first plaintext mismatch");

	testRequire(xrtTlsSessionWrite(
		pSender, Data + iWritten, sizeof(Data) - iWritten, &iRead
	) == XTLS_OK && (iRead == XTLS_RECORD_PLAINTEXT_MAX) &&
		(pSender->WriteKey.Sequence == 2u),
		"TLS large write did not resume after drain");
	testRequire(testTlsSessionRecordMove(
		pSender, pReceiver, 127u
	) != 0, "TLS partial write second record move failed");
	testRequire(__xrtTlsSessionRecordNext(
		pReceiver, &Record
	) == XTLS_OK &&
		__xrtTlsSessionRecordFinish(pReceiver, true) == XTLS_OK &&
		xrtTlsSessionPlainConsume(pReceiver, Record.Data.Size),
		"TLS partial write second record failed");

	testRequire(xrtTlsSessionWrite(
		pSender, Data + (2u * XTLS_RECORD_PLAINTEXT_MAX),
		sizeof(Data) - (2u * XTLS_RECORD_PLAINTEXT_MAX), &iRead
	) == XTLS_OK &&
		(iRead == (sizeof(Data) - (2u * XTLS_RECORD_PLAINTEXT_MAX))) &&
		(pSender->WriteKey.Sequence == 3u),
		"TLS large write tail failed");

	xrtTlsSessionDestroy(pReceiver);
	xrtTlsSessionDestroy(pSender);
	xrtTlsContextRelease(pContext);
}



/* close_notify 必须自动应答、只排队一次并在双向排空后关闭。 */
static void testTlsSessionRecordClose(void)
{
	xtlscontext* pContext = testTlsSessionRecordContext();
	xtlssession* pClient;
	xtlssession* pServer;
	xtlssessionrecord Record;
	xtlsalertlevel Level = XTLS_ALERT_FATAL;
	xtlsalert Alert = XTLS_ALERT_INTERNAL_ERROR;
	size_t iQueued;
	char iByte;
	size_t iRead = SIZE_MAX;

	testRequire(pContext != NULL, "TLS close context failed");
	pClient = testTlsSessionRecordCreate(pContext, XTLS_CLIENT);
	pServer = testTlsSessionRecordCreate(pContext, XTLS_SERVER);
	testTlsSessionRecordKeys13(pClient, pServer);
	testTlsSessionRecordKeys13(pServer, pClient);
	testTlsSessionRecordReady(pClient);
	testTlsSessionRecordReady(pServer);
	testRequire(xrtTlsSessionClose(pClient) == XTLS_OK &&
		pClient->CloseSent &&
		(xrtTlsSessionState(pClient) == XTLS_STATE_CLOSING),
		"TLS close_notify queue failed");
	iQueued = xrtTlsSessionSendSize(pClient);
	testRequire((iQueued != 0) &&
		(xrtTlsSessionClose(pClient) == XTLS_AGAIN) &&
		(xrtTlsSessionSendSize(pClient) == iQueued),
		"TLS close_notify was queued more than once");
	testRequire(testTlsSessionRecordMove(
		pClient, pServer, 1u
	) != 0, "TLS close_notify client move failed");
	testRequire(__xrtTlsSessionRecordNext(
		pServer, &Record
	) == XTLS_OK &&
		(Record.Type == XTLS_RECORD_ALERT) &&
		(__xrtTlsSessionRecordAlert(pServer, &Record) == XTLS_OK) &&
		pServer->CloseReceived && pServer->CloseSent &&
		xrtTlsSessionPeerAlert(pServer, &Level, &Alert) &&
		(Level == XTLS_ALERT_WARNING) &&
		(Alert == XTLS_ALERT_CLOSE_NOTIFY),
		"TLS close_notify server handling failed");
	testRequire(xrtTlsSessionRead(
		pServer, &iByte, 1u, &iRead
	) == XTLS_CLOSED && (iRead == 0),
		"TLS peer close did not close the read direction");
	testRequire(testTlsSessionRecordMove(
		pServer, pClient, 2u
	) != 0 &&
		(xrtTlsSessionState(pServer) == XTLS_STATE_CLOSED),
		"TLS close_notify server response did not drain");
	testRequire(__xrtTlsSessionRecordNext(
		pClient, &Record
	) == XTLS_OK &&
		(__xrtTlsSessionRecordAlert(pClient, &Record) == XTLS_CLOSED) &&
		(xrtTlsSessionState(pClient) == XTLS_STATE_CLOSED) &&
		(xrtTlsSessionEof(pClient) == XTLS_CLOSED),
		"TLS close_notify client completion failed");

	xrtTlsSessionDestroy(pServer);
	xrtTlsSessionDestroy(pClient);
	xrtTlsContextRelease(pContext);
}



/* fatal Alert 和未认证 EOF 必须成为不同的结构化失败。 */
static void testTlsSessionRecordFailure(void)
{
	const uint8 Fatal[2] = {
		XTLS_ALERT_FATAL,
		XTLS_ALERT_HANDSHAKE_FAILURE
	};
	xtlscontext* pContext = testTlsSessionRecordContext();
	xtlssession* pSender;
	xtlssession* pReceiver;
	xtlssession* pTruncated;
	xtlssessionrecord Record;
	xtlsalertlevel Level = XTLS_ALERT_WARNING;
	xtlsalert Alert = XTLS_ALERT_CLOSE_NOTIFY;

	testRequire(pContext != NULL, "TLS failure context failed");
	pSender = testTlsSessionRecordCreate(pContext, XTLS_CLIENT);
	pReceiver = testTlsSessionRecordCreate(pContext, XTLS_SERVER);
	pTruncated = testTlsSessionRecordCreate(pContext, XTLS_CLIENT);
	testTlsSessionRecordKeys13(pSender, pReceiver);
	testTlsSessionRecordReady(pSender);
	testTlsSessionRecordReady(pReceiver);
	testTlsSessionRecordReady(pTruncated);
	testRequire(__xrtTlsSessionRecordProtect(
		pSender, XTLS_RECORD_ALERT,
		(xbytesview) { Fatal, sizeof(Fatal) }, 0
	) == XTLS_OK && testTlsSessionRecordMove(
		pSender, pReceiver, 5u
	) != 0, "TLS fatal alert move failed");
	testRequire(__xrtTlsSessionRecordNext(
		pReceiver, &Record
	) == XTLS_OK, "TLS fatal alert open failed");
	xrtClearError();
	testRequire(__xrtTlsSessionRecordAlert(
		pReceiver, &Record
	) == XTLS_ERROR &&
		(xrtTlsSessionState(pReceiver) == XTLS_STATE_FAILED) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_ALERT) &&
		xrtTlsSessionPeerAlert(pReceiver, &Level, &Alert) &&
		(Level == XTLS_ALERT_FATAL) &&
		(Alert == XTLS_ALERT_HANDSHAKE_FAILURE),
		"TLS fatal alert did not publish failure metadata");

	xrtClearError();
	testRequire(xrtTlsSessionEof(pTruncated) == XTLS_ERROR &&
		(xrtTlsSessionState(pTruncated) == XTLS_STATE_FAILED) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_TRUNCATED),
		"TLS unauthenticated EOF did not report truncation");

	xrtTlsSessionDestroy(pTruncated);
	xrtTlsSessionDestroy(pReceiver);
	xrtTlsSessionDestroy(pSender);
	xrtTlsContextRelease(pContext);
}



/* 执行 TLS 会话记录组合层的边界回归。 */
int main(void)
{
	testTlsSessionRecordPlainFragment();
	testTlsSessionRecordProtected13();
	testTlsSessionRecordPlainBackpressure();
	testTlsSessionRecordSendAtomic();
	testTlsSessionRecordTamper();
	testTlsSessionRecordTls12AndCcs();
	testTlsSessionRecordRejectPlainApplication();
	testTlsSessionRecordWritePartial();
	testTlsSessionRecordClose();
	testTlsSessionRecordFailure();
	return 0;
}
