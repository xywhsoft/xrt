#include "../test.h"

#include <xrt/http_compress.h>



/* 初始化一个已经解析的 Accept-Encoding 字段值。 */
static xhttpacceptencoding testHttpReplyCompressAccept(
	xstrview Value
)
{
	xhttpacceptencoding Accept;

	xrtHttpAcceptEncodingInit(&Accept);
	testRequire(
		xrtHttpAcceptEncodingAdd(&Accept, Value),
		"Reply compression Accept-Encoding setup failed"
	);
	return Accept;
}



/* 创建带完整固定正文和 Content-Type 的 Reply。 */
static xhttpreply* testHttpReplyCompressReply(
	xbytesview Data,
	xstrview ContentType
)
{
	xhttpreply* pReply =
		xrtHttpReplyCreate(XHTTP_STATUS_OK);

	testRequire((pReply != NULL) &&
		xrtHttpReplySetBytes(pReply, Data, ContentType),
		"Reply compression fixture creation failed");
	return pReply;
}



/* 判断 Reply 的全部同名字段是否包含指定 token。 */
static bool testHttpReplyCompressHeaderToken(
	const xhttpreply* pReply,
	xstrview Name,
	xstrview Token
)
{
	size_t i;

	for ( i = 0; i < xrtHttpReplyHeaderCount(pReply); i++ ) {
		const xhttpfield* pField =
			xrtHttpReplyHeaderAt(pReply, i);

		if ( (pField != NULL) &&
			xrtHttpFieldNameEqual(pField->Name, Name) &&
			xrtHttpTokenListHas(pField->Value, Token) ) {
			return true;
		}
	}
	return false;
}



/* 验证 eager gzip、输入不变和全部失效元数据清理。 */
static void testHttpReplyCompressApplied(void)
{
	static uint8 Input[8192];
	xhttpacceptencoding Accept =
		testHttpReplyCompressAccept(
			XRT_STR_LITERAL("gzip")
		);
	xhttpreply* pReply;
	xhttpreply* pOutput = NULL;
	xbytesview Original;
	xbytesview Compressed;
	size_t i;

	for ( i = 0; i < sizeof(Input); i++ ) {
		Input[i] = (uint8)("xrt-http-compress"[i % 17u]);
	}
	pReply = testHttpReplyCompressReply(
		(xbytesview){ Input, sizeof(Input) },
		XRT_STR_LITERAL("application/json; charset=utf-8")
	);
	testRequire(
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Vary"),
			XRT_STR_LITERAL("User-Agent")
		) &&
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Content-Length"),
			XRT_STR_LITERAL("8192")
		) &&
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Transfer-Encoding"),
			XRT_STR_LITERAL("chunked")
		) &&
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Content-Digest"),
			XRT_STR_LITERAL("sha-256=:AAAA:")
		) &&
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Repr-Digest"),
			XRT_STR_LITERAL("sha-256=:BBBB:")
		) &&
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Accept-Ranges"),
			XRT_STR_LITERAL("bytes")
		) &&
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Trailer"),
			XRT_STR_LITERAL("Content-Digest, X-Trace")
		) &&
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("ETag"),
			XRT_STR_LITERAL("W/\"same\"")
		) &&
		xrtHttpReplyAddTrailer(
			pReply,
			XRT_STR_LITERAL("Content-Digest"),
			XRT_STR_LITERAL("sha-256=:CCCC:")
		) &&
		xrtHttpReplyAddTrailer(
			pReply,
			XRT_STR_LITERAL("X-Trace"),
			XRT_STR_LITERAL("done")
		),
		"Reply compression metadata setup failed"
	);
	testRequire(
		xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		) == XHTTP_REPLY_COMPRESS_APPLIED,
		"Reply gzip compression was not applied"
	);
	testRequire((pOutput != NULL) &&
		(pOutput != pReply) &&
		xrtHttpBodyView(
			xrtHttpReplyBody(pReply), &Original
		) &&
		(Original.Size == sizeof(Input)) &&
		(memcmp(Original.Data, Input, sizeof(Input)) == 0) &&
		xrtHttpBodyView(
			xrtHttpReplyBody(pOutput), &Compressed
		) &&
		(Compressed.Size < Original.Size) &&
		(Compressed.Size >= 10u) &&
		(Compressed.Data[0] == UINT8_C(0x1F)) &&
		(Compressed.Data[1] == UINT8_C(0x8B)),
		"Reply gzip body or source atomicity mismatch");
	testRequire(
		(xrtHttpReplyHeader(
			pReply,
			XRT_STR_LITERAL("Content-Encoding")
		 ) == NULL) &&
		(xrtHttpReplyHeader(
			pOutput,
			XRT_STR_LITERAL("Content-Encoding")
		 ) != NULL) &&
		testHttpReplyCompressHeaderToken(
			pOutput,
			XRT_STR_LITERAL("Vary"),
			XRT_STR_LITERAL("Accept-Encoding")
		) &&
		testHttpReplyCompressHeaderToken(
			pOutput,
			XRT_STR_LITERAL("Vary"),
			XRT_STR_LITERAL("User-Agent")
		) &&
		(xrtHttpReplyHeader(
			pOutput,
			XRT_STR_LITERAL("Content-Length")
		 ) == NULL) &&
		(xrtHttpReplyHeader(
			pOutput,
			XRT_STR_LITERAL("Transfer-Encoding")
		 ) == NULL) &&
		(xrtHttpReplyHeader(
			pOutput,
			XRT_STR_LITERAL("Content-Digest")
		 ) == NULL) &&
		(xrtHttpReplyHeader(
			pOutput,
			XRT_STR_LITERAL("Repr-Digest")
		 ) == NULL) &&
		(xrtHttpReplyHeader(
			pOutput,
			XRT_STR_LITERAL("Accept-Ranges")
		 ) == NULL) &&
		(xrtHttpReplyHeader(
			pOutput,
			XRT_STR_LITERAL("Trailer")
		 ) == NULL) &&
		(xrtHttpReplyHeader(
			pOutput,
			XRT_STR_LITERAL("ETag")
		 ) != NULL) &&
		(xrtHttpReplyTrailer(
			pOutput,
			XRT_STR_LITERAL("Content-Digest")
		 ) == NULL) &&
		(xrtHttpReplyTrailer(
			pOutput,
			XRT_STR_LITERAL("X-Trace")
		 ) != NULL),
		"Reply compression stale metadata cleanup mismatch"
	);
	xrtHttpReplyDestroy(pOutput);
	xrtHttpReplyDestroy(pReply);
}



/* 验证小正文回退、强 ETag 清除和强制保留较大编码。 */
static void testHttpReplyCompressEagerPolicy(void)
{
	xhttpacceptencoding Accept =
		testHttpReplyCompressAccept(
			XRT_STR_LITERAL("gzip")
		);
	xhttpreplycompressconfig Config;
	xhttpreply* pReply = testHttpReplyCompressReply(
		(xbytesview){ (cbytes)"x", 1 },
		XRT_STR_LITERAL("text/plain")
	);
	xhttpreply* pOutput = NULL;

	xrtHttpReplyCompressConfigInit(&Config);
	Config.MinimumSize = 0;
	testRequire(
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("ETag"),
			XRT_STR_LITERAL("\"strong\"")
		) &&
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			&Config,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_IDENTITY) &&
		(pOutput != NULL) &&
		(xrtHttpReplyHeader(
			pOutput,
			XRT_STR_LITERAL("Content-Encoding")
		 ) == NULL) &&
		(xrtHttpReplyHeader(
			pOutput,
			XRT_STR_LITERAL("ETag")
		 ) != NULL) &&
		testHttpReplyCompressHeaderToken(
			pOutput,
			XRT_STR_LITERAL("Vary"),
			XRT_STR_LITERAL("Accept-Encoding")
		),
		"Reply compression larger identity fallback mismatch"
	);
	xrtHttpReplyDestroy(pOutput);
	pOutput = NULL;

	Config.Flags |= XHTTP_REPLY_COMPRESS_KEEP_LARGER;
	testRequire(
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			&Config,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_APPLIED) &&
		(pOutput != NULL) &&
		(xrtHttpReplyHeader(
			pOutput,
			XRT_STR_LITERAL("ETag")
		 ) == NULL),
		"Reply compression KEEP_LARGER or strong ETag mismatch"
	);
	xrtHttpReplyDestroy(pOutput);
	xrtHttpReplyDestroy(pReply);
}



/* 验证 Header 缺失兼容策略、显式禁止和 MIME 策略。 */
static void testHttpReplyCompressNegotiation(void)
{
	static uint8 Input[4096];
	xhttpacceptencoding Missing;
	xhttpacceptencoding None =
		testHttpReplyCompressAccept(
			XRT_STR_LITERAL("identity;q=0, *;q=0")
		);
	xhttpreplycompressconfig Config;
	xhttpreply* pReply;
	xhttpreply* pOutput = NULL;

	memset(Input, 'a', sizeof(Input));
	xrtHttpAcceptEncodingInit(&Missing);
	pReply = testHttpReplyCompressReply(
		(xbytesview){ Input, sizeof(Input) },
		XRT_STR_LITERAL("text/plain")
	);
	testRequire(
		(xrtHttpReplyCompress(
			&Missing,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_IDENTITY) &&
		(pOutput != NULL),
		"Reply compression absent compatibility mismatch"
	);
	xrtHttpReplyDestroy(pOutput);
	pOutput = NULL;

	xrtHttpReplyCompressConfigInit(&Config);
	Config.Flags = XHTTP_REPLY_COMPRESS_ALLOW_ABSENT;
	testRequire(
		(xrtHttpReplyCompress(
			&Missing,
			XRT_STR_LITERAL("GET"),
			pReply,
			&Config,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_APPLIED) &&
		(pOutput != NULL),
		"Reply compression absent opt-in mismatch"
	);
	xrtHttpReplyDestroy(pOutput);
	pOutput = (xhttpreply*)(uintptr_t)1u;
	testRequire(
		(xrtHttpReplyCompress(
			&None,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_NOT_ACCEPTABLE) &&
		(pOutput == NULL),
		"Reply compression unacceptable coding mismatch"
	);
	xrtHttpReplyDestroy(pReply);

	pReply = testHttpReplyCompressReply(
		(xbytesview){ Input, sizeof(Input) },
		XRT_STR_LITERAL("image/png")
	);
	pOutput = (xhttpreply*)(uintptr_t)1u;
	testRequire(
		(xrtHttpReplyCompress(
			&Missing,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_SKIP) &&
		(pOutput == NULL) &&
		(xrtHttpReplyCompress(
			&None,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_NOT_ACCEPTABLE),
		"Reply compression MIME policy mismatch"
	);
	xrtHttpReplyDestroy(pReply);
}



/* 验证 no-transform、大小、状态、范围和已有编码的跳过规则。 */
static void testHttpReplyCompressSkip(void)
{
	static uint8 Input[4096];
	xhttpacceptencoding Accept =
		testHttpReplyCompressAccept(
			XRT_STR_LITERAL("gzip")
		);
	xhttpreply* pReply;
	xhttpreply* pOutput = NULL;

	memset(Input, 's', sizeof(Input));
	pReply = testHttpReplyCompressReply(
		(xbytesview){ Input, sizeof(Input) },
		XRT_STR_LITERAL("text/plain")
	);
	testRequire(
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Cache-Control"),
			XRT_STR_LITERAL("public, no-transform")
		) &&
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_SKIP),
		"Reply compression ignored no-transform"
	);
	xrtHttpReplyRemoveHeader(
		pReply, XRT_STR_LITERAL("Cache-Control")
	);
	xrtHttpReplySetStatus(pReply, XHTTP_STATUS_PARTIAL_CONTENT);
	testRequire(
		xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		) == XHTTP_REPLY_COMPRESS_SKIP,
		"Reply compression transformed partial response"
	);
	xrtHttpReplySetStatus(pReply, XHTTP_STATUS_OK);
	testRequire(
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Content-Range"),
			XRT_STR_LITERAL("bytes 0-3/4")
		) &&
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_SKIP),
		"Reply compression transformed ranged representation"
	);
	xrtHttpReplyRemoveHeader(
		pReply, XRT_STR_LITERAL("Content-Range")
	);
	testRequire(
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Content-Encoding"),
			XRT_STR_LITERAL("br")
		) &&
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_SKIP),
		"Reply compression transformed encoded representation"
	);
	xrtHttpReplyDestroy(pReply);
}



/* 验证畸形相关字段和配置都以空输出失败。 */
static void testHttpReplyCompressErrors(void)
{
	static uint8 Input[4096];
	xhttpacceptencoding Accept =
		testHttpReplyCompressAccept(
			XRT_STR_LITERAL("gzip")
		);
	xhttpreplycompressconfig Config;
	xhttpreply* pReply;
	xhttpreply* pOutput;

	memset(Input, 'e', sizeof(Input));
	pReply = testHttpReplyCompressReply(
		(xbytesview){ Input, sizeof(Input) },
		XRT_STR_LITERAL("text/plain")
	);
	pOutput = (xhttpreply*)(uintptr_t)1u;
	testRequire(
		(xrtHttpReplyCompress(
			NULL,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_ERROR) &&
		(pOutput == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 (int32)XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT) &&
		(strcmp(
			xrtErrorDomain(xrtGetError()),
			"http.reply.compress"
		 ) == 0) &&
		(strcmp(
			xrtErrorOperation(xrtGetError()),
			"compress"
		 ) == 0),
		"Reply compression invalid arguments did not clear output"
	);
	xrtClearError();
	testRequire(xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("Vary"),
		XRT_STR_LITERAL("Accept-Encoding;bad")
	), "Reply compression malformed Vary setup failed");
	pOutput = (xhttpreply*)(uintptr_t)1u;
	testRequire(
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_ERROR) &&
		(pOutput == NULL) &&
		(xrtErrorDomain(xrtGetError()) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtGetError()),
			"http.reply.compress"
		) == 0),
		"Reply compression malformed Vary error mismatch"
	);
	xrtClearError();
	xrtHttpReplyDestroy(pReply);

	pReply = testHttpReplyCompressReply(
		(xbytesview){ Input, sizeof(Input) },
		XRT_STR_LITERAL("text/plain")
	);
	testRequire(xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("Cache-Control"),
		XRT_STR_LITERAL("public, no-transform=1")
	), "Reply compression invalid Cache-Control setup failed");
	pOutput = (xhttpreply*)(uintptr_t)1u;
	testRequire(
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_ERROR) &&
		(pOutput == NULL) &&
		(xrtErrorDomain(xrtGetError()) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtGetError()),
			"http.reply.compress"
		) == 0),
		"Reply compression invalid Cache-Control mismatch"
	);
	xrtClearError();
	xrtHttpReplyDestroy(pReply);

	pReply = testHttpReplyCompressReply(
		(xbytesview){ Input, sizeof(Input) },
		XRT_STR_LITERAL("text/plain")
	);
	testRequire(xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("Cache-Control"),
		XRT_STR_LITERAL("public, no-transform=\"unterminated")
	), "Reply compression malformed Cache-Control setup failed");
	pOutput = (xhttpreply*)(uintptr_t)1u;
	testRequire(
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_ERROR) &&
		(pOutput == NULL),
		"Reply compression malformed Cache-Control mismatch"
	);
	xrtClearError();
	xrtHttpReplyDestroy(pReply);

	pReply = testHttpReplyCompressReply(
		(xbytesview){ Input, sizeof(Input) },
		XRT_STR_LITERAL("text/plain")
	);
	testRequire(xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("application/json")
	), "Reply compression duplicate Content-Type setup failed");
	pOutput = (xhttpreply*)(uintptr_t)1u;
	testRequire(
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_ERROR) &&
		(pOutput == NULL),
		"Reply compression duplicate Content-Type mismatch"
	);
	xrtClearError();
	xrtHttpReplyDestroy(pReply);

	xrtHttpReplyCompressConfigInit(&Config);
	Config.MinimumSize = 2;
	Config.MaximumSize = 1;
	pReply = testHttpReplyCompressReply(
		(xbytesview){ Input, sizeof(Input) },
		XRT_STR_LITERAL("text/plain")
	);
	pOutput = (xhttpreply*)(uintptr_t)1u;
	testRequire(
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			&Config,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_ERROR) &&
		(pOutput == NULL),
		"Reply compression invalid config mismatch"
	);
	xrtClearError();
	xrtHttpReplyDestroy(pReply);
}



/* 运行 Reply 自动压缩的协商、元数据、策略和错误契约测试。 */
int main(void)
{
	testHttpReplyCompressApplied();
	testHttpReplyCompressEagerPolicy();
	testHttpReplyCompressNegotiation();
	testHttpReplyCompressSkip();
	testHttpReplyCompressErrors();
	printf("[PASS] HTTP Reply compression\n");
	return 0;
}
