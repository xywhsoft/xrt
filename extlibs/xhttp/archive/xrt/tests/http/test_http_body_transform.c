#include "../test.h"
#include "../../src/internal/xrt_http_body_transform.h"



/* 测试算法配置记录行为、销毁次数和清理错误注入。 */
typedef struct test_http_body_transform_config {
	size_t* Destroys;
	bool InvalidOutput;
	bool DuplicateOutput;
	bool CleanupError;
} test_http_body_transform_config;



/* 每个 Reader 持有一份独立的测试算法状态。 */
typedef struct test_http_body_transform_codec {
	test_http_body_transform_config Config;
} test_http_body_transform_codec;



/* 失败来源保存需要按原对象传播的稳定错误。 */
typedef struct test_http_body_transform_source {
	xerror* Error;
} test_http_body_transform_source;



/* 发布测试错误，并保留调用方持有的错误引用。 */
static void testHttpBodyTransformSetError(
	cstr sDomain,
	int32 iCode,
	cstr sMessage
)
{
	xerror* pError = xrtErrorCreate(
		XERR_IO, sDomain, iCode, sMessage
	);

	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 从可能未对齐的配置副本创建独立算法状态。 */
static ptr testHttpBodyTransformCreate(const void* pConfig)
{
	test_http_body_transform_codec* pCodec =
		(test_http_body_transform_codec*)xrtMalloc(
			sizeof(*pCodec)
		);

	if ( pCodec == NULL ) {
		return NULL;
	}
	memcpy(&pCodec->Config, pConfig, sizeof(pCodec->Config));
	return pCodec;
}



/* 原样转发输入，或故意发布回绕视图验证适配层防御。 */
static bool testHttpBodyTransformWrite(
	ptr pCodecData,
	xbytesview Input,
	bool bFinal,
	xrt_http_body_transform_output_proc pOutput,
	ptr pData
)
{
	test_http_body_transform_codec* pCodec =
		(test_http_body_transform_codec*)pCodecData;

	if ( bFinal || (Input.Size == 0) ) {
		return true;
	}
	if ( pCodec->Config.InvalidOutput ) {
		return pOutput(
			(xbytesview){
				(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
				3
			},
			pData
		);
	}
	if ( pCodec->Config.DuplicateOutput &&
		!pOutput(Input, pData) ) {
		return false;
	}
	return pOutput(Input, pData);
}



/* 销毁算法状态，并可故意发布应被清理边界隔离的错误。 */
static void testHttpBodyTransformDestroy(ptr pCodecData)
{
	test_http_body_transform_codec* pCodec =
		(test_http_body_transform_codec*)pCodecData;
	test_http_body_transform_config Config;

	if ( pCodec == NULL ) {
		return;
	}
	Config = pCodec->Config;
	xrtFree(pCodec);
	if ( Config.Destroys != NULL ) {
		(*Config.Destroys)++;
	}
	if ( Config.CleanupError ) {
		testHttpBodyTransformSetError(
			"test.http.body.transform.cleanup",
			102,
			"cleanup failed"
		);
	}
}



/* 打开时发布来源自己的稳定错误。 */
static bool testHttpBodyTransformSourceOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	test_http_body_transform_source* pSource =
		(test_http_body_transform_source*)pFactory;

	memset(pOps, 0, sizeof(*pOps));
	*ppReader = NULL;
	xrtSetError(pSource->Error);
	return false;
}



/* 创建使用测试算法操作的内部变换正文。 */
static xhttpbody* testHttpBodyTransformBody(
	xhttpbody* pSource,
	const test_http_body_transform_config* pConfig,
	size_t iReadSize,
	size_t iQueueLimit
)
{
	static const xrt_http_body_transform_ops Ops = {
		testHttpBodyTransformCreate,
		testHttpBodyTransformWrite,
		testHttpBodyTransformDestroy
	};

	return __xrtHttpBodyTransformCreate(
		pSource,
		&Ops,
		pConfig,
		sizeof(*pConfig),
		iReadSize,
		iQueueLimit
	);
}



/* 验证通用变换按来源粒度推进、按调用方上限发布并可重放。 */
static void testHttpBodyTransformFlow(void)
{
	static const uint8 Input[] = "transform body";
	test_http_body_transform_config Config = { 0 };
	xhttpbody* pSource = xrtHttpBodyBorrow(
		(xbytesview){ Input, sizeof(Input) - 1u }
	);
	xhttpbody* pBody = testHttpBodyTransformBody(
		pSource, &Config, 5, 0
	);
	uint8 Output[sizeof(Input) - 1u];
	size_t iOutput = 0;
	size_t iPass;

	testRequire((pSource != NULL) && (pBody != NULL) &&
		xrtHttpBodyReplayable(pBody) &&
		(xrtHttpBodyLength(pBody) == XHTTP_BODY_UNKNOWN),
		"HTTP body transform metadata mismatch");
	xrtHttpBodyDestroy(pSource);
	for ( iPass = 0; iPass < 2; iPass++ ) {
		xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
		xhttpbodychunk Chunk;
		xhttpbodystatus Status;

		iOutput = 0;
		testRequire(pReader != NULL,
			"HTTP body transform reader open failed");
		while ( (Status = xrtHttpBodyNext(
			pReader, 3, &Chunk
		)) == XHTTP_BODY_DATA ) {
			testRequire(Chunk.Size <= (sizeof(Output) - iOutput),
				"HTTP body transform output overflow");
			memcpy(Output + iOutput, Chunk.Data, Chunk.Size);
			iOutput += Chunk.Size;
			xrtHttpBodyChunkRelease(&Chunk);
		}
		testRequire((Status == XHTTP_BODY_EOF) &&
			(iOutput == sizeof(Output)) &&
			(memcmp(Output, Input, sizeof(Output)) == 0),
			"HTTP body transform flow mismatch");
		xrtHttpBodyReaderDestroy(pReader);
	}
	xrtHttpBodyDestroy(pBody);
}



/* 验证回绕的配置和算法输出都在解引用前被拒绝。 */
static void testHttpBodyTransformRanges(void)
{
	static const xrt_http_body_transform_ops Ops = {
		testHttpBodyTransformCreate,
		testHttpBodyTransformWrite,
		testHttpBodyTransformDestroy
	};
	test_http_body_transform_config Config = { 0 };
	xhttpbody* pSource = xrtHttpBodyBorrow(
		XRT_BYTES_LITERAL("range")
	);
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	const xerror* pFirst;

	testRequire(pSource != NULL,
		"HTTP body transform range source failed");
	testRequire(__xrtHttpBodyTransformCreate(
		pSource,
		&Ops,
		(const void*)(uintptr_t)(UINTPTR_MAX - 1u),
		3,
		1,
		0
	) == NULL, "HTTP body transform accepted wrapping config");
	xrtClearError();

	Config.InvalidOutput = true;
	pBody = testHttpBodyTransformBody(pSource, &Config, 5, 0);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pBody != NULL) && (pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 8, &Chunk
		) == XHTTP_BODY_ERROR),
		"HTTP body transform accepted wrapping output");
	pFirst = xrtHttpBodyReaderError(pReader);
	testRequire((pFirst != NULL) &&
		(xrtHttpBodyNext(
			pReader, 8, &Chunk
		) == XHTTP_BODY_ERROR) &&
		(xrtHttpBodyReaderError(pReader) == pFirst),
		"HTTP body transform range error was not stable");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtClearError();

	Config.InvalidOutput = false;
	Config.DuplicateOutput = true;
	pBody = testHttpBodyTransformBody(pSource, &Config, 5, 7);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pBody != NULL) && (pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 8, &Chunk
		) == XHTTP_BODY_ERROR) &&
		(xrtErrorKind(
			xrtHttpBodyReaderError(pReader)
		) == XERR_RANGE),
		"HTTP body transform queue limit was ignored");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyDestroy(pSource);
	xrtClearError();
}



/* 验证失败回滚和正常销毁都不能覆盖进入清理前的错误。 */
static void testHttpBodyTransformCleanup(void)
{
	static const xhttpbodyops SourceOps = {
		testHttpBodyTransformSourceOpen,
		NULL
	};
	test_http_body_transform_source SourceState;
	test_http_body_transform_config Config = { 0 };
	xhttpbody* pSource;
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xerror* pSourceError = xrtErrorCreate(
		XERR_IO,
		"test.http.body.transform.source",
		101,
		"source open failed"
	);
	xerror* pOld;
	size_t iDestroys = 0;

	testRequire(pSourceError != NULL,
		"HTTP body transform source error setup failed");
	SourceState.Error = pSourceError;
	Config.Destroys = &iDestroys;
	Config.CleanupError = true;
	pSource = xrtHttpBodyCreate(
		&SourceOps,
		&SourceState,
		XHTTP_BODY_UNKNOWN,
		XHTTP_BODY_REPLAYABLE
	);
	pBody = testHttpBodyTransformBody(pSource, &Config, 4, 0);
	testRequire((pSource != NULL) && (pBody != NULL) &&
		(xrtHttpBodyOpen(pBody) == NULL) &&
		(xrtGetError() == pSourceError) &&
		(iDestroys == 1),
		"HTTP body transform cleanup replaced source error");
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyDestroy(pSource);
	xrtClearError();
	xrtErrorFree(pSourceError);

	iDestroys = 0;
	pSource = xrtHttpBodyBorrow(XRT_BYTES_LITERAL("cleanup"));
	pBody = testHttpBodyTransformBody(pSource, &Config, 4, 0);
	xrtHttpBodyDestroy(pSource);
	pReader = xrtHttpBodyOpen(pBody);
	pOld = xrtErrorCreate(
		XERR_VALUE,
		"test.http.body.transform.old",
		103,
		"old error"
	);
	testRequire((pReader != NULL) && (pOld != NULL),
		"HTTP body transform cleanup setup failed");
	xrtSetError(pOld);
	xrtHttpBodyReaderDestroy(pReader);
	testRequire((xrtGetError() == pOld) && (iDestroys == 1),
		"HTTP body transform close replaced current error");
	xrtHttpBodyDestroy(pBody);
	xrtClearError();
	xrtErrorFree(pOld);
}



/* 执行通用 HTTP 正文变换底座的直接契约测试。 */
int main(void)
{
	testHttpBodyTransformFlow();
	testHttpBodyTransformRanges();
	testHttpBodyTransformCleanup();
	printf("[PASS] HTTP body transform\n");
	return 0;
}
