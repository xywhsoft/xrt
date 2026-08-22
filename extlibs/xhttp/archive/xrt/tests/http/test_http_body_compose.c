#include "../test.h"



/* 测试正文来源记录延迟打开，并发布一段静态数据。 */
typedef struct test_http_body_compose_source {
	size_t Opens;
	size_t Closes;
	size_t Step;
} test_http_body_compose_source;



/* 静态测试 Chunk 不需要回收数据。 */
static void testHttpBodyComposeStaticRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 未知长度来源先发布一个字节，随后正常结束。 */
static xhttpbodystatus testHttpBodyComposeSourceNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	test_http_body_compose_source* pSource =
		(test_http_body_compose_source*)pContext;

	(void)iMaxBytes;
	if ( pSource->Step++ != 0 ) {
		return XHTTP_BODY_EOF;
	}
	pChunk->Data = (cbytes)"x";
	pChunk->Size = 1;
	pChunk->Release = testHttpBodyComposeStaticRelease;
	return XHTTP_BODY_DATA;
}



/* 关闭测试来源时记录活动 Reader 生命周期。 */
static void testHttpBodyComposeSourceClose(ptr pContext)
{
	test_http_body_compose_source* pSource =
		(test_http_body_compose_source*)pContext;

	pSource->Closes++;
}



/* 打开测试来源时返回同一一次性状态。 */
static bool testHttpBodyComposeSourceOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	test_http_body_compose_source* pSource =
		(test_http_body_compose_source*)pFactory;

	pSource->Opens++;
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpBodyComposeSourceNext;
	pOps->Close = testHttpBodyComposeSourceClose;
	*ppReader = pSource;
	return true;
}



/* 把正文完整复制到固定输出并验证不会越过容量。 */
static size_t testHttpBodyComposeReadAll(
	xhttpbody* pBody,
	char* sOutput,
	size_t iCapacity,
	size_t iChunk
)
{
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	size_t iWritten = 0;

	testRequire(pReader != NULL,
		"composed HTTP body open failed");
	for ( ;; ) {
		xhttpbodychunk Chunk;
		xhttpbodystatus Status = xrtHttpBodyNext(
			pReader, iChunk, &Chunk
		);

		if ( Status == XHTTP_BODY_EOF ) {
			break;
		}
		testRequire(Status == XHTTP_BODY_DATA,
			"composed HTTP body read failed");
		testRequire(Chunk.Size <= (iCapacity - iWritten),
			"composed HTTP body exceeded output capacity");
		memcpy(sOutput + iWritten, Chunk.Data, Chunk.Size);
		iWritten += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	xrtHttpBodyReaderDestroy(pReader);
	return iWritten;
}



/* 验证字节复制、子正文引用、顺序、切片、已知长度和重放。 */
static void testHttpBodyComposeSequence(void)
{
	char Prefix[] = "<";
	xhttpbody* pMiddle = xrtHttpBodyBorrow(
		(xbytesview){ (cbytes)"middle", 6 }
	);
	xhttpbodypiece Pieces[4];
	xhttpbody* pBody;
	char Output[16];
	size_t iSize;

	testRequire(pMiddle != NULL,
		"composed HTTP body child create failed");
	Pieces[0] = xrtHttpBodyPieceBytes(
		(xbytesview){ (cbytes)Prefix, 1 }
	);
	Pieces[1] = xrtHttpBodyPieceBytes(
		(xbytesview){ NULL, 0 }
	);
	Pieces[2] = xrtHttpBodyPieceBody(pMiddle);
	Pieces[3] = xrtHttpBodyPieceBytes(
		(xbytesview){ (cbytes)">", 1 }
	);
	pBody = xrtHttpBodyCompose(Pieces, 4);
	testRequire(pBody != NULL,
		"composed HTTP body create failed");
	Prefix[0] = '[';
	xrtHttpBodyDestroy(pMiddle);
	testRequire((xrtHttpBodyLength(pBody) == 8) &&
		xrtHttpBodyReplayable(pBody),
		"composed HTTP body metadata mismatch");
	iSize = testHttpBodyComposeReadAll(
		pBody, Output, sizeof(Output), 2
	);
	testRequire((iSize == 8) &&
		(memcmp(Output, "<middle>", 8) == 0),
		"composed HTTP body sequence mismatch");
	iSize = testHttpBodyComposeReadAll(
		pBody, Output, sizeof(Output), 7
	);
	testRequire((iSize == 8) &&
		(memcmp(Output, "<middle>", 8) == 0),
		"composed HTTP body replay mismatch");
	xrtHttpBodyDestroy(pBody);
}



/* 子正文释放计数用于验证未释放 Chunk 可以延长整个工厂生命周期。 */
static void testHttpBodyComposeChildRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	size_t* pCalls = (size_t*)pContext;

	(void)pData;
	(void)iSize;
	(*pCalls)++;
}



/* 验证字节 Chunk 在 Reader、正文和原子正文销毁后仍然有效。 */
static void testHttpBodyComposeChunkLifetime(void)
{
	size_t iReleases = 0;
	xhttpbody* pChild = xrtHttpBodyReference(
		(xbytesview){ (cbytes)"child", 5 },
		testHttpBodyComposeChildRelease,
		&iReleases
	);
	xhttpbodypiece Pieces[2];
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;

	testRequire(pChild != NULL,
		"composed HTTP body lifetime child failed");
	Pieces[0] = xrtHttpBodyPieceBytes(
		(xbytesview){ (cbytes)"prefix", 6 }
	);
	Pieces[1] = xrtHttpBodyPieceBody(pChild);
	pBody = xrtHttpBodyCompose(Pieces, 2);
	xrtHttpBodyDestroy(pChild);
	testRequire(pBody != NULL,
		"composed HTTP body lifetime create failed");
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 3, &Chunk
		) == XHTTP_BODY_DATA) &&
		(Chunk.Size == 3),
		"composed HTTP body lifetime read failed");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	testRequire((iReleases == 0) &&
		(memcmp(Chunk.Data, "pre", 3) == 0),
		"composed HTTP body Chunk expired early");
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(iReleases == 1,
		"composed HTTP body retained child after final Chunk");
}



/* 验证未知长度、一次性能力和子正文延迟打开。 */
static void testHttpBodyComposeStreaming(void)
{
	static const xhttpbodyops Ops = {
		testHttpBodyComposeSourceOpen,
		NULL
	};
	test_http_body_compose_source Source = { 0 };
	xhttpbody* pChild = xrtHttpBodyCreate(
		&Ops, &Source, XHTTP_BODY_UNKNOWN, XHTTP_BODY_NONE
	);
	xhttpbodypiece Pieces[3];
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;

	testRequire(pChild != NULL,
		"streaming composed child create failed");
	Pieces[0] = xrtHttpBodyPieceBytes(
		(xbytesview){ (cbytes)"a", 1 }
	);
	Pieces[1] = xrtHttpBodyPieceBody(pChild);
	Pieces[2] = xrtHttpBodyPieceBytes(
		(xbytesview){ (cbytes)"b", 1 }
	);
	pBody = xrtHttpBodyCompose(Pieces, 3);
	xrtHttpBodyDestroy(pChild);
	testRequire((pBody != NULL) &&
		(xrtHttpBodyLength(pBody) == XHTTP_BODY_UNKNOWN) &&
		!xrtHttpBodyReplayable(pBody) &&
		(Source.Opens == 0),
		"streaming composed metadata mismatch");
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 1, &Chunk
		) == XHTTP_BODY_DATA) &&
		(Source.Opens == 0),
		"composed child was opened before its turn");
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire((xrtHttpBodyNext(
		pReader, 1, &Chunk
	) == XHTTP_BODY_DATA) &&
		(Source.Opens == 1) &&
		(Chunk.Data[0] == 'x'),
		"streaming composed child read mismatch");
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire((xrtHttpBodyNext(
		pReader, 1, &Chunk
	) == XHTTP_BODY_DATA) &&
		(Chunk.Data[0] == 'b') &&
		(Chunk.Size == 1),
		"streaming composed completion mismatch");
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(xrtHttpBodyNext(
		pReader, 1, &Chunk
	) == XHTTP_BODY_EOF,
		"streaming composed body did not end");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	testRequire((Source.Opens == 1) && (Source.Closes == 1),
		"streaming composed child lifecycle mismatch");
}



/* 验证空组合、单片段和所有非法描述都具有确定行为。 */
static void testHttpBodyComposeEdges(void)
{
	xhttpbodypiece Piece;
	xhttpbody* pBody;
	xhttpbody* pChild;
	char Output[4];

	pBody = xrtHttpBodyCompose(NULL, 0);
	testRequire((pBody != NULL) &&
		(xrtHttpBodyLength(pBody) == 0) &&
		xrtHttpBodyReplayable(pBody),
		"empty composed HTTP body mismatch");
	xrtHttpBodyDestroy(pBody);
	Piece = xrtHttpBodyPieceBytes(
		(xbytesview){ (cbytes)"one", 3 }
	);
	pBody = xrtHttpBodyCompose(&Piece, 1);
	testRequire((pBody != NULL) &&
		(testHttpBodyComposeReadAll(
			pBody, Output, sizeof(Output), 3
		) == 3) &&
		(memcmp(Output, "one", 3) == 0),
		"single composed HTTP body mismatch");
	xrtHttpBodyDestroy(pBody);

	memset(&Piece, 0, sizeof(Piece));
	Piece.Kind = (xhttpbodypiecekind)99;
	testRequire(xrtHttpBodyCompose(&Piece, 1) == NULL,
		"composed HTTP body accepted unknown piece kind");
	xrtClearError();
	Piece = xrtHttpBodyPieceBytes(
		(xbytesview){ NULL, 1 }
	);
	testRequire(xrtHttpBodyCompose(&Piece, 1) == NULL,
		"composed HTTP body accepted invalid bytes");
	xrtClearError();
	Piece = xrtHttpBodyPieceBody(NULL);
	testRequire(xrtHttpBodyCompose(&Piece, 1) == NULL,
		"composed HTTP body accepted null child");
	xrtClearError();
	testRequire(xrtHttpBodyCompose(NULL, 1) == NULL,
		"composed HTTP body accepted null piece array");
	xrtClearError();

	Piece = xrtHttpBodyPieceBytes(
		(xbytesview){
			(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
			4
		}
	);
	testRequire(
		(xrtHttpBodyCompose(&Piece, 1) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"composed HTTP body accepted a wrapping byte range"
	);
	xrtClearError();
	testRequire(
		xrtHttpBodyCompose(
			(const xhttpbodypiece*)(uintptr_t)(
				UINTPTR_MAX - sizeof(xhttpbodypiece) + 2u
			),
			1
		) == NULL,
		"composed HTTP body dereferenced a wrapping piece array"
	);
	xrtClearError();
	testRequire(
		xrtHttpBodyCompose(&Piece, SIZE_MAX) == NULL,
		"composed HTTP body accepted an overflowing piece count"
	);
	xrtClearError();
	Piece = xrtHttpBodyPieceBytes(
		XRT_BYTES_LITERAL("x")
	);
	Piece.Body = (xhttpbody*)(uintptr_t)1;
	testRequire(
		xrtHttpBodyCompose(&Piece, 1) == NULL,
		"composed HTTP body accepted an unused Body field"
	);
	xrtClearError();
	pChild = xrtHttpBodyEmpty();
	testRequire(pChild != NULL,
		"composed HTTP body unused byte field setup failed");
	Piece = xrtHttpBodyPieceBody(pChild);
	Piece.Bytes = XRT_BYTES_LITERAL("x");
	testRequire(
		xrtHttpBodyCompose(&Piece, 1) == NULL,
		"composed HTTP body accepted unused byte fields"
	);
	xrtHttpBodyDestroy(pChild);
	xrtClearError();
}



/* 运行通用 HTTP 正文组合契约测试。 */
int main(void)
{
	testHttpBodyComposeSequence();
	testHttpBodyComposeChunkLifetime();
	testHttpBodyComposeStreaming();
	testHttpBodyComposeEdges();
	printf("[PASS] HTTP body compose\n");
	return 0;
}
