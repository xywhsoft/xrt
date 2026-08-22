#include "../test.h"

#include "../../src/internal/xrt_http_body_stream.h"



/* OOM 通知期间记录对同一个 Stream 的无分配状态查询。 */
typedef struct testhttpbodystreamoomhandler {
	xhttpbodystream* Stream;
	size_t Calls;
	bool InfoOk;
} testhttpbodystreamoomhandler;



/* 在分配错误处理器中重入 Stream，检测锁内分配造成的死锁。 */
static void testHttpBodyStreamOomHandler(
	const xerror* pError,
	ptr pUserData
)
{
	testhttpbodystreamoomhandler* pHandler =
		(testhttpbodystreamoomhandler*)pUserData;
	xhttpbodystreaminfo Info;

	(void)pError;
	pHandler->Calls++;
	pHandler->InfoOk = xrtHttpBodyStreamInfo(
		pHandler->Stream,
		&Info
	);
}



/* 构建器回调用于验证单分配编码路径确实执行并写入完整输出。 */
static bool testHttpBodyStreamFill(
	void* pOutput,
	size_t iSize,
	ptr pData
)
{
	memcpy(pOutput, pData, iSize);
	return true;
}



/* 主动拒绝输出，用于验证已预留预算和临时节点都会回滚。 */
static bool testHttpBodyStreamFillFail(
	void* pOutput,
	size_t iSize,
	ptr pData
)
{
	(void)pOutput;
	(void)iSize;
	(void)pData;
	return false;
}



/* 引用写入释放回调记录 Stream 是否真正接管了调用方租约。 */
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



/* 扫描创建过程中的 Stream 与 Body 两级分配并要求输出保持原子。 */
static size_t testHttpBodyStreamCreateOom(void)
{
	size_t iFail;

	for ( iFail = 0; iFail < 16u; iFail++ ) {
		xhttpbodystream* pStream = (xhttpbodystream*)(uintptr_t)1;
		xhttpbody* pBody;
		bool bTriggered;

		testRequire(
			xrtMemDebugFailAfter((uint64)iFail),
			"HTTP body stream create OOM setup failed"
		);
		pBody = xrtHttpBodyStreamCreate(NULL, &pStream);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( pBody == NULL ) {
			testRequire(
				bTriggered && (pStream == NULL) &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
				"HTTP body stream create OOM published partial state"
			);
		} else {
			testRequire(
				!bTriggered && (pStream != NULL),
				"HTTP body stream create ignored an allocation fault"
			);
			xrtHttpBodyStreamDestroy(pStream);
			xrtHttpBodyDestroy(pBody);
			xrtClearError();
			testMemoryDebugDrain(
				"HTTP body stream create OOM leaked storage"
			);
			return iFail;
		}
		xrtClearError();
		testMemoryDebugDrain(
			"HTTP body stream create OOM leaked storage"
		);
	}
	testRequire(false,
		"HTTP body stream create OOM scan did not converge");
	return 0;
}



/* 验证复制与内部构建分配失败不会消耗队列预算或累计流量。 */
static void testHttpBodyStreamWriteOom(void)
{
	xhttpbodystreamconfig Config = { 16, 2 };
	xhttpbodystream* pStream = NULL;
	xhttpbody* pBody = xrtHttpBodyStreamCreate(&Config, &pStream);
	xhttpbodystreaminfo Info;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;

	testRequire((pBody != NULL) && (pStream != NULL),
		"HTTP body stream write OOM setup failed");
	testRequire(xrtMemDebugFailAfter(0),
		"HTTP body stream write OOM fault setup failed");
	testRequire(
		(xrtHttpBodyStreamWrite(
			pStream, XRT_BYTES_LITERAL("copy")
		) == XHTTP_BODY_STREAM_ERROR) &&
		xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP body stream copy write did not preserve OOM"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	testRequire(
		xrtHttpBodyStreamInfo(pStream, &Info) &&
		(Info.PendingBytes == 0) &&
		(Info.PendingChunks == 0) &&
		(Info.WrittenBytes == 0),
		"HTTP body stream copy OOM did not roll back budget"
	);

	testRequire(xrtMemDebugFailAfter(0),
		"HTTP body stream build OOM fault setup failed");
	testRequire(
		(__xrtHttpBodyStreamBuild(
			pStream,
			4,
			testHttpBodyStreamFill,
			(ptr)"build"
		) == XHTTP_BODY_STREAM_ERROR) &&
		xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP body stream internal build did not preserve OOM"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	testRequire(
		xrtHttpBodyStreamInfo(pStream, &Info) &&
		(Info.PendingBytes == 0) &&
		(Info.PendingChunks == 0) &&
		(Info.WrittenBytes == 0),
		"HTTP body stream build OOM did not roll back budget"
	);
	testRequire(
		__xrtHttpBodyStreamBuild(
			pStream,
			4,
			testHttpBodyStreamFillFail,
			NULL
		) == XHTTP_BODY_STREAM_ERROR,
		"HTTP body stream accepted a failed fill callback"
	);
	xrtClearError();
	testRequire(
		xrtHttpBodyStreamInfo(pStream, &Info) &&
		(Info.PendingBytes == 0) &&
		(Info.PendingChunks == 0) &&
		(Info.WrittenBytes == 0),
		"HTTP body stream failed fill did not roll back budget"
	);

	testRequire(
		__xrtHttpBodyStreamBuild(
			pStream,
			5,
			testHttpBodyStreamFill,
			(ptr)"build"
		) == XHTTP_BODY_STREAM_OK,
		"HTTP body stream did not recover after write OOM"
	);
	testRequire(xrtHttpBodyStreamClose(pStream),
		"HTTP body stream write OOM close failed");
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(
		(pReader != NULL) &&
		(xrtHttpBodyNext(pReader, 8, &Chunk) == XHTTP_BODY_DATA) &&
		(Chunk.Size == 5) &&
		(memcmp(Chunk.Data, "build", 5) == 0),
		"HTTP body stream write OOM recovery data mismatch"
	);
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(
		xrtHttpBodyNext(pReader, 8, &Chunk) == XHTTP_BODY_EOF,
		"HTTP body stream write OOM recovery EOF mismatch"
	);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyStreamDestroy(pStream);
	xrtHttpBodyDestroy(pBody);
	xrtClearError();
	testMemoryDebugDrain(
		"HTTP body stream write OOM leaked storage"
	);
}



/* 验证引用节点分配失败时外部租约仍归调用方，恢复后只释放一次。 */
static void testHttpBodyStreamRefOom(void)
{
	xhttpbodystream* pStream = NULL;
	xhttpbody* pBody = xrtHttpBodyStreamCreate(NULL, &pStream);
	size_t iReleased = 0;

	testRequire((pBody != NULL) && (pStream != NULL),
		"HTTP body stream reference OOM setup failed");
	testRequire(xrtMemDebugFailAfter(0),
		"HTTP body stream reference OOM fault setup failed");
	testRequire(
		(xrtHttpBodyStreamWriteRef(
			pStream,
			XRT_BYTES_LITERAL("reference"),
			testHttpBodyStreamRelease,
			&iReleased
		) == XHTTP_BODY_STREAM_ERROR) &&
		xrtMemDebugFailTriggered() &&
		(iReleased == 0),
		"HTTP body stream reference OOM consumed caller ownership"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	testRequire(
		xrtHttpBodyStreamWriteRef(
			pStream,
			XRT_BYTES_LITERAL("reference"),
			testHttpBodyStreamRelease,
			&iReleased
		) == XHTTP_BODY_STREAM_OK,
		"HTTP body stream reference write did not recover"
	);
	xrtHttpBodyDestroy(pBody);
	testRequire(iReleased == 1,
		"HTTP body stream reference ownership was not released once");
	xrtHttpBodyStreamDestroy(pStream);
	testMemoryDebugDrain(
		"HTTP body stream reference OOM leaked storage"
	);
}



/* 验证可写等待可重试，而通用 Reader 按契约保存可读等待失败。 */
static void testHttpBodyStreamWaitOom(void)
{
	testhttpbodystreamoomhandler Handler = { 0 };
	xhttpbodystream* pStream = NULL;
	xhttpbody* pBody = xrtHttpBodyStreamCreate(NULL, &pStream);
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	xhttpbodychunk Chunk;
	xfuture* pFuture;

	testRequire((pBody != NULL) && (pReader != NULL),
		"HTTP body stream wait OOM setup failed");
	testRequire(
		xrtHttpBodyNext(pReader, 8, &Chunk) == XHTTP_BODY_AGAIN,
		"HTTP body stream wait OOM did not begin pending"
	);

	testRequire(xrtMemDebugFailAfter(0),
		"HTTP body stream writable OOM fault setup failed");
	Handler.Stream = pStream;
	xrtSetErrorHandler(testHttpBodyStreamOomHandler, &Handler);
	pFuture = xrtHttpBodyStreamWaitWritable(pStream);
	xrtSetErrorHandler(NULL, NULL);
	testRequire(
		(pFuture == NULL) &&
		xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(Handler.Calls != 0) && Handler.InfoOk,
		"HTTP body stream writable Future ignored OOM"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	pFuture = xrtHttpBodyStreamWaitWritable(pStream);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWait(pFuture) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED),
		"HTTP body stream writable Future did not recover"
	);
	xrtFutureDestroy(pFuture);

	testRequire(xrtMemDebugFailAfter(0),
		"HTTP body stream readable OOM fault setup failed");
	testRequire(
		(xrtHttpBodyReaderWait(pReader) == NULL) &&
		xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtHttpBodyReaderError(pReader) != NULL) &&
		(xrtErrorKind(
			xrtHttpBodyReaderError(pReader)
		) == XERR_MEMORY),
		"HTTP body stream readable Future ignored OOM"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	testRequire(
		(xrtHttpBodyReaderWait(pReader) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP body stream Reader did not preserve wait failure"
	);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyStreamDestroy(pStream);
	xrtHttpBodyDestroy(pBody);
	xrtClearError();
	testMemoryDebugDrain(
		"HTTP body stream wait OOM leaked storage"
	);
}



/* 已失败的 Stream 重复失败必须复用终态错误且完全不分配。 */
static void testHttpBodyStreamRepeatedFailOom(void)
{
	xhttpbodystream* pStream = NULL;
	xhttpbody* pBody = xrtHttpBodyStreamCreate(NULL, &pStream);
	xerror* pCause = xrtErrorCreate(
		XERR_IO,
		"test.http.body.stream.oom",
		91,
		"producer failed"
	);

	testRequire(
		(pBody != NULL) && (pStream != NULL) && (pCause != NULL),
		"HTTP body stream repeated failure setup failed"
	);
	testRequire(
		xrtHttpBodyStreamFail(pStream, pCause),
		"HTTP body stream initial failure failed"
	);
	xrtClearError();
	testRequire(
		xrtMemDebugFailAfter(0),
		"HTTP body stream repeated failure OOM setup failed"
	);
	testRequire(
		!xrtHttpBodyStreamFail(pStream, pCause) &&
		!xrtMemDebugFailTriggered() &&
		(xrtGetError() != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtGetError()),
			"xrt.http.body.stream"
		) == 0),
		"HTTP body stream repeated failure allocated or lost terminal error"
	);
	xrtMemDebugFailClear();
	xrtErrorFree(pCause);
	xrtHttpBodyStreamDestroy(pStream);
	xrtHttpBodyDestroy(pBody);
	xrtClearError();
	testMemoryDebugDrain(
		"HTTP body stream repeated failure leaked storage"
	);
}



/* 运行 Body Stream 创建、写入、所有权和等待代际的分配故障回归。 */
int main(void)
{
	size_t iCreateFaults = testHttpBodyStreamCreateOom();

	testRequire(iCreateFaults != 0,
		"HTTP body stream create path had no allocations");
	testHttpBodyStreamWriteOom();
	testHttpBodyStreamRefOom();
	testHttpBodyStreamWaitOom();
	testHttpBodyStreamRepeatedFailOom();
	printf(
		"[PASS] HTTP body stream OOM create_faults=%u\n",
		(unsigned)iCreateFaults
	);
	return 0;
}
