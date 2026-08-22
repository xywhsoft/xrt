#include "http_server_request_fixture.h"



/* 验证服务端从真实 HTTP/1 请求快照解码源站和代理 Digest 凭据。 */
int main(void)
{
	xhttpserverrequest* pRequest = testHttpServerRequestFixtureCreate(
		"GET /private HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Authorization: Digest username=\"user\", realm=\"api\", "
		"uri=\"/private\", algorithm=SHA-256, nonce=\"n\", "
		"nc=00000001, cnonce=\"c\", qop=auth, "
		"response=\"0123456789abcdef0123456789abcdef"
		"0123456789abcdef0123456789abcdef\"\r\n"
		"Proxy-Authorization: Digest username=\"proxy\", realm=\"api\", "
		"uri=\"/private\", algorithm=SHA-256, nonce=\"n\", "
		"nc=00000001, cnonce=\"c\", qop=auth, "
		"response=\"0123456789abcdef0123456789abcdef"
		"0123456789abcdef0123456789abcdef\"\r\n"
		"\r\n",
		(xbytesview){ NULL, 0 },
		XHTTP_SERVER_REQUEST_NONE
	);
	xhttpdigestauth Digest;
	char Output[128];
	size_t iSize;
	uint8 DigestStorage[sizeof(xhttpdigestauth) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	xhttpdigestauth* pUnalignedDigest =
		(xhttpdigestauth*)(void*)(DigestStorage + 1u);
	size_t* pUnalignedSize =
		(size_t*)(void*)(SizeStorage + 1u);

	testRequire((xrtHttpServerRequestDigestAuth(
		pRequest, Output, sizeof(Output), &iSize, &Digest
	) == XHTTP_NEXT_ITEM) &&
		testHttpServerRequestFixtureText(Digest.Username, "user") &&
		(xrtHttpServerRequestProxyDigestAuth(
			pRequest, Output, sizeof(Output), &iSize, &Digest
		) == XHTTP_NEXT_ITEM) &&
		testHttpServerRequestFixtureText(Digest.Username, "proxy"),
		"HTTP server Digest authentication mismatch");
	memset(DigestStorage, 0xA5, sizeof(DigestStorage));
	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	testRequire(
		xrtHttpServerRequestDigestAuth(
			pRequest,
			Output,
			sizeof(Output),
			pUnalignedSize,
			pUnalignedDigest
		) == XHTTP_NEXT_ITEM,
		"HTTP server rejected unaligned Digest outputs"
	);
	memcpy(&iSize, pUnalignedSize, sizeof(iSize));
	memcpy(&Digest, pUnalignedDigest, sizeof(Digest));
	testRequire(
		(iSize != 0) &&
		testHttpServerRequestFixtureText(
			Digest.Username, "user"
		) && (DigestStorage[0] == UINT8_C(0xA5)) &&
		(DigestStorage[sizeof(DigestStorage) - 1u] ==
		 UINT8_C(0xA5)) &&
		(SizeStorage[0] == UINT8_C(0xA5)) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] ==
		 UINT8_C(0xA5)),
		"HTTP server unaligned Digest output mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);
	puts("[PASS] HTTP server request Digest authentication");
	return 0;
}
