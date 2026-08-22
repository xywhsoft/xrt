#include "../test.h"

#include <xrt/http_compress.h>



/* 创建公开协商状态并合并一个字段值。 */
static xhttpacceptencoding testHttpReplyCompressEdgeAccept(
	xstrview Value
)
{
	xhttpacceptencoding Accept;

	xrtHttpAcceptEncodingInit(&Accept);
	testRequire(
		xrtHttpAcceptEncodingAdd(&Accept, Value),
		"Reply compression edge Accept-Encoding setup failed"
	);
	return Accept;
}



/* 创建指定大小、类型和状态的固定 Reply。 */
static xhttpreply* testHttpReplyCompressEdgeReply(
	size_t iSize,
	xstrview ContentType
)
{
	static uint8 Input[8192];
	xhttpreply* pReply;

	testRequire(iSize <= sizeof(Input),
		"Reply compression edge fixture is too large");
	memset(Input, 'q', iSize);
	pReply = xrtHttpReplyCreate(XHTTP_STATUS_OK);
	testRequire((pReply != NULL) &&
		xrtHttpReplySetBytes(
			pReply,
			(xbytesview){ Input, iSize },
			ContentType
		),
		"Reply compression edge fixture creation failed");
	return pReply;
}



/* 未知长度测试来源打开后直接结束。 */
static xhttpbodystatus testHttpReplyCompressEdgeNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	(void)pContext;
	(void)iMaxBytes;
	(void)pChunk;
	return XHTTP_BODY_EOF;
}



/* 为未知长度来源返回无状态 Reader。 */
static bool testHttpReplyCompressEdgeOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	(void)pFactory;
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpReplyCompressEdgeNext;
	*ppReader = NULL;
	return true;
}



/* 创建已声明未知长度的自定义 Reply。 */
static xhttpreply* testHttpReplyCompressEdgeUnknown(void)
{
	static const xhttpbodyops Ops = {
		testHttpReplyCompressEdgeOpen,
		NULL
	};
	xhttpbody* pBody = xrtHttpBodyCreate(
		&Ops,
		NULL,
		XHTTP_BODY_UNKNOWN,
		XHTTP_BODY_REPLAYABLE
	);
	xhttpreply* pReply =
		xrtHttpReplyCreate(XHTTP_STATUS_OK);

	testRequire((pBody != NULL) &&
		(pReply != NULL) &&
		xrtHttpReplySetBody(pReply, pBody) &&
		xrtHttpReplyAddHeader(
			pReply,
			XRT_STR_LITERAL("Content-Type"),
			XRT_STR_LITERAL("text/plain")
		),
		"Reply compression unknown fixture failed");
	xrtHttpBodyDestroy(pBody);
	return pReply;
}



/* 验证 HEAD 可选择表示、CONNECT 和无正文状态不会变换。 */
static void testHttpReplyCompressEdgeMethods(void)
{
	xhttpacceptencoding Accept =
		testHttpReplyCompressEdgeAccept(
			XRT_STR_LITERAL("gzip")
		);
	xhttpreply* pReply =
		testHttpReplyCompressEdgeReply(
			4096, XRT_STR_LITERAL("text/plain")
		);
	xhttpreply* pOutput = NULL;

	testRequire(
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("HEAD"),
			pReply,
			NULL,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_APPLIED) &&
		(pOutput != NULL),
		"Reply compression HEAD representation mismatch"
	);
	xrtHttpReplyDestroy(pOutput);
	pOutput = NULL;
	testRequire(
		xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("CONNECT"),
			pReply,
			NULL,
			&pOutput
		) == XHTTP_REPLY_COMPRESS_SKIP,
		"Reply compression transformed CONNECT tunnel"
	);
	xrtHttpReplySetStatus(pReply, XHTTP_STATUS_NO_CONTENT);
	testRequire(
		xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		) == XHTTP_REPLY_COMPRESS_SKIP,
		"Reply compression transformed no-content status"
	);
	xrtHttpReplyDestroy(pReply);
}



/* 验证未知长度默认关闭，显式打开后走流式变换。 */
static void testHttpReplyCompressEdgeUnknownLength(void)
{
	xhttpacceptencoding Accept =
		testHttpReplyCompressEdgeAccept(
			XRT_STR_LITERAL("gzip")
		);
	xhttpacceptencoding None =
		testHttpReplyCompressEdgeAccept(
			XRT_STR_LITERAL("identity;q=0, *;q=0")
		);
	xhttpreplycompressconfig Config;
	xhttpreply* pReply =
		testHttpReplyCompressEdgeUnknown();
	xhttpreply* pOutput = NULL;

	testRequire(
		xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		) == XHTTP_REPLY_COMPRESS_SKIP,
		"Reply compression accepted unknown length by default"
	);
	testRequire(
		xrtHttpReplyCompress(
			&None,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		) == XHTTP_REPLY_COMPRESS_NOT_ACCEPTABLE,
		"Reply compression unknown identity exclusion mismatch"
	);
	xrtHttpReplyCompressConfigInit(&Config);
	Config.Flags =
		XHTTP_REPLY_COMPRESS_ALLOW_UNKNOWN_LENGTH;
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
		"Reply compression unknown streaming path mismatch"
	);
	xrtHttpReplyDestroy(pOutput);
	xrtHttpReplyDestroy(pReply);
}



/* 验证 Content-Type 缺失、任意类型和 no-transform 覆盖策略。 */
static void testHttpReplyCompressEdgePolicyFlags(void)
{
	xhttpacceptencoding Accept =
		testHttpReplyCompressEdgeAccept(
			XRT_STR_LITERAL("gzip")
		);
	xhttpreplycompressconfig Config;
	xhttpreply* pReply =
		testHttpReplyCompressEdgeReply(
			4096, (xstrview){ NULL, 0 }
		);
	xhttpreply* pOutput = NULL;

	testRequire(
		xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		) == XHTTP_REPLY_COMPRESS_SKIP,
		"Reply compression accepted absent Content-Type"
	);
	xrtHttpReplyCompressConfigInit(&Config);
	Config.Flags = XHTTP_REPLY_COMPRESS_ALLOW_ANY_TYPE;
	testRequire(
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			&Config,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_APPLIED) &&
		(pOutput != NULL),
		"Reply compression ANY_TYPE mismatch"
	);
	xrtHttpReplyDestroy(pOutput);
	xrtHttpReplyDestroy(pReply);

	pReply = testHttpReplyCompressEdgeReply(
		4096, XRT_STR_LITERAL("text/plain")
	);
	testRequire(xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("Cache-Control"),
		XRT_STR_LITERAL("no-transform")
	), "Reply compression no-transform fixture failed");
	Config.Flags =
		XHTTP_REPLY_COMPRESS_IGNORE_NO_TRANSFORM;
	pOutput = NULL;
	testRequire(
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			&Config,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_APPLIED) &&
		(pOutput != NULL),
		"Reply compression no-transform override mismatch"
	);
	xrtHttpReplyDestroy(pOutput);
	xrtHttpReplyDestroy(pReply);
}



/* 验证 Vary 星号、混合列表、空字段和已有维度的追加规则。 */
static void testHttpReplyCompressEdgeVary(void)
{
	xhttpacceptencoding Accept =
		testHttpReplyCompressEdgeAccept(
			XRT_STR_LITERAL("gzip")
		);
	xhttpreply* pReply =
		testHttpReplyCompressEdgeReply(
			4096, XRT_STR_LITERAL("text/plain")
		);
	xhttpreply* pOutput = NULL;
	size_t iHeaders;

	testRequire(xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("Vary"),
		XRT_STR_LITERAL("*")
	), "Reply compression Vary wildcard setup failed");
	iHeaders = xrtHttpReplyHeaderCount(pReply);
	testRequire(
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_APPLIED) &&
		(pOutput != NULL) &&
		(xrtHttpReplyHeaderCount(pOutput) ==
		 iHeaders + 1u) &&
		(xrtHttpHeadersCountName(
			xrtHttpReplyHeaders(pOutput),
			XRT_STR_LITERAL("Vary")
		 ) == 1),
		"Reply compression Vary wildcard preservation mismatch"
	);
	xrtHttpReplyDestroy(pOutput);
	xrtHttpReplyDestroy(pReply);

	pReply = testHttpReplyCompressEdgeReply(
		4096, XRT_STR_LITERAL("text/plain")
	);
	testRequire(xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("Vary"),
		XRT_STR_LITERAL("*, Accept-Encoding")
	), "Reply compression mixed Vary setup failed");
	iHeaders = xrtHttpReplyHeaderCount(pReply);
	pOutput = NULL;
	testRequire(
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_APPLIED) &&
		(pOutput != NULL) &&
		(xrtHttpReplyHeaderCount(pOutput) ==
		 iHeaders + 1u) &&
		(xrtHttpHeadersCountName(
			xrtHttpReplyHeaders(pOutput),
			XRT_STR_LITERAL("Vary")
		 ) == 1),
		"Reply compression mixed Vary preservation mismatch"
	);
	xrtHttpReplyDestroy(pOutput);
	xrtHttpReplyDestroy(pReply);

	pReply = testHttpReplyCompressEdgeReply(
		4096, XRT_STR_LITERAL("text/plain")
	);
	testRequire(xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("Vary"),
		XRT_STR_LITERAL("")
	), "Reply compression empty Vary setup failed");
	iHeaders = xrtHttpReplyHeaderCount(pReply);
	pOutput = NULL;
	testRequire(
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			NULL,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_APPLIED) &&
		(pOutput != NULL) &&
		(xrtHttpReplyHeaderCount(pOutput) ==
		 iHeaders + 2u) &&
		(xrtHttpHeadersCountName(
			xrtHttpReplyHeaders(pOutput),
			XRT_STR_LITERAL("Vary")
		 ) == 2) &&
		(xrtHttpReplyHeader(
			pOutput,
			XRT_STR_LITERAL("Content-Encoding")
		 ) != NULL),
		"Reply compression empty Vary extension mismatch"
	);
	xrtHttpReplyDestroy(pOutput);
	xrtHttpReplyDestroy(pReply);
}



/* 验证畸形 ETag 被清理且成功路径保留调用前错误。 */
static void testHttpReplyCompressEdgeETag(void)
{
	xhttpacceptencoding Accept =
		testHttpReplyCompressEdgeAccept(
			XRT_STR_LITERAL("gzip")
		);
	xhttpreplycompressconfig Config;
	xhttpreply* pReply =
		testHttpReplyCompressEdgeReply(
			4096, XRT_STR_LITERAL("text/plain")
		);
	xhttpreply* pOutput = NULL;
	xerror* pOld;

	testRequire(xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("ETag"),
		XRT_STR_LITERAL("\"unterminated")
	), "Reply compression malformed ETag setup failed");
	pOld = xrtErrorCreate(
		XERR_VALUE, "test.old", 47, "old error"
	);
	testRequire(pOld != NULL,
		"Reply compression old error setup failed");
	xrtSetError(pOld);
	xrtErrorFree(pOld);
	xrtHttpReplyCompressConfigInit(&Config);
	Config.EagerLimit = 0;
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
			pOutput, XRT_STR_LITERAL("ETag")
		 ) == NULL) &&
		(xrtGetError() != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtGetError()), "test.old"
		) == 0) &&
		(xrtErrorCode(xrtGetError()) == 47),
		"Reply compression malformed ETag error isolation mismatch"
	);
	xrtClearError();
	xrtHttpReplyDestroy(pOutput);
	xrtHttpReplyDestroy(pReply);
}



/* 验证输出限额失败、强制流式和 identity 禁止规则。 */
static void testHttpReplyCompressEdgeOutput(void)
{
	xhttpacceptencoding Accept =
		testHttpReplyCompressEdgeAccept(
			XRT_STR_LITERAL("gzip")
		);
	xhttpacceptencoding NoIdentity =
		testHttpReplyCompressEdgeAccept(
			XRT_STR_LITERAL("gzip, identity;q=0")
		);
	xhttpreplycompressconfig Config;
	xhttpreply* pReply =
		testHttpReplyCompressEdgeReply(
			4096, XRT_STR_LITERAL("text/plain")
		);
	xhttpreply* pOutput = NULL;
	xbytesview View;

	xrtHttpReplyCompressConfigInit(&Config);
	Config.OutputLimit = 1;
	testRequire(
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			&Config,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_ERROR) &&
		(pOutput == NULL) &&
		(xrtHttpReplyHeader(
			pReply,
			XRT_STR_LITERAL("Content-Encoding")
		 ) == NULL),
		"Reply compression output limit atomicity mismatch"
	);
	xrtClearError();

	xrtHttpReplyCompressConfigInit(&Config);
	Config.EagerLimit = 0;
	testRequire(
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			&Config,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_APPLIED) &&
		(pOutput != NULL) &&
		!xrtHttpBodyView(
			xrtHttpReplyBody(pOutput), &View
		) &&
		(View.Data == NULL) &&
		(View.Size == 0),
		"Reply compression forced streaming path mismatch"
	);
	xrtHttpReplyDestroy(pOutput);
	xrtHttpReplyDestroy(pReply);

	pReply = testHttpReplyCompressEdgeReply(
		1, XRT_STR_LITERAL("text/plain")
	);
	xrtHttpReplyCompressConfigInit(&Config);
	Config.MinimumSize = 0;
	pOutput = NULL;
	testRequire(
		(xrtHttpReplyCompress(
			&NoIdentity,
			XRT_STR_LITERAL("GET"),
			pReply,
			&Config,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_APPLIED) &&
		(pOutput != NULL),
		"Reply compression fell back to forbidden identity"
	);
	xrtHttpReplyDestroy(pOutput);
	xrtHttpReplyDestroy(pReply);
}



/* 验证公开配置快照、地址范围和流式队列硬上限。 */
static void testHttpReplyCompressEdgeConfig(void)
{
	static uint8 Storage[sizeof(xhttpreplycompressconfig) + 1u];
	static uint8 OutputStorage[sizeof(xhttpreply*) + 1u];
	xhttpacceptencoding Accept =
		testHttpReplyCompressEdgeAccept(
			XRT_STR_LITERAL("gzip")
		);
	xhttpreplycompressconfig Config;
	xhttpreplycompressconfig* pUnaligned =
		(xhttpreplycompressconfig*)(Storage + 1u);
	xhttpreply* pReply =
		testHttpReplyCompressEdgeReply(
			4096, XRT_STR_LITERAL("text/plain")
		);
	xhttpreply* pOutput = NULL;
	xhttpreply* pPublished = (xhttpreply*)(uintptr_t)1u;
	xhttpreply** ppUnaligned =
		(xhttpreply**)(OutputStorage + 1u);
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xhttpreplycompressstatus Status;

	xrtHttpReplyCompressConfigInit(&Config);
	testRequire(
		Config.QueueLimit == XHTTP_BODY_DEFLATE_QUEUE_DEFAULT,
		"Reply compression default queue limit mismatch"
	);
	Config.EagerLimit = 0;
	memcpy(pUnaligned, &Config, sizeof(Config));
	memcpy(ppUnaligned, &pPublished, sizeof(pPublished));
	Status = xrtHttpReplyCompress(
		&Accept,
		XRT_STR_LITERAL("GET"),
		pReply,
		pUnaligned,
		ppUnaligned
	);
	memcpy(&pPublished, ppUnaligned, sizeof(pPublished));
	testRequire(
		(Status == XHTTP_REPLY_COMPRESS_APPLIED) &&
		(pPublished != NULL),
		"Reply compression unaligned config/output mismatch"
	);
	xrtHttpReplyDestroy(pPublished);

	testRequire(
		xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			pUnaligned,
			(xhttpreply**)(UINTPTR_MAX - 1u)
		) == XHTTP_REPLY_COMPRESS_ERROR,
		"Reply compression wrapping output range mismatch"
	);
	xrtClearError();

	pOutput = (xhttpreply*)(uintptr_t)1u;
	testRequire(
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			(const xhttpreplycompressconfig*)(
				UINTPTR_MAX - 1u
			),
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_ERROR) &&
		(pOutput == NULL),
		"Reply compression wrapping config range mismatch"
	);
	xrtClearError();
	xrtHttpReplyCompressConfigInit(
		(xhttpreplycompressconfig*)(UINTPTR_MAX - 1u)
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"Reply compression wrapping config init mismatch"
	);
	xrtClearError();

	xrtHttpReplyCompressConfigInit(&Config);
	Config.EagerLimit = 0;
	Config.QueueLimit = 1;
	pOutput = NULL;
	testRequire(
		(xrtHttpReplyCompress(
			&Accept,
			XRT_STR_LITERAL("GET"),
			pReply,
			&Config,
			&pOutput
		 ) == XHTTP_REPLY_COMPRESS_APPLIED) &&
		(pOutput != NULL),
		"Reply compression queue-limit setup failed"
	);
	pReader = xrtHttpBodyOpen(xrtHttpReplyBody(pOutput));
	testRequire(pReader != NULL,
		"Reply compression queue-limit reader open failed");
	memset(&Chunk, 0, sizeof(Chunk));
	testRequire(
		xrtHttpBodyNext(
			pReader, SIZE_MAX, &Chunk
		) == XHTTP_BODY_ERROR,
		"Reply compression did not pass the queue hard limit"
	);
	xrtHttpBodyChunkRelease(&Chunk);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpReplyDestroy(pOutput);
	xrtHttpReplyDestroy(pReply);
}



/* 运行方法、策略、元数据和输出边界测试。 */
int main(void)
{
	testHttpReplyCompressEdgeMethods();
	testHttpReplyCompressEdgeUnknownLength();
	testHttpReplyCompressEdgePolicyFlags();
	testHttpReplyCompressEdgeVary();
	testHttpReplyCompressEdgeETag();
	testHttpReplyCompressEdgeOutput();
	testHttpReplyCompressEdgeConfig();
	printf("[PASS] HTTP Reply compression edges\n");
	return 0;
}
