#include "../test.h"

#include <xrt/http_compress.h>



/* 自定义未知长度来源使用独立 Reader 偏移。 */
typedef struct test_http_reply_compress_reader {
	size_t Offset;
} test_http_reply_compress_reader;



/* 静态测试正文由 main 初始化，工厂只借用它。 */
typedef struct test_http_reply_compress_source {
	cbytes Data;
	size_t Size;
} test_http_reply_compress_source;



/* 静态输入 Chunk 没有额外释放责任。 */
static void testHttpReplyCompressChunkRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 关闭一个自定义来源 Reader。 */
static void testHttpReplyCompressReaderClose(ptr pContext)
{
	xrtFree(pContext);
}



/* 按调用方上限发布未知长度来源的下一段数据。 */
static xhttpbodystatus testHttpReplyCompressReaderNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	extern test_http_reply_compress_source
		TestHttpReplyCompressSource;
	test_http_reply_compress_reader* pReader =
		(test_http_reply_compress_reader*)pContext;
	size_t iRemaining;
	size_t iSize;

	if ( pReader->Offset ==
		TestHttpReplyCompressSource.Size ) {
		return XHTTP_BODY_EOF;
	}
	iRemaining = TestHttpReplyCompressSource.Size -
		pReader->Offset;
	iSize = iRemaining < iMaxBytes ?
		iRemaining : iMaxBytes;
	pChunk->Data = TestHttpReplyCompressSource.Data +
		pReader->Offset;
	pChunk->Size = iSize;
	pChunk->Release =
		testHttpReplyCompressChunkRelease;
	pReader->Offset += iSize;
	return XHTTP_BODY_DATA;
}



/* 为每次 Open 创建独立未知长度 Reader。 */
static bool testHttpReplyCompressSourceOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	test_http_reply_compress_reader* pReader;

	(void)pFactory;
	pReader = (test_http_reply_compress_reader*)xrtCalloc(
		1, sizeof(*pReader)
	);
	if ( pReader == NULL ) {
		return false;
	}
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpReplyCompressReaderNext;
	pOps->Close = testHttpReplyCompressReaderClose;
	*ppReader = pReader;
	return true;
}



/* 全局工厂存储只在当前独立测试进程内使用。 */
test_http_reply_compress_source
	TestHttpReplyCompressSource;



/* 读取 Reply 的完整压缩正文。 */
static size_t testHttpReplyCompressRead(
	xhttpbody* pBody,
	uint8* pOutput,
	size_t iCapacity
)
{
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	xhttpbodychunk Chunk;
	size_t iSize = 0;

	testRequire(pReader != NULL,
		"Reply compression roundtrip open failed");
	for ( ;; ) {
		xhttpbodystatus Status = xrtHttpBodyNext(
			pReader, 997, &Chunk
		);

		if ( Status == XHTTP_BODY_EOF ) {
			break;
		}
		testRequire((Status == XHTTP_BODY_DATA) &&
			(Chunk.Size <= (iCapacity - iSize)),
			"Reply compression roundtrip read failed");
		memcpy(pOutput + iSize, Chunk.Data, Chunk.Size);
		iSize += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	xrtHttpBodyReaderDestroy(pReader);
	return iSize;
}



/* 解码压缩 Reply 并与原始正文逐字节比较。 */
static void testHttpReplyCompressDecoded(
	const xhttpreply* pReply,
	xhttpcoding Coding,
	xbytesview Expected
)
{
	uint8 Compressed[131072];
	xinflateconfig Inflate;
	bytes pDecoded;
	size_t iCompressed;
	size_t iDecoded;

	iCompressed = testHttpReplyCompressRead(
		xrtHttpReplyBody(pReply),
		Compressed,
		sizeof(Compressed)
	);
	xrtInflateConfigInit(&Inflate);
	Inflate.Format = Coding == XHTTP_CODING_GZIP ?
		XINFLATE_GZIP : XINFLATE_ZLIB;
	pDecoded = xrtInflateAll(
		(xbytesview){ Compressed, iCompressed },
		&Inflate,
		&iDecoded
	);
	testRequire((pDecoded != NULL) &&
		(iDecoded == Expected.Size) &&
		(memcmp(
			pDecoded, Expected.Data, Expected.Size
		) == 0),
		"Reply compression decoded payload mismatch");
	xrtFree(pDecoded);
}



/* 对固定正文验证 gzip 和 HTTP deflate 的包装格式闭环。 */
static void testHttpReplyCompressFixed(
	xbytesview Input,
	xhttpcoding Coding
)
{
	xhttpacceptencoding Accept;
	xhttpreplycompressconfig Config;
	xhttpreply* pReply =
		xrtHttpReplyCreate(XHTTP_STATUS_OK);
	xhttpreply* pOutput = NULL;

	xrtHttpAcceptEncodingInit(&Accept);
	testRequire(
		xrtHttpAcceptEncodingAdd(
			&Accept, xrtHttpCodingName(Coding)
		) &&
		(pReply != NULL) &&
		xrtHttpReplySetBytes(
			pReply,
			Input,
			XRT_STR_LITERAL("application/json")
		),
		"Reply compression fixed setup failed"
	);
	xrtHttpReplyCompressConfigInit(&Config);
	Config.Codings = (uint32)Coding;
	Config.Preferred = Coding;
	testRequire(
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			&Config,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_APPLIED) &&
		(pOutput != NULL),
		"Reply compression fixed transform failed"
	);
	testHttpReplyCompressDecoded(
		pOutput, Coding, Input
	);
	xrtHttpReplyDestroy(pOutput);
	xrtHttpReplyDestroy(pReply);
}



/* 验证未知长度自定义来源沿流式 BodyDeflate 路径闭环。 */
static void testHttpReplyCompressStream(xbytesview Input)
{
	static const xhttpbodyops Ops = {
		testHttpReplyCompressSourceOpen,
		NULL
	};
	xhttpacceptencoding Accept;
	xhttpreplycompressconfig Config;
	xhttpbody* pBody;
	xhttpreply* pReply =
		xrtHttpReplyCreate(XHTTP_STATUS_OK);
	xhttpreply* pOutput = NULL;

	TestHttpReplyCompressSource.Data = Input.Data;
	TestHttpReplyCompressSource.Size = Input.Size;
	pBody = xrtHttpBodyCreate(
		&Ops,
		&TestHttpReplyCompressSource,
		XHTTP_BODY_UNKNOWN,
		XHTTP_BODY_REPLAYABLE
	);
	xrtHttpAcceptEncodingInit(&Accept);
	testRequire((pBody != NULL) &&
		(pReply != NULL) &&
		xrtHttpAcceptEncodingAdd(
			&Accept, XRT_STR_LITERAL("gzip")
		) &&
		xrtHttpReplySetBody(pReply, pBody) &&
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Content-Type"),
			XRT_STR_LITERAL("text/plain")
		),
		"Reply compression stream setup failed");
	xrtHttpBodyDestroy(pBody);
	xrtHttpReplyCompressConfigInit(&Config);
	Config.Flags =
		XHTTP_REPLY_COMPRESS_ALLOW_UNKNOWN_LENGTH;
	Config.ReadSize = 113;
	testRequire(
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			&Config,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_APPLIED) &&
		(pOutput != NULL) &&
		(xrtHttpBodyLength(
			xrtHttpReplyBody(pOutput)
		 ) == XHTTP_BODY_UNKNOWN),
		"Reply compression stream transform failed"
	);
	testHttpReplyCompressDecoded(
		pOutput, XHTTP_CODING_GZIP, Input
	);
	xrtHttpReplyDestroy(pOutput);
	xrtHttpReplyDestroy(pReply);
}



/* 运行 eager 与未知长度流式响应的压缩解压闭环。 */
int main(void)
{
	static uint8 Input[65536];
	size_t i;

	for ( i = 0; i < sizeof(Input); i++ ) {
		Input[i] =
			(uint8)("future-proof-xrt-http"[i % 21u]);
	}
	testHttpReplyCompressFixed(
		(xbytesview){ Input, sizeof(Input) },
		XHTTP_CODING_GZIP
	);
	testHttpReplyCompressFixed(
		(xbytesview){ Input, sizeof(Input) },
		XHTTP_CODING_DEFLATE
	);
	testHttpReplyCompressStream(
		(xbytesview){ Input, sizeof(Input) }
	);
	printf("[PASS] HTTP Reply compression roundtrip\n");
	return 0;
}
