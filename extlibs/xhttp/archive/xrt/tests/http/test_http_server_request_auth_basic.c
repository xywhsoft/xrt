#include "http_server_request_fixture.h"



/* 验证服务端 Basic 凭据解码及缺失语义。 */
int main(void)
{
	xhttpserverrequest* pRequest =
		testHttpServerRequestFixtureCreate(
			"GET / HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==\r\n"
			"Proxy-Authorization: Basic cHJveHk6c2VjcmV0\r\n"
			"\r\n",
			(xbytesview){ NULL, 0 },
			XHTTP_SERVER_REQUEST_NONE
		);
	char Output[64];
	xhttpbasicauth Basic;
	xhttpbasicauth Empty = { 0 };
	size_t iSize;
	uint8 BasicStorage[sizeof(xhttpbasicauth) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	xhttpbasicauth* pUnalignedBasic =
		(xhttpbasicauth*)(void*)(BasicStorage + 1u);
	size_t* pUnalignedSize =
		(size_t*)(void*)(SizeStorage + 1u);

	testRequire((xrtHttpServerRequestBasicAuth(
		pRequest,
		Output,
		sizeof(Output),
		&iSize,
		&Basic
	) == XHTTP_NEXT_ITEM) &&
		testHttpServerRequestFixtureText(Basic.User, "Aladdin") &&
		testHttpServerRequestFixtureText(Basic.Password, "open sesame") &&
		(xrtHttpServerRequestProxyBasicAuth(
			pRequest,
			Output,
			sizeof(Output),
			&iSize,
			&Basic
		) == XHTTP_NEXT_ITEM) &&
		testHttpServerRequestFixtureText(Basic.User, "proxy"),
		"HTTP server Basic authentication mismatch");
	memset(BasicStorage, 0xA5, sizeof(BasicStorage));
	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	testRequire(
		xrtHttpServerRequestBasicAuth(
			pRequest,
			Output,
			sizeof(Output),
			pUnalignedSize,
			pUnalignedBasic
		) == XHTTP_NEXT_ITEM,
		"HTTP server rejected unaligned Basic outputs"
	);
	memcpy(&iSize, pUnalignedSize, sizeof(iSize));
	memcpy(&Basic, pUnalignedBasic, sizeof(Basic));
	testRequire(
		(iSize == strlen("Aladdin:open sesame")) &&
		testHttpServerRequestFixtureText(
			Basic.User, "Aladdin"
		) && (BasicStorage[0] == UINT8_C(0xA5)) &&
		(BasicStorage[sizeof(BasicStorage) - 1u] ==
		 UINT8_C(0xA5)) &&
		(SizeStorage[0] == UINT8_C(0xA5)) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] ==
		 UINT8_C(0xA5)),
		"HTTP server unaligned Basic output mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);

	/* 无效凭据清空结果，但不读取或改写调用方的旧长度和正文。 */
	pRequest = testHttpServerRequestFixtureCreate(
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Authorization: Basic ???\r\n"
		"\r\n",
		(xbytesview){ NULL, 0 },
		XHTTP_SERVER_REQUEST_NONE
	);
	iSize = 77;
	memset(Output, 0xA5, sizeof(Output));
	memset(&Basic, 0xA5, sizeof(Basic));
	testRequire(
		(xrtHttpServerRequestBasicAuth(
			pRequest,
			Output,
			sizeof(Output),
			&iSize,
			&Basic
		) == XHTTP_NEXT_ERROR) && (iSize == 77) &&
		(memcmp(&Basic, &Empty, sizeof(Basic)) == 0) &&
		(Output[0] == (char)0xA5),
		"HTTP server Basic failure output was not atomic"
	);
	xrtHttpServerRequestDestroy(pRequest);
	puts("[PASS] HTTP server request Basic authentication");
	return 0;
}
