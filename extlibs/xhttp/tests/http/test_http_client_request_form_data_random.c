#include "../test.h"



/* 验证随机 boundary 与请求 Content-Type 一致，并且失败不会发布输出。 */
int main(void)
{
	xhttpheadersconfig Headers;
	xhttprequest* pRequest;
	xhttprequest* pLimited;
	xformdata* pForm = xrtFormDataCreate(NULL);
	xmultipartboundary Boundary;
	xmultipartboundary Parsed;
	xmultipartboundary Sentinel;
	xbytesview OldBody;
	xhttpbody* pBefore;
	const xhttpfield* pType;
	uint8 BoundaryStorage[sizeof(xmultipartboundary) + 2u];

	testRequire(
		(pForm != NULL) && xrtFormDataAppendText(
			pForm,
			XRT_STR_LITERAL("name"),
			XRT_STR_LITERAL("value")
		),
		"HTTP request random FormData setup failed"
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/upload")
	);
	memset(BoundaryStorage, 0xA5, sizeof(BoundaryStorage));
	testRequire(
		(pRequest != NULL) && xrtHttpRequestSetFormDataRandom(
			pRequest,
			pForm,
			(xmultipartboundary*)(void*)(BoundaryStorage + 1u)
		),
		"HTTP request unaligned random FormData commit failed"
	);
	memcpy(&Boundary, BoundaryStorage + 1u, sizeof(Boundary));
	testRequire(
		(BoundaryStorage[0] == 0xA5) &&
		(BoundaryStorage[sizeof(BoundaryStorage) - 1u] == 0xA5) &&
		(Boundary.Size == 45u) &&
		(memcmp(Boundary.Data, "----xrt-form-", 13u) == 0),
		"HTTP request random FormData commit failed"
	);
	xrtClearError();
	testRequire(
		!xrtHttpRequestSetFormDataRandom(
			pRequest,
			pForm,
			(xmultipartboundary*)(uintptr_t)(UINTPTR_MAX - 1u)
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP request wrapping random FormData output was accepted"
	);
	xrtClearError();
	pType = xrtHttpRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Content-Type")
	);
	testRequire(
		(pType != NULL) && xrtMultipartBoundaryFromContentType(
			pType->Value,
			&Parsed
		) && (Parsed.Size == Boundary.Size) &&
		(memcmp(
			Parsed.Data,
			Boundary.Data,
			Boundary.Size
		) == 0),
		"HTTP request random FormData boundary mismatch"
	);
	xrtHttpRequestDestroy(pRequest);

	xrtHttpHeadersConfigInit(&Headers);
	Headers.InitialBytes = 0;
	Headers.MaxValue = 8u;
	pLimited = xrtHttpRequestCreateWithHeaders(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/upload"),
		&Headers
	);
	testRequire(
		(pLimited != NULL) && xrtHttpRequestSetBytes(
			pLimited,
			XRT_BYTES_LITERAL("old"),
			XRT_STR_LITERAL("old/type")
		),
		"HTTP request limited FormData setup failed"
	);
	pBefore = xrtHttpRequestBody(pLimited);
	testRequire(
		xrtHttpBodyView(pBefore, &OldBody) &&
		!xrtHttpRequestSetFormDataRandom(
			pLimited,
			pForm,
			(xmultipartboundary*)(void*)OldBody.Data
		) && (xrtHttpRequestBody(pLimited) == pBefore) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP request accepted random FormData output over its old body"
	);
	xrtClearError();
	memset(&Sentinel, 0xA5, sizeof(Sentinel));
	Boundary = Sentinel;
	testRequire(
		!xrtHttpRequestSetFormDataRandom(
			pLimited,
			pForm,
			&Boundary
		) && (xrtHttpRequestBody(pLimited) == pBefore) &&
		(memcmp(
			&Boundary,
			&Sentinel,
			sizeof(Boundary)
		) == 0),
		"failed random FormData commit published partial state"
	);
	xrtClearError();

	xrtHttpRequestDestroy(pLimited);
	xrtFormDataDestroy(pForm);
	printf("[PASS] HTTP client request random FormData\n");
	return 0;
}

