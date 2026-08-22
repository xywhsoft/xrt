#include "http_server_request_fixture.h"



/* 验证 Content-Type 唯一字段、缺失和重复语义。 */
int main(void)
{
	xhttpserverrequest* pRequest =
		testHttpServerRequestFixtureCreate(
			"POST / HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Content-Type: Application/JSON; charset=utf-8\r\n"
			"Content-Length: 0\r\n"
			"\r\n",
			(xbytesview){ NULL, 0 },
			XHTTP_SERVER_REQUEST_NONE
		);
	xmediatype Type;
	uint8 TypeStorage[sizeof(xmediatype) + 2u];
	xmediatype* pUnalignedType =
		(xmediatype*)(void*)(TypeStorage + 1u);

	testRequire(
		(xrtHttpServerRequestContentType(
			pRequest,
			&Type
		 ) == XHTTP_NEXT_ITEM) &&
		xrtHttpMediaTypeEqual(
			&Type,
			XRT_STR_LITERAL("application"),
			XRT_STR_LITERAL("json")
		),
		"HTTP server request Content-Type mismatch"
	);
	memset(TypeStorage, 0xA5, sizeof(TypeStorage));
	testRequire(
		xrtHttpServerRequestContentType(
			pRequest, pUnalignedType
		) == XHTTP_NEXT_ITEM,
		"HTTP server request rejected unaligned Content-Type output"
	);
	memcpy(&Type, pUnalignedType, sizeof(Type));
	testRequire(
		xrtHttpMediaTypeEqual(
			&Type,
			XRT_STR_LITERAL("application"),
			XRT_STR_LITERAL("json")
		) && (TypeStorage[0] == UINT8_C(0xA5)) &&
		(TypeStorage[sizeof(TypeStorage) - 1u] ==
		 UINT8_C(0xA5)),
		"HTTP server request unaligned Content-Type mismatch"
	);
	testRequire(
		xrtHttpServerRequestContentType(
			pRequest,
			(xmediatype*)(void*)pRequest->Fields
		) == XHTTP_NEXT_ERROR,
		"HTTP server Content-Type output overwrote request fields"
	);
	testHttpServerRequestFixtureError(
		XERR_ARGUMENT,
		XHTTP_SERVER_REQUEST_ERROR_ARGUMENT,
		"HTTP server Content-Type overlap error mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);

	pRequest = testHttpServerRequestFixtureCreate(
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"\r\n",
		(xbytesview){ NULL, 0 },
		XHTTP_SERVER_REQUEST_NONE
	);
	testRequire(
		xrtHttpServerRequestContentType(
			pRequest,
			&Type
		) == XHTTP_NEXT_END,
		"HTTP server request missing Content-Type mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);

	pRequest = testHttpServerRequestFixtureCreate(
		"POST / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: 0\r\n"
		"\r\n",
		(xbytesview){ NULL, 0 },
		XHTTP_SERVER_REQUEST_NONE
	);
	testRequire(
		xrtHttpServerRequestContentType(
			pRequest,
			&Type
		) == XHTTP_NEXT_ERROR,
		"HTTP server request accepted duplicate Content-Type"
	);
	testHttpServerRequestFixtureError(
		XERR_PROTOCOL,
		XHTTP_SERVER_REQUEST_ERROR_HEADER,
		"HTTP server request duplicate Content-Type error mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);
	printf("[PASS] HTTP server request Content-Type helper\n");
	return 0;
}

