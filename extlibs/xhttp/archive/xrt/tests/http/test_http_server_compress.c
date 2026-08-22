#include "../test.h"
#include "../../src/internal/xrt_http_server.h"

#include <xrt/http_compress.h>



/* 从完整请求头创建服务端请求快照。 */
static xhttpserverrequest* testHttpServerCompressRequest(
	char* pInput,
	size_t iSize
)
{
	xhttpfield Fields[16];
	xhttp1bodyplan Plan;
	xhttp1head Head;

	xrtHttp1HeadInit(
		&Head,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	testRequire(
		xrtHttp1RequestParse(
			(xbytesview){ (cbytes)pInput, iSize },
			&Head,
			NULL,
			NULL
		) == XHTTP1_READY,
		"HTTP server compression request parse failed"
	);
	testRequire(
		xrtHttp1RequestBodyPlan(&Head, &Plan),
		"HTTP server compression body plan failed"
	);
	return __xrtHttpServerRequestCreate(
		&Head, &Plan, XHTTP_SERVER_REQUEST_COMPLETE
	);
}



/* 创建可压缩的固定服务端 Reply。 */
static xhttpreply* testHttpServerCompressReply(void)
{
	static uint8 Input[4096];
	xhttpreply* pReply;

	memset(Input, 'r', sizeof(Input));
	pReply = xrtHttpReplyCreate(XHTTP_STATUS_OK);
	testRequire((pReply != NULL) &&
		xrtHttpReplySetBytes(
			pReply,
			(xbytesview){ Input, sizeof(Input) },
			XRT_STR_LITERAL("application/json")
		),
		"HTTP server compression Reply setup failed");
	return pReply;
}



/* 验证重复字段合并、缺失兼容策略和畸形字段失败。 */
int main(void)
{
	uint8 OutputStorage[sizeof(xhttpreply*) + 1u];
	char Repeated[] =
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Accept-Encoding: gzip;q=0.2\r\n"
		"accept-encoding: gzip;q=1\r\n"
		"\r\n";
	char Missing[] =
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"\r\n";
	char Invalid[] =
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Accept-Encoding: gzip;q=1.0000\r\n"
		"\r\n";
	xhttpserverrequest* pRequest;
	xhttpreply* pReply = testHttpServerCompressReply();
	xhttpreply* pOutput = NULL;
	xhttpreply* pPublished = (xhttpreply*)(uintptr_t)1u;
	xhttpreply** ppUnaligned =
		(xhttpreply**)(OutputStorage + 1u);
	xhttpreplycompressstatus Status;

	pOutput = (xhttpreply*)(uintptr_t)1u;
	testRequire(
		(xrtHttpServerReplyCompress(
			NULL, pReply, NULL, &pOutput
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
			"compress-server-reply"
		 ) == 0),
		"HTTP server compression arguments did not clear output"
	);
	xrtClearError();

	pRequest = testHttpServerCompressRequest(
		Repeated, sizeof(Repeated) - 1u
	);
	Status = xrtHttpServerReplyCompress(
		pRequest, pReply, NULL, &pOutput
	);
	if ( Status != XHTTP_REPLY_COMPRESS_APPLIED ) {
		const xerror* pError = xrtGetError();

		fprintf(
			stderr,
			"[INFO] repeated Accept-Encoding status=%d "
			"domain=%s code=%d operation=%s message=%s\n",
			(int)Status,
			pError != NULL ? xrtErrorDomain(pError) : "",
			pError != NULL ? (int)xrtErrorCode(pError) : 0,
			pError != NULL ? xrtErrorOperation(pError) : "",
			pError != NULL ? xrtErrorMessage(pError) : ""
		);
	}
	testRequire((pRequest != NULL) &&
		(Status == XHTTP_REPLY_COMPRESS_APPLIED) &&
		(pOutput != NULL),
		"HTTP server repeated Accept-Encoding mismatch");
	xrtHttpReplyDestroy(pOutput);
	xrtHttpServerRequestDestroy(pRequest);

	pOutput = NULL;
	pRequest = testHttpServerCompressRequest(
		Missing, sizeof(Missing) - 1u
	);
	memcpy(ppUnaligned, &pPublished, sizeof(pPublished));
	Status = xrtHttpServerReplyCompress(
		pRequest, pReply, NULL, ppUnaligned
	);
	memcpy(&pPublished, ppUnaligned, sizeof(pPublished));
	testRequire((pRequest != NULL) &&
		(Status == XHTTP_REPLY_COMPRESS_IDENTITY) &&
		(pPublished != NULL),
		"HTTP server unaligned output mismatch");
	xrtHttpReplyDestroy(pPublished);
	testRequire(
		xrtHttpServerReplyCompress(
			pRequest,
			pReply,
			NULL,
			(xhttpreply**)(UINTPTR_MAX - 1u)
		) == XHTTP_REPLY_COMPRESS_ERROR,
		"HTTP server wrapping output range mismatch"
	);
	xrtClearError();
	xrtHttpServerRequestDestroy(pRequest);

	pOutput = (xhttpreply*)(uintptr_t)1u;
	pRequest = testHttpServerCompressRequest(
		Invalid, sizeof(Invalid) - 1u
	);
	testRequire((pRequest != NULL) &&
		(xrtHttpServerReplyCompress(
			pRequest, pReply, NULL, &pOutput
		 ) == XHTTP_REPLY_COMPRESS_ERROR) &&
		(pOutput == NULL),
		"HTTP server malformed Accept-Encoding mismatch");
	xrtClearError();
	xrtHttpServerRequestDestroy(pRequest);
	xrtHttpReplyDestroy(pReply);
	printf("[PASS] HTTP server Reply compression\n");
	return 0;
}
