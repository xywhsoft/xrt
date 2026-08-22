#include "../test.h"
#include "../../src/internal/xrt_tls_session.h"



typedef struct test_tls_session_record_alloc {
	size_t Calls;
	size_t FailAt;
} test_tls_session_record_alloc;



/* 在指定底层分配调用上注入一次 OOM。 */
static ptr testTlsSessionRecordAlloc(ptr pContext, size_t iSize)
{
	test_tls_session_record_alloc* pState =
		(test_tls_session_record_alloc*)pContext;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	return malloc(iSize);
}



/* 让重分配与普通分配共享同一个故障序号。 */
static ptr testTlsSessionRecordRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_tls_session_record_alloc* pState =
		(test_tls_session_record_alloc*)pContext;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放故障注入期间成功取得的底层内存。 */
static void testTlsSessionRecordFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 创建支持最大明文记录但不预分配任何队列块的上下文。 */
static xtlscontext* testTlsSessionRecordOomContext(void)
{
	xtlscontextconfig Config;

	xrtTlsContextConfigInit(&Config);
	Config.Limits.FeedLimit = XTLS_RECORD_HEADER_SIZE +
		XTLS12_RECORD_CIPHERTEXT_MAX;
	Config.Limits.SendLimit = Config.Limits.FeedLimit;
	Config.Limits.PlainLimit = XTLS_RECORD_PLAINTEXT_MAX;
	return xrtTlsContextCreate(&Config);
}



/* 验证发送和解密 scratch OOM 均保持输入、队列及序列号原子。 */
int main(void)
{
	static uint8 Plain[XTLS_RECORD_PLAINTEXT_MAX];
	static const uint8 Key[16] = { 0 };
	static const uint8 Iv[12] = { 0 };
	test_tls_session_record_alloc State = { 0, SIZE_MAX };
	xallocator Allocator;
	xtlscontext* pContext;
	xtlssession* pSender;
	xtlssession* pReceiver;
	xtlssessionrecord Record;
	xnetspan Span;
	size_t iFeed;

	Allocator.Context = &State;
	Allocator.Alloc = testTlsSessionRecordAlloc;
	Allocator.Realloc = testTlsSessionRecordRealloc;
	Allocator.Free = testTlsSessionRecordFree;
	testRequire(xrtSetAllocator(&Allocator),
		"TLS record OOM allocator install failed");
	memset(Plain, 0x6B, sizeof(Plain));
	pContext = testTlsSessionRecordOomContext();
	testRequire(pContext != NULL, "TLS record OOM context failed");
	pSender = __xrtTlsSessionCreate(pContext, NULL, XTLS_CLIENT);
	pReceiver = __xrtTlsSessionCreate(pContext, NULL, XTLS_SERVER);
	xrtTlsContextRelease(pContext);
	testRequire((pSender != NULL) && (pReceiver != NULL),
		"TLS record OOM session creation failed");
	testRequire(__xrtTlsSessionWriteKey(
		pSender, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		(xbytesview) { Key, sizeof(Key) },
		(xbytesview) { Iv, sizeof(Iv) }
	) && __xrtTlsSessionReadKey(
		pReceiver, XTLS_VERSION_13, XTLS_AES_128_GCM_SHA256,
		(xbytesview) { Key, sizeof(Key) },
		(xbytesview) { Iv, sizeof(Iv) }
	), "TLS record OOM key setup failed");

	State.FailAt = State.Calls + 1u;
	xrtClearError();
	testRequire(__xrtTlsSessionRecordProtect(
		pSender, XTLS_RECORD_APPLICATION_DATA,
		(xbytesview) { Plain, sizeof(Plain) }, 0
	) == XTLS_ERROR && (pSender->WriteKey.Sequence == 0) &&
		(xrtTlsSessionSendSize(pSender) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtErrorCause(xrtGetError()) != NULL),
		"TLS send record OOM changed the active epoch");

	State.FailAt = SIZE_MAX;
	xrtClearError();
	testRequire(__xrtTlsSessionRecordProtect(
		pSender, XTLS_RECORD_APPLICATION_DATA,
		(xbytesview) { Plain, sizeof(Plain) }, 0
	) == XTLS_OK && (pSender->WriteKey.Sequence == 1u) &&
		xrtTlsSessionSendFront(pSender, &Span),
		"TLS send record did not recover after OOM");
	testRequire(xrtTlsSessionFeedBorrow(
		pReceiver, Span.Data, Span.Size
	) == XTLS_OK, "TLS scratch OOM borrowed feed failed");
	iFeed = xrtTlsSessionFeedSize(pReceiver);

	State.FailAt = State.Calls + 1u;
	xrtClearError();
	testRequire(__xrtTlsSessionRecordNext(
		pReceiver, &Record
	) == XTLS_ERROR && (pReceiver->ReadKey.Sequence == 0) &&
		(xrtTlsSessionFeedSize(pReceiver) == iFeed) &&
		xrtNetBufEmpty(&pReceiver->Scratch) &&
		!pReceiver->RecordPending &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtErrorCause(xrtGetError()) != NULL),
		"TLS scratch OOM changed the pending record");

	State.FailAt = SIZE_MAX;
	xrtClearError();
	testRequire(__xrtTlsSessionRecordNext(
		pReceiver, &Record
	) == XTLS_OK && Record.Protected &&
		(Record.Type == XTLS_RECORD_APPLICATION_DATA) &&
		(Record.Data.Size == sizeof(Plain)) &&
		(pReceiver->ReadKey.Sequence == 1u),
		"TLS scratch did not recover after OOM");
	testRequire(__xrtTlsSessionRecordFinish(
		pReceiver, true
	) == XTLS_OK && (xrtTlsSessionFeedSize(pReceiver) == 0) &&
		(xrtTlsSessionPlainSize(pReceiver) == sizeof(Plain)),
		"TLS recovered scratch record publication failed");

	testRequire(xrtTlsSessionSendConsume(
		pSender, xrtTlsSessionSendSize(pSender)
	), "TLS record OOM sender consumption failed");
	xrtTlsSessionDestroy(pReceiver);
	xrtTlsSessionDestroy(pSender);
	return 0;
}
