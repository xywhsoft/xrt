#include "http_client_response_fixture.h"



/* 验证唯一 Authentication-Info 的源站、代理和重复字段语义。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Authentication-Info"),
			XRT_STR_INIT(
				"nextnonce=\"next\", qop=auth, "
				"rspauth=\"0123456789abcdef0123456789abcdef"
				"0123456789abcdef0123456789abcdef\", "
				"cnonce=\"c\", nc=00000001"
			)
		},
		{
			XRT_STR_INIT("Proxy-Authentication-Info"),
			XRT_STR_INIT("nextnonce=\"proxy\"")
		}
	};
	static const xhttpfield Duplicate[] = {
		{ XRT_STR_INIT("Authentication-Info"), XRT_STR_INIT("nextnonce=\"a\"") },
		{ XRT_STR_INIT("Authentication-Info"), XRT_STR_INIT("nextnonce=\"b\"") }
	};
	xhttpresponse* pResponse = testHttpResponseFixtureCreate(
		Fields, sizeof(Fields) / sizeof(Fields[0])
	);
	xhttpdigestinfo Info;
	char Output[96];
	size_t iSize;

	testRequire((xrtHttpResponseDigestInfo(
		pResponse, XHTTP_DIGEST_ALGORITHM_SHA256,
		Output, sizeof(Output), &iSize, &Info
	) == XHTTP_NEXT_ITEM) &&
		(Info.Flags == (XHTTP_DIGEST_INFO_HAS_NEXT_NONCE |
		 XHTTP_DIGEST_INFO_HAS_RESPONSE)) &&
		testHttpResponseFixtureText(Info.NextNonce, "next"),
		"HTTP response Digest info mismatch");
	testRequire((xrtHttpResponseProxyDigestInfo(
		pResponse, XHTTP_DIGEST_ALGORITHM_SHA256,
		Output, sizeof(Output), &iSize, &Info
	) == XHTTP_NEXT_ITEM) &&
		testHttpResponseFixtureText(Info.NextNonce, "proxy"),
		"HTTP response proxy Digest info mismatch");

	/* 结构输出允许未对齐，并拒绝覆盖响应拥有区间。 */
	{
		uint8 SizeStorage[sizeof(size_t) + 1u];
		uint8 InfoStorage[sizeof(xhttpdigestinfo) + 1u];
		size_t* pSize = (size_t*)(void*)(SizeStorage + 1u);
		xhttpdigestinfo* pInfo =
			(xhttpdigestinfo*)(void*)(InfoStorage + 1u);

		testRequire(xrtHttpResponseDigestInfo(
			pResponse,
			XHTTP_DIGEST_ALGORITHM_SHA256,
			Output,
			sizeof(Output),
			pSize,
			pInfo
		) == XHTTP_NEXT_ITEM,
			"HTTP response unaligned Digest info failed");
		memcpy(&iSize, pSize, sizeof(iSize));
		memcpy(&Info, pInfo, sizeof(Info));
		testRequire((iSize != 0) &&
			testHttpResponseFixtureText(Info.NextNonce, "next"),
			"HTTP response unaligned Digest info mismatch");
	}
	testRequire(xrtHttpResponseDigestInfo(
		pResponse,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		Output,
		sizeof(Output),
		&iSize,
		(xhttpdigestinfo*)(void*)pResponse
	) == XHTTP_NEXT_ERROR,
		"HTTP response Digest info accepted response object output");
	testHttpResponseFixtureError(
		XERR_ARGUMENT,
		XHTTP_RESPONSE_ERROR_ARGUMENT,
		"HTTP response Digest info response object error mismatch"
	);
	testRequire(xrtHttpResponseDigestInfo(
		pResponse,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		(void*)xrtHttpResponseHeaders(pResponse),
		1u,
		&iSize,
		&Info
	) == XHTTP_NEXT_ERROR,
		"HTTP response Digest info accepted Header container output");
	testHttpResponseFixtureError(
		XERR_ARGUMENT,
		XHTTP_RESPONSE_ERROR_ARGUMENT,
		"HTTP response Digest info Header container error mismatch"
	);
	testRequire(xrtHttpResponseDigestInfo(
		pResponse,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		(void*)(uintptr_t)(UINTPTR_MAX - 1u),
		4u,
		&iSize,
		&Info
	) == XHTTP_NEXT_ERROR,
		"HTTP response Digest info accepted wrapping output");
	testHttpResponseFixtureError(
		XERR_ARGUMENT,
		XHTTP_RESPONSE_ERROR_ARGUMENT,
		"HTTP response Digest info wrapping output error mismatch"
	);
	testRequire(
		xrtHttpResponseStatus(pResponse) == XHTTP_STATUS_OK,
		"HTTP response Digest info alias damaged response"
	);
	xrtHttpResponseDestroy(pResponse);

	pResponse = testHttpResponseFixtureCreate(Duplicate, 2u);
	testRequire(xrtHttpResponseDigestInfo(
		pResponse, XHTTP_DIGEST_ALGORITHM_SHA256,
		Output, sizeof(Output), &iSize, &Info
	) == XHTTP_NEXT_ERROR,
		"HTTP response accepted duplicate Digest info");
	testHttpResponseFixtureError(
		XERR_PROTOCOL,
		XHTTP_RESPONSE_ERROR_HEADER,
		"HTTP response duplicate Digest info error mismatch"
	);
	xrtHttpResponseDestroy(pResponse);
	puts("[PASS] HTTP client response Digest info");
	return 0;
}
