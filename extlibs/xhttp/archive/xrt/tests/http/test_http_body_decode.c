#include "../test.h"

#include "test_http_body_inflate_fixture.h"



/* 完整读取 Body，并把每次返回限制为很小的 Chunk。 */
static size_t testHttpBodyDecodeRead(
	xhttpbody* pBody,
	uint8* pOutput,
	size_t iCapacity
)
{
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	size_t iSize = 0;

	testRequire(pReader != NULL,
		"HTTP Body decode reader open failed");
	while ( (Status = xrtHttpBodyNext(
		pReader, 3, &Chunk
	)) == XHTTP_BODY_DATA ) {
		testRequire(
			(iSize <= iCapacity) &&
			(Chunk.Size <= (iCapacity - iSize)),
			"HTTP Body decode output overflow");
		memcpy(pOutput + iSize, Chunk.Data, Chunk.Size);
		iSize += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	testRequire(Status == XHTTP_BODY_EOF,
		"HTTP Body decode did not reach EOF");
	xrtHttpBodyReaderDestroy(pReader);
	return iSize;
}



/* 验证 gzip、兼容别名与 identity 层按相反顺序组合。 */
static void testHttpBodyDecodeBuiltIn(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("identity")
		},
		{
			XRT_STR_INIT("content-encoding"),
			XRT_STR_INIT("gzip")
		}
	};
	xhttpbody* pSource = xrtHttpBodyBorrow(
		(xbytesview){
			TestHttpBodyInflateGzip,
			sizeof(TestHttpBodyInflateGzip)
		}
	);
	xhttpbody* pBody = NULL;
	uint8 Output[sizeof(TestHttpBodyInflatePlain) - 1u];
	size_t iSize;

	testRequire(
		(pSource != NULL) &&
		(xrtHttpBodyDecodeFields(
			pSource,
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			NULL,
			&pBody
		 ) == XHTTP_BODY_DECODE_APPLIED) &&
		(pBody != NULL),
		"HTTP Body built-in decode setup failed"
	);
	xrtHttpBodyDestroy(pSource);
	iSize = testHttpBodyDecodeRead(
		pBody, Output, sizeof(Output)
	);
	testRequire(
		(iSize == sizeof(Output)) &&
		(memcmp(
			Output,
			TestHttpBodyInflatePlain,
			sizeof(Output)
		) == 0),
		"HTTP Body built-in decode output mismatch"
	);
	xrtHttpBodyDestroy(pBody);

	pSource = xrtHttpBodyBorrow(
		(xbytesview){
			TestHttpBodyInflateGzip,
			sizeof(TestHttpBodyInflateGzip)
		}
	);
	pBody = NULL;
	testRequire(
		xrtHttpBodyDecode(
			pSource,
			XRT_STR_LITERAL("x-gzip"),
			NULL,
			&pBody
		) == XHTTP_BODY_DECODE_APPLIED,
		"HTTP Body x-gzip decode failed"
	);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyDestroy(pSource);
}



/* 验证未知编码完整回退原始 Body，不能先解掉已知外层。 */
static void testHttpBodyDecodeUnsupported(void)
{
	xhttpbody* pSource = xrtHttpBodyBorrow(
		(xbytesview){
			TestHttpBodyInflateGzip,
			sizeof(TestHttpBodyInflateGzip)
		}
	);
	xhttpbody* pBody = NULL;
	uint8 Output[sizeof(TestHttpBodyInflateGzip)];
	size_t iSize;

	testRequire(
		(pSource != NULL) &&
		(xrtHttpBodyDecode(
			pSource,
			XRT_STR_LITERAL("gzip, br"),
			NULL,
			&pBody
		 ) == XHTTP_BODY_DECODE_UNSUPPORTED) &&
		(pBody != NULL),
		"HTTP Body unknown coding did not return raw fallback"
	);
	xrtHttpBodyDestroy(pSource);
	iSize = testHttpBodyDecodeRead(
		pBody, Output, sizeof(Output)
	);
	testRequire(
		(iSize == sizeof(Output)) &&
		(memcmp(
			Output,
			TestHttpBodyInflateGzip,
			sizeof(Output)
		) == 0),
		"HTTP Body unknown coding was partially decoded"
	);
	xrtHttpBodyDestroy(pBody);
}



/* 验证空列表、语法错误和编码层数上限。 */
static void testHttpBodyDecodeEdges(void)
{
	static const xhttpfield Invalid[] = {
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("gzip;q=1")
		}
	};
	xhttpbodydecodeconfig Config;
	xhttpbody* pSource = xrtHttpBodyBorrow(
		(xbytesview){
			TestHttpBodyInflateGzip,
			sizeof(TestHttpBodyInflateGzip)
		}
	);
	xhttpbody* pBody = NULL;

	testRequire(
		(pSource != NULL) &&
		(xrtHttpBodyDecode(
			pSource,
			XRT_STR_LITERAL(""),
			NULL,
			&pBody
		 ) == XHTTP_BODY_DECODE_UNCHANGED) &&
		(pBody != NULL) &&
		(xrtHttpBodyLength(pBody) ==
		 sizeof(TestHttpBodyInflateGzip)),
		"HTTP Body empty coding did not preserve source"
	);
	xrtHttpBodyDestroy(pBody);
	pBody = NULL;
	testRequire(
		xrtHttpBodyDecodeFields(
			pSource, Invalid, 1, NULL, &pBody
		) == XHTTP_BODY_DECODE_ERROR,
		"HTTP Body decode accepted coding parameters"
	);
	testRequire(pBody == NULL,
		"HTTP Body decode published output after syntax error");
	xrtClearError();

	xrtHttpBodyDecodeConfigInit(&Config);
	Config.MaxCodings = 1;
	testRequire(
		xrtHttpBodyDecode(
			pSource,
			XRT_STR_LITERAL("identity, gzip"),
			&Config,
			&pBody
		) == XHTTP_BODY_DECODE_ERROR,
		"HTTP Body decode ignored nesting limit"
	);
	testRequire(
		(pBody == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP Body decode nesting error mismatch"
	);
	xrtClearError();
	xrtHttpBodyDecodeConfigInit(&Config);
	Config.Inflate.ReadSize = 0;
	testRequire(
		xrtHttpBodyDecode(
			pSource,
			XRT_STR_LITERAL(""),
			&Config,
			&pBody
		) == XHTTP_BODY_DECODE_ERROR,
		"HTTP Body decode accepted invalid unused config"
	);
	testRequire(pBody == NULL,
		"HTTP Body decode published output for invalid config");
	xrtClearError();
	xrtHttpBodyDestroy(pSource);
}



/* 验证输出槽不能覆盖配置、字段描述符或借用字段文本。 */
static void testHttpBodyDecodeOverlap(void)
{
	union {
		xhttpbodydecodeconfig Config;
		xhttpbody* Output;
	} ConfigStorage;
	union {
		xhttpfield Field;
		xhttpbody* Output;
	} FieldStorage;
	union {
		char Text[16];
		xhttpbody* Output;
	} TextStorage;
	xhttpfield Field;
	xhttpbody* pSource = xrtHttpBodyBorrow(
		(xbytesview){
			TestHttpBodyInflateGzip,
			sizeof(TestHttpBodyInflateGzip)
		}
	);

	testRequire(pSource != NULL,
		"HTTP Body overlap source create failed");
	testRequire(
		xrtHttpBodyDecode(
			pSource,
			XRT_STR_LITERAL("gzip"),
			NULL,
			(xhttpbody**)pSource
		) == XHTTP_BODY_DECODE_ERROR,
		"HTTP Body decode accepted source/output overlap"
	);
	xrtClearError();
	xrtHttpBodyDecodeConfigInit(&ConfigStorage.Config);
	testRequire(
		xrtHttpBodyDecode(
			pSource,
			XRT_STR_LITERAL("gzip"),
			&ConfigStorage.Config,
			&ConfigStorage.Output
		) == XHTTP_BODY_DECODE_ERROR,
		"HTTP Body decode accepted config/output overlap"
	);
	xrtClearError();

	FieldStorage.Field.Name =
		XRT_STR_LITERAL("Content-Encoding");
	FieldStorage.Field.Value =
		XRT_STR_LITERAL("gzip");
	testRequire(
		xrtHttpBodyDecodeFields(
			pSource,
			&FieldStorage.Field,
			1,
			NULL,
			&FieldStorage.Output
		) == XHTTP_BODY_DECODE_ERROR,
		"HTTP Body decode accepted field/output overlap"
	);
	xrtClearError();

	memcpy(TextStorage.Text, "gzip", 4);
	Field.Name = XRT_STR_LITERAL("Content-Encoding");
	Field.Value = (xstrview){
		TextStorage.Text,
		4
	};
	testRequire(
		xrtHttpBodyDecodeFields(
			pSource,
			&Field,
			1,
			NULL,
			&TextStorage.Output
		) == XHTTP_BODY_DECODE_ERROR,
		"HTTP Body decode accepted value/output overlap"
	);
	xrtClearError();
	xrtHttpBodyDestroy(pSource);
}



/* 验证配置快照、地址回绕和多层组合的队列上限传递。 */
static void testHttpBodyDecodeConfig(void)
{
	static uint8 Storage[sizeof(xhttpbodydecodeconfig) + 1u];
	xhttpbodydecodeconfig Config;
	xhttpbodydecodeconfig* pUnaligned =
		(xhttpbodydecodeconfig*)(Storage + 1u);
	xhttpbody* pSource = xrtHttpBodyBorrow(
		(xbytesview){
			TestHttpBodyInflateGzip,
			sizeof(TestHttpBodyInflateGzip)
		}
	);
	xhttpbody* pBody = NULL;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;

	testRequire(pSource != NULL,
		"HTTP Body decode config source create failed");
	xrtHttpBodyDecodeConfigInit(pUnaligned);
	memcpy(&Config, pUnaligned, sizeof(Config));
	testRequire(
		(Config.MaxCodings == XHTTP_CONTENT_CODINGS_DEFAULT) &&
		(Config.Inflate.QueueLimit ==
		 XHTTP_BODY_INFLATE_QUEUE_DEFAULT),
		"HTTP Body decode unaligned default config mismatch"
	);
	testRequire(
		(xrtHttpBodyDecode(
			pSource,
			XRT_STR_LITERAL("gzip"),
			pUnaligned,
			&pBody
		 ) == XHTTP_BODY_DECODE_APPLIED) &&
		(pBody != NULL),
		"HTTP Body decode unaligned config snapshot mismatch"
	);
	xrtHttpBodyDestroy(pBody);

	pBody = (xhttpbody*)(uintptr_t)1u;
	testRequire(
		(xrtHttpBodyDecode(
			pSource,
			XRT_STR_LITERAL("gzip"),
			(const xhttpbodydecodeconfig*)(
				UINTPTR_MAX - 1u
			),
			&pBody
		 ) == XHTTP_BODY_DECODE_ERROR) &&
		(pBody == NULL),
		"HTTP Body decode wrapping config range mismatch"
	);
	xrtClearError();
	xrtHttpBodyDecodeConfigInit(
		(xhttpbodydecodeconfig*)(UINTPTR_MAX - 1u)
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"HTTP Body decode wrapping config init mismatch"
	);
	xrtClearError();

	xrtHttpBodyDecodeConfigInit(&Config);
	Config.Inflate.QueueLimit = 1;
	pBody = NULL;
	testRequire(
		(xrtHttpBodyDecode(
			pSource,
			XRT_STR_LITERAL("gzip"),
			&Config,
			&pBody
		 ) == XHTTP_BODY_DECODE_APPLIED) &&
		(pBody != NULL),
		"HTTP Body decode queue-limit setup failed"
	);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(pReader != NULL,
		"HTTP Body decode queue-limit reader open failed");
	memset(&Chunk, 0, sizeof(Chunk));
	testRequire(
		xrtHttpBodyNext(
			pReader, SIZE_MAX, &Chunk
		) == XHTTP_BODY_ERROR,
		"HTTP Body decode did not pass the queue hard limit"
	);
	xrtHttpBodyChunkRelease(&Chunk);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyDestroy(pSource);
}



/* 执行通用 Content-Encoding Body 解码测试。 */
int main(void)
{
	testHttpBodyDecodeBuiltIn();
	testHttpBodyDecodeUnsupported();
	testHttpBodyDecodeEdges();
	testHttpBodyDecodeOverlap();
	testHttpBodyDecodeConfig();
	printf("[PASS] HTTP Body content decode\n");
	return 0;
}
