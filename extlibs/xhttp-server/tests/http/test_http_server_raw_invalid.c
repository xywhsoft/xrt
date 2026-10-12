#include "../test.h"



/* 失败提交不得接管调用方的引用。 */
static void testHttpServerRawInvalidRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	uint32* pReleases = (uint32*)pContext;

	(void)pData;
	(void)iSize;
	(*pReleases)++;
}



/* 未知长度夹具不会进入 Reader，只用于验证提交前长度契约。 */
static bool testHttpServerRawUnknownOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	(void)pFactory;
	(void)pOps;
	(void)ppReader;
	return false;
}



/* 验证原始响应的空值、空报文、未知长度和标志边界。 */
int main(void)
{
	static const uint8 Wire[] =
		"HTTP/1.1 204 No Content\r\n\r\n";
	const xhttpbodyops Ops = {
		testHttpServerRawUnknownOpen,
		NULL
	};
	uint32 iReleases = 0;
	xnetref Ref = {
		Wire,
		sizeof(Wire) - 1u,
		testHttpServerRawInvalidRelease,
		&iReleases
	};
	xnetref InvalidRef = {
		Wire,
		sizeof(Wire) - 1u,
		NULL,
		&iReleases
	};
	xhttpbody* pBody = xrtHttpBodyCopy(
		XRT_BYTES_LITERAL("HTTP/1.1 204 No Content\r\n\r\n")
	);
	xhttpbody* pEmpty = xrtHttpBodyEmpty();
	xhttpbody* pUnknown = xrtHttpBodyCreate(
		&Ops,
		NULL,
		XHTTP_BODY_UNKNOWN,
		XHTTP_BODY_REPLAYABLE
	);

	testRequire(
		(pBody != NULL) &&
		(pEmpty != NULL) &&
		(pUnknown != NULL),
		"HTTP server raw invalid fixture failed"
	);
	testRequire(
		(xrtHttpConnRespondRaw(
			NULL,
			XRT_BYTES_LITERAL("HTTP/1.1 204 No Content\r\n\r\n"),
			XHTTP_SERVER_RAW_NONE
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server raw null connection was accepted"
	);
	testRequire(
		(xrtHttpConnRespondRawRef(
			NULL,
			&Ref,
			XHTTP_SERVER_RAW_NONE
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(iReleases == 0),
		"HTTP server raw Ref failure transferred ownership"
	);
	testRequire(
		(xrtHttpConnRespondRawRefs(
			NULL,
			NULL,
			0,
			XHTTP_SERVER_RAW_NONE
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server raw empty Refs were accepted"
	);
	testRequire(
		(xrtHttpConnRespondRawRefs(
			NULL,
			&InvalidRef,
			1,
			XHTTP_SERVER_RAW_NONE
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(iReleases == 0),
		"HTTP server raw Ref without release was accepted"
	);
	testRequire(
		(xrtHttpConnRespondRawTake(
			NULL,
			NULL,
			0,
			XHTTP_SERVER_RAW_NONE
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server raw empty Take was accepted"
	);
	testRequire(
		(xrtHttpConnRespondRaw(
			NULL,
			(xbytesview){ NULL, 0 },
			XHTTP_SERVER_RAW_NONE
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server raw empty bytes were accepted"
	);
	testRequire(
		(xrtHttpConnRespondRawBody(
			NULL,
			NULL,
			XHTTP_SERVER_RAW_NONE
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server raw null Body was accepted"
	);
	testRequire(
		(xrtHttpConnRespondRawBody(
			NULL,
			pEmpty,
			XHTTP_SERVER_RAW_NONE
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server raw empty Body was accepted"
	);
	testRequire(
		(xrtHttpConnRespondRawBody(
			NULL,
			pUnknown,
			XHTTP_SERVER_RAW_NONE
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server raw unknown-length Body was accepted"
	);
	testRequire(
		(xrtHttpConnRespondRawBody(
			NULL,
			pBody,
			UINT32_C(0x80000000)
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server raw unknown flags were accepted"
	);
	xrtHttpBodyDestroy(pUnknown);
	xrtHttpBodyDestroy(pEmpty);
	xrtHttpBodyDestroy(pBody);
	printf("[PASS] HTTP server raw invalid boundaries\n");
	return 0;
}
