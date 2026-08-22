#include "http_client_response_fixture.h"



/* 验证唯一、缺失、重复和非法 Content-Type 响应语义。 */
int main(void)
{
	static const xhttpfield Valid[] = {
		{
			XRT_STR_INIT("Content-Type"),
			XRT_STR_INIT("Application/JSON; charset=utf-8")
		}
	};
	static const xhttpfield Duplicate[] = {
		{
			XRT_STR_INIT("Content-Type"),
			XRT_STR_INIT("text/plain")
		},
		{
			XRT_STR_INIT("content-type"),
			XRT_STR_INIT("application/json")
		}
	};
	static const xhttpfield Invalid[] = {
		{
			XRT_STR_INIT("Content-Type"),
			XRT_STR_INIT("not-a-media-type")
		}
	};
	xhttpresponse* pResponse;
	const xhttpfield* pField;
	xmediatype Type;
	uint8 Storage[sizeof(xmediatype) + 2u];

	pResponse = testHttpResponseFixtureCreate(Valid, 1);
	memset(Storage, 0xA5, sizeof(Storage));
	testRequire(
		(xrtHttpResponseContentType(
			pResponse,
			(xmediatype*)(void*)(Storage + 1u)
		 ) == XHTTP_NEXT_ITEM),
		"HTTP response unaligned Content-Type output failed"
	);
	memcpy(&Type, Storage + 1u, sizeof(Type));
	testRequire(
		(Storage[0] == 0xA5) &&
		(Storage[sizeof(Storage) - 1u] == 0xA5) &&
		xrtHttpMediaTypeEqual(
			&Type,
			XRT_STR_LITERAL("application"),
			XRT_STR_LITERAL("json")
		),
		"HTTP response Content-Type mismatch"
	);
	pField = xrtHttpResponseHeader(
		pResponse,
		XRT_STR_LITERAL("Content-Type")
	);
	testRequire(
		(pField != NULL) &&
		(xrtHttpResponseContentType(
			pResponse,
			(xmediatype*)(void*)pField->Value.Data
		 ) == XHTTP_NEXT_ERROR),
		"HTTP response accepted Content-Type output over a field"
	);
	testHttpResponseFixtureError(
		XERR_ARGUMENT,
		XHTTP_RESPONSE_ERROR_ARGUMENT,
		"HTTP response overlapping Content-Type output error mismatch"
	);
	testRequire(
		xrtHttpResponseContentType(
			pResponse,
			(xmediatype*)(uintptr_t)(UINTPTR_MAX - 1u)
		) == XHTTP_NEXT_ERROR,
		"HTTP response accepted wrapping Content-Type output"
	);
	testHttpResponseFixtureError(
		XERR_ARGUMENT,
		XHTTP_RESPONSE_ERROR_ARGUMENT,
		"HTTP response wrapping Content-Type output error mismatch"
	);
	xrtHttpResponseDestroy(pResponse);

	pResponse = testHttpResponseFixtureCreate(NULL, 0);
	testRequire(
		xrtHttpResponseContentType(
			pResponse,
			&Type
		) == XHTTP_NEXT_END,
		"HTTP response missing Content-Type mismatch"
	);
	testRequire(
		(Type.Type.Data == NULL) &&
		(Type.Type.Size == 0) &&
		(Type.Subtype.Data == NULL) &&
		(Type.Subtype.Size == 0),
		"HTTP response missing Content-Type left stale output"
	);
	xrtHttpResponseDestroy(pResponse);

	pResponse = testHttpResponseFixtureCreate(Duplicate, 2);
	testRequire(
		xrtHttpResponseContentType(
			pResponse,
			&Type
		) == XHTTP_NEXT_ERROR,
		"HTTP response accepted duplicate Content-Type"
	);
	testHttpResponseFixtureError(
		XERR_PROTOCOL,
		XHTTP_RESPONSE_ERROR_HEADER,
		"HTTP response duplicate Content-Type error mismatch"
	);
	xrtHttpResponseDestroy(pResponse);

	pResponse = testHttpResponseFixtureCreate(Invalid, 1);
	testRequire(
		xrtHttpResponseContentType(
			pResponse,
			&Type
		) == XHTTP_NEXT_ERROR,
		"HTTP response accepted invalid Content-Type"
	);
	testHttpResponseFixtureError(
		XERR_VALUE,
		XHTTP_RESPONSE_ERROR_CONTENT_TYPE,
		"HTTP response invalid Content-Type error mismatch"
	);
	xrtHttpResponseDestroy(pResponse);
	printf("[PASS] HTTP client response Content-Type helper\n");
	return 0;
}
