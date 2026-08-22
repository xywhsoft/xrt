#include "../test.h"

#include <xrt/http_auth.h>



/* 比较 MD5 Digest 的固定长度文本结果。 */
static bool testHttpDigestMd5Equal(
	const char* sDigest,
	size_t iSize,
	cstr sExpected
)
{
	return (iSize == 32u) &&
		(memcmp(sDigest, sExpected, 32u) == 0);
}



/* 验证历史 RFC MD5 向量和可选后端分派。 */
static void testHttpDigestMd5Vector(void)
{
	char Secret[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	char Digest[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;
	xhttpdigestproof Proof = {
		XHTTP_DIGEST_ALGORITHM_MD5,
		XHTTP_DIGEST_QOP_AUTH,
		1u,
		{ Secret, 32u },
		XRT_STR_INIT("dcd98b7102dd2f0e8b11d0f600bfb0c093"),
		XRT_STR_INIT("0a4f113b"),
		XRT_STR_INIT("/dir/index.html"),
		{ NULL, 0 }
	};

	testRequire(xrtHttpDigestAlgorithmSupported(
		XHTTP_DIGEST_ALGORITHM_MD5
	), "HTTP Digest MD5 backend was not reported");
	testRequire(xrtHttpDigestSecret(
		XHTTP_DIGEST_ALGORITHM_MD5,
		XRT_STR_LITERAL("Mufasa"),
		XRT_STR_LITERAL("testrealm@host.com"),
		XRT_STR_LITERAL("Circle Of Life"),
		Secret, sizeof(Secret), &iSize
	) && testHttpDigestMd5Equal(
		Secret, iSize, "939e7578ed9e3c518a452acee763bce9"
	), "HTTP Digest MD5 secret mismatch");
	testRequire(xrtHttpDigestRequest(
		&Proof, XRT_STR_LITERAL("GET"),
		Digest, sizeof(Digest), &iSize
	) && testHttpDigestMd5Equal(
		Digest, iSize, "6629fae49393a05397450978507c4ef1"
	), "HTTP Digest MD5 request vector mismatch");
	testRequire(xrtHttpDigestRspAuth(
		&Proof, Digest, sizeof(Digest), &iSize
	) && testHttpDigestMd5Equal(
		Digest, iSize, "376602cfd2f4e8e5e78b948a85263e85"
	), "HTTP Digest MD5 rspauth mismatch");
}



/* 验证 MD5-sess 复用基础 secret 且不会引入第二套 API。 */
static void testHttpDigestMd5Session(void)
{
	xhttpdigestproof Proof = {
		XHTTP_DIGEST_ALGORITHM_MD5_SESSION,
		XHTTP_DIGEST_QOP_AUTH,
		1u,
		XRT_STR_INIT("939e7578ed9e3c518a452acee763bce9"),
		XRT_STR_INIT("dcd98b7102dd2f0e8b11d0f600bfb0c093"),
		XRT_STR_INIT("0a4f113b"),
		XRT_STR_INIT("/dir/index.html"),
		{ NULL, 0 }
	};
	char Digest[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;

	testRequire(xrtHttpDigestRequest(
		&Proof, XRT_STR_LITERAL("GET"),
		Digest, sizeof(Digest), &iSize
	) && testHttpDigestMd5Equal(
		Digest, iSize, "8e3825c57e897f5a0dec6c2d4e5059d0"
	), "HTTP Digest MD5-sess request mismatch");
	testRequire(xrtHttpDigestRspAuth(
		&Proof, Digest, sizeof(Digest), &iSize
	) && testHttpDigestMd5Equal(
		Digest, iSize, "b600873c6b5797f53d87684d8fc17026"
	), "HTTP Digest MD5-sess rspauth mismatch");
}



int main(void)
{
	testHttpDigestMd5Vector();
	testHttpDigestMd5Session();
	puts("[PASS] HTTP Digest MD5 compatibility");
	return 0;
}
