#include "../test.h"



/* 错误处理器重入时记录无分配状态查询是否能够完成。 */
typedef struct testhttpbodystreamhandler {
	xhttpbodystream* Stream;
	size_t Calls;
	bool InfoOk;
	xhttpbodystreaminfo Info;
} testhttpbodystreamhandler;



/* 在错误通知中重入同一个 Stream，验证实现没有持锁调用用户代码。 */
static void testHttpBodyStreamErrorHandler(
	const xerror* pError,
	ptr pUserData
)
{
	testhttpbodystreamhandler* pHandler =
		(testhttpbodystreamhandler*)pUserData;

	(void)pError;
	pHandler->Calls++;
	pHandler->InfoOk = xrtHttpBodyStreamInfo(
		pHandler->Stream,
		&pHandler->Info
	);
}



/* 引用写入释放回调只累计被 Stream 真正接管的租约。 */
static void testHttpBodyStreamRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	size_t* pReleased = (size_t*)pContext;

	(void)pData;
	(void)iSize;
	(*pReleased)++;
}



/* 验证配置和输出句柄支持未对齐存储，并拒绝回绕地址。 */
static void testHttpBodyStreamMemoryContracts(void)
{
	uint8 ConfigStorage[sizeof(xhttpbodystreamconfig) + 2u];
	uint8 StreamStorage[sizeof(xhttpbodystream*) + 2u];
	uint8 InfoStorage[sizeof(xhttpbodystreaminfo) + 2u];
	xhttpbodystreamconfig Config;
	xhttpbodystreaminfo ExpectedInfo;
	xhttpbodystream* pStream;
	xhttpbody* pBody;

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtHttpBodyStreamConfigInit(
		(xhttpbodystreamconfig*)(void*)(ConfigStorage + 1u)
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	testRequire(
		(ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5) &&
		(Config.MaxBytes == XHTTP_BODY_STREAM_BYTES_DEFAULT) &&
		(Config.MaxChunks == XHTTP_BODY_STREAM_CHUNKS_DEFAULT) &&
		xrtHttpBodyStreamConfigValid(
			(const xhttpbodystreamconfig*)(const void*)(
				ConfigStorage + 1u
			)
		), "HTTP body stream config did not support unaligned storage");
	Config.MaxBytes = 8u;
	Config.MaxChunks = 1u;
	memcpy(ConfigStorage + 1u, &Config, sizeof(Config));
	memset(StreamStorage, 0xA5, sizeof(StreamStorage));
	pBody = xrtHttpBodyStreamCreate(
		(const xhttpbodystreamconfig*)(const void*)(ConfigStorage + 1u),
		(xhttpbodystream**)(void*)(StreamStorage + 1u)
	);
	memcpy(&pStream, StreamStorage + 1u, sizeof(pStream));
	testRequire((pBody != NULL) && (pStream != NULL) &&
		(StreamStorage[0] == 0xA5) &&
		(StreamStorage[sizeof(StreamStorage) - 1u] == 0xA5),
		"HTTP body stream create did not support unaligned output"
	);
	memset(InfoStorage, 0xA5, sizeof(InfoStorage));
	memset(&ExpectedInfo, 0, sizeof(ExpectedInfo));
	testRequire(
		xrtHttpBodyStreamInfo(
			pStream,
			(xhttpbodystreaminfo*)(void*)(InfoStorage + 1u)
		) &&
		(InfoStorage[0] == 0xA5) &&
		(InfoStorage[sizeof(InfoStorage) - 1u] == 0xA5) &&
		(memcmp(
			InfoStorage + 1u,
			&ExpectedInfo,
			sizeof(ExpectedInfo)
		) == 0),
		"HTTP body stream Info did not publish an unaligned clean snapshot"
	);
	testRequire(
		!xrtHttpBodyStreamInfo(
			pStream,
			(xhttpbodystreaminfo*)(uintptr_t)(UINTPTR_MAX - 1u)
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP body stream Info accepted wrapping output"
	);
	xrtClearError();
	memset(ConfigStorage + 1u, 0, sizeof(Config));
	testRequire((xrtHttpBodyStreamWrite(
		pStream, XRT_BYTES_LITERAL("snapshot")
	) == XHTTP_BODY_STREAM_OK),
		"HTTP body stream retained caller config storage");
	xrtHttpBodyStreamDestroy(pStream);
	xrtHttpBodyDestroy(pBody);

	xrtHttpBodyStreamConfigInit(
		(xhttpbodystreamconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"HTTP body stream config init accepted wrapping output");
	xrtClearError();
	testRequire(!xrtHttpBodyStreamConfigValid(
		(const xhttpbodystreamconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP body stream config validation accepted wrapping input");
	testRequire((xrtHttpBodyStreamCreate(
		(const xhttpbodystreamconfig*)(uintptr_t)(UINTPTR_MAX - 1u),
		&pStream
	) == NULL) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP body stream create accepted wrapping config");
	xrtClearError();
	testRequire((xrtHttpBodyStreamCreate(
		NULL,
		(xhttpbodystream**)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP body stream create accepted wrapping output");
	xrtClearError();
}



/* 验证分段租约、硬背压、可读/可写 Future 和三种写入所有权。 */
static void testHttpBodyStreamFlow(void)
{
	xhttpbodystreamconfig Config;
	xhttpbodystreaminfo Info;
	xhttpbodystream* pStream = NULL;
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xfuture* pReadable;
	xfuture* pWritable;
	size_t iReleased = 0;
	char* sTaken;

	xrtHttpBodyStreamConfigInit(&Config);
	testRequire(
		(Config.MaxBytes == XHTTP_BODY_STREAM_BYTES_DEFAULT) &&
		(Config.MaxChunks == XHTTP_BODY_STREAM_CHUNKS_DEFAULT) &&
		xrtHttpBodyStreamConfigValid(&Config),
		"HTTP body stream default config mismatch"
	);
	Config.MaxBytes = 6;
	Config.MaxChunks = 2;
	pBody = xrtHttpBodyStreamCreate(&Config, &pStream);
	testRequire(
		(pBody != NULL) && (pStream != NULL) &&
		(xrtHttpBodyLength(pBody) == XHTTP_BODY_UNKNOWN) &&
		!xrtHttpBodyReplayable(pBody),
		"HTTP body stream creation mismatch"
	);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(
		(pReader != NULL) &&
		(xrtHttpBodyNext(pReader, 2, &Chunk) == XHTTP_BODY_AGAIN),
		"HTTP body stream did not begin with AGAIN"
	);
	pReadable = xrtHttpBodyReaderWait(pReader);
	testRequire(
		(pReadable != NULL) &&
		(xrtFutureState(pReadable) == XFUTURE_PENDING),
		"HTTP body stream readable Future was not pending"
	);
	testRequire(
		xrtHttpBodyStreamWrite(
			pStream, XRT_BYTES_LITERAL("abcdef")
		) == XHTTP_BODY_STREAM_OK,
		"HTTP body stream copy write failed"
	);
	testRequire(
		(xrtFutureWait(pReadable) == XWAIT_OK) &&
		(xrtFutureState(pReadable) == XFUTURE_RESOLVED),
		"HTTP body stream readable Future did not resolve"
	);
	xrtFutureDestroy(pReadable);
	testRequire(
		xrtHttpBodyStreamWrite(
			pStream, XRT_BYTES_LITERAL("x")
		) == XHTTP_BODY_STREAM_AGAIN,
		"HTTP body stream byte budget did not apply backpressure"
	);
	pWritable = xrtHttpBodyStreamWaitWritable(pStream);
	testRequire(
		(pWritable != NULL) &&
		(xrtFutureState(pWritable) == XFUTURE_PENDING),
		"HTTP body stream writable Future was not pending"
	);

	testRequire(
		(xrtHttpBodyNext(pReader, 2, &Chunk) == XHTTP_BODY_DATA) &&
		(Chunk.Size == 2) &&
		(memcmp(Chunk.Data, "ab", 2) == 0),
		"HTTP body stream first slice mismatch"
	);
	testRequire(
		xrtHttpBodyNext(pReader, 2, &(xhttpbodychunk){ 0 }) ==
			XHTTP_BODY_AGAIN,
		"HTTP body stream allowed concurrent lease advancement"
	);
	pReadable = xrtHttpBodyReaderWait(pReader);
	testRequire(
		(pReadable != NULL) &&
		(xrtFutureState(pReadable) == XFUTURE_PENDING),
		"HTTP body stream lease wait was not pending"
	);
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(
		(xrtFutureWait(pReadable) == XWAIT_OK) &&
		(xrtFutureState(pReadable) == XFUTURE_RESOLVED) &&
		(xrtFutureState(pWritable) == XFUTURE_PENDING),
		"HTTP body stream partial lease released its byte budget"
	);
	xrtFutureDestroy(pReadable);

	testRequire(
		(xrtHttpBodyNext(pReader, 2, &Chunk) == XHTTP_BODY_DATA) &&
		(memcmp(Chunk.Data, "cd", 2) == 0),
		"HTTP body stream second slice mismatch"
	);
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(
		xrtFutureState(pWritable) == XFUTURE_PENDING,
		"HTTP body stream middle slice released its byte budget"
	);
	testRequire(
		(xrtHttpBodyNext(pReader, 2, &Chunk) == XHTTP_BODY_DATA) &&
		(memcmp(Chunk.Data, "ef", 2) == 0),
		"HTTP body stream final slice mismatch"
	);
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(
		(xrtFutureWait(pWritable) == XWAIT_OK) &&
		(xrtFutureState(pWritable) == XFUTURE_RESOLVED),
		"HTTP body stream final lease did not restore writability"
	);
	xrtFutureDestroy(pWritable);

	testRequire(
		xrtHttpBodyStreamWriteRef(
			pStream,
			XRT_BYTES_LITERAL("gh"),
			testHttpBodyStreamRelease,
			&iReleased
		) == XHTTP_BODY_STREAM_OK,
		"HTTP body stream reference write failed"
	);
	sTaken = (char*)xrtMalloc(2);
	testRequire(sTaken != NULL,
		"HTTP body stream take allocation failed");
	memcpy(sTaken, "ij", 2);
	testRequire(
		xrtHttpBodyStreamWriteTake(
			pStream, sTaken, 2
		) == XHTTP_BODY_STREAM_OK,
		"HTTP body stream take write failed"
	);
	sTaken = NULL;
	testRequire(
		xrtHttpBodyStreamClose(pStream) &&
		xrtHttpBodyStreamClose(pStream),
		"HTTP body stream close was not idempotent"
	);
	testRequire(
		(xrtHttpBodyNext(pReader, 8, &Chunk) == XHTTP_BODY_DATA) &&
		(Chunk.Size == 2) &&
		(memcmp(Chunk.Data, "gh", 2) == 0),
		"HTTP body stream reference data mismatch"
	);
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(
		iReleased == 1,
		"HTTP body stream reference release count mismatch"
	);
	testRequire(
		(xrtHttpBodyNext(pReader, 8, &Chunk) == XHTTP_BODY_DATA) &&
		(Chunk.Size == 2) &&
		(memcmp(Chunk.Data, "ij", 2) == 0),
		"HTTP body stream taken data mismatch"
	);
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(
		xrtHttpBodyNext(pReader, 8, &Chunk) == XHTTP_BODY_EOF,
		"HTTP body stream did not drain before EOF"
	);
	testRequire(
		xrtHttpBodyStreamInfo(pStream, &Info) &&
		(Info.PendingBytes == 0) &&
		(Info.PendingChunks == 0) &&
		(Info.WrittenBytes == 10) &&
		(Info.ReadBytes == 10) &&
		Info.Opened && Info.InputClosed && !Info.Failed,
		"HTTP body stream info mismatch"
	);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyStreamDestroy(pStream);
}



/* 残余空间不足时必须等待预算真实释放，不能立即完成形成忙循环。 */
static void testHttpBodyStreamPartialCapacity(void)
{
	xhttpbodystreamconfig Config = { 6, 2 };
	xhttpbodystream* pStream = NULL;
	xhttpbody* pBody = xrtHttpBodyStreamCreate(
		&Config, &pStream
	);
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	xhttpbodychunk Chunk;
	xfuture* pWritable;

	testRequire(
		(pBody != NULL) && (pStream != NULL) &&
		(pReader != NULL),
		"HTTP body stream partial capacity setup failed"
	);
	testRequire(
		(xrtHttpBodyStreamWrite(
			pStream,
			XRT_BYTES_LITERAL("abcde")
		 ) == XHTTP_BODY_STREAM_OK) &&
		(xrtHttpBodyStreamWrite(
			pStream,
			XRT_BYTES_LITERAL("xy")
		 ) == XHTTP_BODY_STREAM_AGAIN),
		"HTTP body stream partial capacity did not backpressure"
	);
	pWritable = xrtHttpBodyStreamWaitWritable(pStream);
	testRequire(
		(pWritable != NULL) &&
		(xrtFutureState(pWritable) == XFUTURE_PENDING),
		"HTTP body stream partial capacity wait completed early"
	);
	testRequire(
		(xrtHttpBodyNext(pReader, 8, &Chunk) ==
		 XHTTP_BODY_DATA) &&
		(Chunk.Size == 5) &&
		(memcmp(Chunk.Data, "abcde", 5) == 0),
		"HTTP body stream partial capacity read mismatch"
	);
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(
		(xrtFutureWait(pWritable) == XWAIT_OK) &&
		(xrtFutureState(pWritable) == XFUTURE_RESOLVED) &&
		(xrtHttpBodyStreamWrite(
			pStream,
			XRT_BYTES_LITERAL("xy")
		 ) == XHTTP_BODY_STREAM_OK),
		"HTTP body stream budget release did not restore writing"
	);
	xrtFutureDestroy(pWritable);
	testRequire(
		xrtHttpBodyStreamClose(pStream) &&
		(xrtHttpBodyNext(pReader, 8, &Chunk) ==
		 XHTTP_BODY_DATA) &&
		(Chunk.Size == 2) &&
		(memcmp(Chunk.Data, "xy", 2) == 0),
		"HTTP body stream partial capacity retry mismatch"
	);
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(
		xrtHttpBodyNext(pReader, 8, &Chunk) == XHTTP_BODY_EOF,
		"HTTP body stream partial capacity EOF missing"
	);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyStreamDestroy(pStream);
}



/* 验证生产失败丢弃队列、拒绝等待 Future 并保留 Cause。 */
static void testHttpBodyStreamFailure(void)
{
	xhttpbodystreamconfig Config = { 4, 1 };
	testhttpbodystreamhandler Handler = { 0 };
	xhttpbodystream* pStream = NULL;
	xhttpbody* pBody = xrtHttpBodyStreamCreate(
		&Config, &pStream
	);
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	xhttpbodychunk Chunk;
	xfuture* pReadable;
	xfuture* pWritable;
	size_t iReleased = 0;
	xerror* pCause;
	const xerror* pError;

	testRequire((pBody != NULL) && (pReader != NULL),
		"HTTP body stream failure setup failed");
	testRequire(
		xrtHttpBodyStreamWriteRef(
			pStream,
			XRT_BYTES_LITERAL("fail"),
			testHttpBodyStreamRelease,
			&iReleased
		) == XHTTP_BODY_STREAM_OK,
		"HTTP body stream failure queue setup failed"
	);
	testRequire(
		xrtHttpBodyStreamWrite(
			pStream, XRT_BYTES_LITERAL("x")
		) == XHTTP_BODY_STREAM_AGAIN,
		"HTTP body stream failure backpressure setup failed"
	);
	pWritable = xrtHttpBodyStreamWaitWritable(pStream);
	testRequire(pWritable != NULL,
		"HTTP body stream failure writable wait failed");
	pCause = xrtErrorCreate(
		XERR_IO,
		"test.http.body.stream",
		77,
		"producer failed"
	);
	testRequire(
		(pCause != NULL) &&
		xrtHttpBodyStreamFail(pStream, pCause),
		"HTTP body stream fail submission failed"
	);
	xrtErrorFree(pCause);
	testRequire(
		(iReleased == 1) &&
		(xrtFutureWait(pWritable) == XWAIT_OK) &&
		(xrtFutureState(pWritable) == XFUTURE_FAILED),
		"HTTP body stream failure did not reject writability"
	);
	xrtFutureDestroy(pWritable);
	Handler.Stream = pStream;
	xrtClearError();
	xrtSetErrorHandler(testHttpBodyStreamErrorHandler, &Handler);
	testRequire(
		xrtHttpBodyStreamWrite(
			pStream, XRT_BYTES_LITERAL("x")
		) == XHTTP_BODY_STREAM_ERROR,
		"HTTP body stream accepted data after failure"
	);
	xrtSetErrorHandler(NULL, NULL);
	testRequire(
		(Handler.Calls == 1) && Handler.InfoOk && Handler.Info.Failed,
		"HTTP body stream notified failure while holding its mutex"
	);
	testRequire(
		xrtHttpBodyNext(pReader, 8, &Chunk) == XHTTP_BODY_ERROR,
		"HTTP body stream failure did not reach Reader"
	);
	pError = xrtHttpBodyReaderError(pReader);
	testRequire(
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.body.stream"
		) == 0) &&
		(xrtErrorCode(pError) == XHTTP_BODY_STREAM_ERROR_FAILED) &&
		(xrtErrorFind(
			pError,
			"test.http.body.stream",
			77
		) != NULL),
		"HTTP body stream failure Cause mismatch"
	);
	xrtClearError();
	pReadable = xrtHttpBodyStreamWaitWritable(pStream);
	testRequire(
		(pReadable != NULL) &&
		(xrtFutureWait(pReadable) == XWAIT_OK) &&
		(xrtFutureState(pReadable) == XFUTURE_FAILED),
		"HTTP body stream failed wait was not immediately rejected"
	);
	xrtFutureDestroy(pReadable);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyStreamDestroy(pStream);
	xrtClearError();
}



/* 验证公共输出和载荷不能覆盖 Stream 本身或发生地址回绕。 */
static void testHttpBodyStreamAliases(void)
{
	union {
		xhttpbodystreamconfig Config;
		xhttpbodystream* Stream;
	} Alias;
	xhttpbodystreamconfig Expected = { 8, 2 };
	xhttpbodystreaminfo Info;
	xhttpbodystream* pStream = NULL;
	xhttpbody* pBody;
	size_t iReleased = 0;

	Alias.Config = Expected;
	testRequire(
		(xrtHttpBodyStreamCreate(
			&Alias.Config,
			&Alias.Stream
		) == NULL) &&
		(Alias.Config.MaxBytes == Expected.MaxBytes) &&
		(Alias.Config.MaxChunks == Expected.MaxChunks),
		"HTTP body stream create output overwrote aliased config"
	);
	xrtClearError();
	pBody = xrtHttpBodyStreamCreate(&Expected, &pStream);
	testRequire(
		(pBody != NULL) && (pStream != NULL),
		"HTTP body stream alias setup failed"
	);
	testRequire(
		!xrtHttpBodyStreamInfo(
			pStream,
			(xhttpbodystreaminfo*)pStream
		),
		"HTTP body stream info accepted internal output alias"
	);
	xrtClearError();
	testRequire(
		xrtHttpBodyStreamInfo(pStream, &Info) &&
		(Info.PendingBytes == 0) && (Info.PendingChunks == 0),
		"HTTP body stream alias rejection corrupted state"
	);
	testRequire(
		xrtHttpBodyStreamWrite(
			pStream,
			(xbytesview){ (cbytes)pStream, 1 }
		) == XHTTP_BODY_STREAM_ERROR,
		"HTTP body stream copy accepted internal payload alias"
	);
	testRequire(
		xrtHttpBodyStreamWriteRef(
			pStream,
			(xbytesview){ (cbytes)pStream, 1 },
			testHttpBodyStreamRelease,
			&iReleased
		) == XHTTP_BODY_STREAM_ERROR,
		"HTTP body stream reference accepted internal payload alias"
	);
	testRequire(
		(xrtHttpBodyStreamWriteTake(
			pStream,
			(ptr)pStream,
			1
		) == XHTTP_BODY_STREAM_ERROR) &&
		(iReleased == 0),
		"HTTP body stream take accepted internal payload alias"
	);
	testRequire(
		xrtHttpBodyStreamWrite(
			pStream,
			(xbytesview){
				(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
				4
			}
		) == XHTTP_BODY_STREAM_ERROR,
		"HTTP body stream accepted a wrapping payload range"
	);
	xrtClearError();
	testRequire(
		xrtHttpBodyStreamInfo(pStream, &Info),
		"HTTP body stream payload rejection corrupted state"
	);
	xrtHttpBodyStreamDestroy(pStream);
	xrtHttpBodyDestroy(pBody);
	xrtClearError();
}



/* 验证消费者提前关闭和最后生产者释放的对称终态。 */
static void testHttpBodyStreamEndpoints(void)
{
	xhttpbodystream* pStream = NULL;
	xhttpbodystream* pOther;
	xhttpbody* pBody = xrtHttpBodyStreamCreate(
		NULL, &pStream
	);
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	xhttpbodychunk Chunk;
	xfuture* pFuture;

	testRequire((pBody != NULL) && (pReader != NULL),
		"HTTP body stream endpoint setup failed");
	xrtHttpBodyReaderDestroy(pReader);
	testRequire(
		xrtHttpBodyStreamWrite(
			pStream, XRT_BYTES_LITERAL("late")
		) == XHTTP_BODY_STREAM_CLOSED,
		"HTTP body stream accepted data after consumer close"
	);
	pFuture = xrtHttpBodyStreamWaitWritable(pStream);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWait(pFuture) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_CLOSED),
		"HTTP body stream consumer close did not close writable wait"
	);
	xrtFutureDestroy(pFuture);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyStreamDestroy(pStream);

	pBody = xrtHttpBodyStreamCreate(NULL, &pStream);
	pReader = xrtHttpBodyOpen(pBody);
	pOther = xrtHttpBodyStreamRef(pStream);
	testRequire(
		(pBody != NULL) && (pReader != NULL) &&
		(pOther != NULL),
		"HTTP body stream producer reference setup failed"
	);
	xrtHttpBodyStreamDestroy(pStream);
	testRequire(
		xrtHttpBodyStreamWrite(
			pOther, XRT_BYTES_LITERAL("end")
		) == XHTTP_BODY_STREAM_OK,
		"HTTP body stream remaining producer was closed early"
	);
	xrtHttpBodyStreamDestroy(pOther);
	testRequire(
		(xrtHttpBodyNext(pReader, 8, &Chunk) == XHTTP_BODY_DATA) &&
		(Chunk.Size == 3) &&
		(memcmp(Chunk.Data, "end", 3) == 0),
		"HTTP body stream final producer data mismatch"
	);
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(
		xrtHttpBodyNext(pReader, 8, &Chunk) == XHTTP_BODY_EOF,
		"HTTP body stream final producer did not publish EOF"
	);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
}



/* 验证活动 Chunk 在失败后仍独立存活，并在最后释放时回收 Stream。 */
static void testHttpBodyStreamLeaseFailure(void)
{
	xhttpbodystreamconfig Config = { 4, 1 };
	xhttpbodystream* pStream = NULL;
	xhttpbody* pBody = xrtHttpBodyStreamCreate(
		&Config, &pStream
	);
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	xhttpbodychunk Chunk;
	xfuture* pWritable;
	xerror* pCause;
	size_t iReleased = 0;

	testRequire((pBody != NULL) && (pReader != NULL),
		"HTTP body stream lease failure setup failed");
	testRequire(
		xrtHttpBodyStreamWriteRef(
			pStream,
			XRT_BYTES_LITERAL("hold"),
			testHttpBodyStreamRelease,
			&iReleased
		) == XHTTP_BODY_STREAM_OK,
		"HTTP body stream lease failure write failed"
	);
	testRequire(
		(xrtHttpBodyNext(pReader, 2, &Chunk) == XHTTP_BODY_DATA) &&
		(Chunk.Size == 2) &&
		(memcmp(Chunk.Data, "ho", 2) == 0),
		"HTTP body stream lease failure chunk mismatch"
	);
	pWritable = xrtHttpBodyStreamWaitWritable(pStream);
	testRequire(
		(pWritable != NULL) &&
		(xrtFutureState(pWritable) == XFUTURE_PENDING),
		"HTTP body stream lease failure writable setup failed"
	);
	pCause = xrtErrorCreate(
		XERR_IO,
		"test.http.body.stream.lease",
		91,
		"leased stream failed"
	);
	testRequire(
		(pCause != NULL) &&
		xrtHttpBodyStreamFail(pStream, pCause),
		"HTTP body stream active lease failure failed"
	);
	xrtErrorFree(pCause);
	testRequire(
		(iReleased == 0) &&
		(xrtFutureWait(pWritable) == XWAIT_OK) &&
		(xrtFutureState(pWritable) == XFUTURE_FAILED),
		"HTTP body stream active lease was released too early"
	);

	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyStreamDestroy(pStream);
	testRequire(iReleased == 0,
		"HTTP body stream active lease did not outlive endpoints");
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(iReleased == 1,
		"HTTP body stream active lease was not released once");
	xrtFutureDestroy(pWritable);
	xrtClearError();
}



/* 验证消费者提前关闭会丢弃队列、关闭等待并释放外部租约一次。 */
static void testHttpBodyStreamConsumerDiscard(void)
{
	xhttpbodystreamconfig Config = { 4, 1 };
	xhttpbodystream* pStream = NULL;
	xhttpbody* pBody = xrtHttpBodyStreamCreate(
		&Config, &pStream
	);
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	xfuture* pWritable;
	size_t iReleased = 0;

	testRequire((pBody != NULL) && (pReader != NULL),
		"HTTP body stream consumer discard setup failed");
	testRequire(
		xrtHttpBodyStreamWriteRef(
			pStream,
			XRT_BYTES_LITERAL("drop"),
			testHttpBodyStreamRelease,
			&iReleased
		) == XHTTP_BODY_STREAM_OK,
		"HTTP body stream consumer discard write failed"
	);
	pWritable = xrtHttpBodyStreamWaitWritable(pStream);
	testRequire(
		(pWritable != NULL) &&
		(xrtFutureState(pWritable) == XFUTURE_PENDING),
		"HTTP body stream consumer discard wait setup failed"
	);
	xrtHttpBodyReaderDestroy(pReader);
	testRequire(
		(iReleased == 1) &&
		(xrtFutureWait(pWritable) == XWAIT_OK) &&
		(xrtFutureState(pWritable) == XFUTURE_CLOSED),
		"HTTP body stream consumer discard terminal mismatch"
	);
	xrtFutureDestroy(pWritable);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyStreamDestroy(pStream);
}



/* 运行 Body Stream 的完整生命周期和背压回归。 */
int main(void)
{
	xhttpbodystreamconfig Invalid = { 0, 1 };
	xhttpbodystream* pStream = NULL;

	testRequire(
		!xrtHttpBodyStreamConfigValid(NULL) &&
		!xrtHttpBodyStreamConfigValid(&Invalid) &&
		(xrtHttpBodyStreamCreate(&Invalid, &pStream) == NULL) &&
		(pStream == NULL),
		"HTTP body stream accepted invalid config"
	);
	xrtClearError();
	testHttpBodyStreamMemoryContracts();
	testHttpBodyStreamFlow();
	testHttpBodyStreamPartialCapacity();
	testHttpBodyStreamFailure();
	testHttpBodyStreamAliases();
	testHttpBodyStreamEndpoints();
	testHttpBodyStreamLeaseFailure();
	testHttpBodyStreamConsumerDiscard();
	printf("[PASS] HTTP body stream\n");
	return 0;
}

