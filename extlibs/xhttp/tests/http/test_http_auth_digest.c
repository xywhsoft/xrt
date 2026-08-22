#include "../test.h"

#include <xrt/http_auth.h>



/* 验证算法名称、摘要长度和 session 元数据保持完全对称。 */
static void testHttpDigestAlgorithms(void)
{
	static const struct {
		cstr Name;
		xhttpdigestalgorithm Algorithm;
		size_t Size;
		bool Session;
	} Cases[] = {
		{ "MD5", XHTTP_DIGEST_ALGORITHM_MD5, 16u, false },
		{ "MD5-sess", XHTTP_DIGEST_ALGORITHM_MD5_SESSION, 16u, true },
		{ "SHA-256", XHTTP_DIGEST_ALGORITHM_SHA256, 32u, false },
		{
			"SHA-256-sess",
			XHTTP_DIGEST_ALGORITHM_SHA256_SESSION,
			32u,
			true
		},
		{
			"SHA-512-256",
			XHTTP_DIGEST_ALGORITHM_SHA512_256,
			32u,
			false
		},
		{
			"SHA-512-256-sess",
			XHTTP_DIGEST_ALGORITHM_SHA512_256_SESSION,
			32u,
			true
		}
	};

	for ( size_t i = 0; i < (sizeof(Cases) / sizeof(Cases[0])); i++ ) {
		xstrview Name = {
			Cases[i].Name,
			strlen(Cases[i].Name)
		};
		xstrview Canonical;

		testRequire(
			xrtHttpDigestAlgorithmParse(Name) == Cases[i].Algorithm,
			"HTTP Digest algorithm parse mismatch"
		);
		Canonical = xrtHttpDigestAlgorithmName(Cases[i].Algorithm);
		testRequire(
			(Canonical.Size == Name.Size) &&
			(memcmp(Canonical.Data, Cases[i].Name, Name.Size) == 0) &&
			(xrtHttpDigestSize(Cases[i].Algorithm) == Cases[i].Size) &&
			(xrtHttpDigestAlgorithmSession(Cases[i].Algorithm) ==
			 Cases[i].Session),
			"HTTP Digest algorithm metadata mismatch"
		);
	}
	testRequire(
		xrtHttpDigestAlgorithmParse(
			XRT_STR_LITERAL("sha-256-SeSs")
		) == XHTTP_DIGEST_ALGORITHM_SHA256_SESSION,
		"HTTP Digest algorithm comparison was case-sensitive"
	);
	testRequire(
		xrtHttpDigestAlgorithmParse(
			XRT_STR_LITERAL("future-hash")
		) == XHTTP_DIGEST_ALGORITHM_UNKNOWN,
		"HTTP Digest unknown algorithm was not preserved as unknown"
	);
	testRequire(
		xrtHttpDigestAlgorithmName(
			XHTTP_DIGEST_ALGORITHM_UNKNOWN
		).Size == 0u,
		"HTTP Digest unknown algorithm exposed a canonical name"
	);
	testRequire(
		(xrtHttpDigestSize(XHTTP_DIGEST_ALGORITHM_UNKNOWN) == 0u) &&
		!xrtHttpDigestAlgorithmSession(
			XHTTP_DIGEST_ALGORITHM_UNKNOWN
		),
		"HTTP Digest unknown algorithm metadata mismatch"
	);
}



/* 验证 qop 元数据和当前裁剪闭包的算法能力报告。 */
static void testHttpDigestCapabilities(void)
{
	xstrview Name;

	testRequire(
		xrtHttpDigestQopParse(
			XRT_STR_LITERAL("AuTh")
		) == XHTTP_DIGEST_QOP_AUTH,
		"HTTP Digest auth qop parse failed"
	);
	testRequire(
		xrtHttpDigestQopParse(
			XRT_STR_LITERAL("AUTH-INT")
		) == XHTTP_DIGEST_QOP_AUTH_INT,
		"HTTP Digest auth-int qop parse failed"
	);
	testRequire(
		xrtHttpDigestQopParse(
			XRT_STR_LITERAL("future")
		) == XHTTP_DIGEST_QOP_NONE,
		"HTTP Digest unknown qop was accepted"
	);
	Name = xrtHttpDigestQopName(XHTTP_DIGEST_QOP_AUTH_INT);
	testRequire(
		(Name.Size == 8u) &&
		(memcmp(Name.Data, "auth-int", 8u) == 0) &&
		(xrtHttpDigestQopName(XHTTP_DIGEST_QOP_NONE).Size == 0u),
		"HTTP Digest qop canonical metadata mismatch"
	);

	#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2)
	testRequire(
		xrtHttpDigestAlgorithmSupported(
			XHTTP_DIGEST_ALGORITHM_SHA256
		) && xrtHttpDigestAlgorithmSupported(
			XHTTP_DIGEST_ALGORITHM_SHA512_256_SESSION
		),
		"HTTP Digest SHA-2 capability was not reported"
	);
	#else
	testRequire(
		!xrtHttpDigestAlgorithmSupported(
			XHTTP_DIGEST_ALGORITHM_SHA256
		) && !xrtHttpDigestAlgorithmSupported(
			XHTTP_DIGEST_ALGORITHM_SHA512_256
		),
		"HTTP Digest metadata pulled a SHA-2 backend"
	);
	#endif

	#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_MD5)
	testRequire(
		xrtHttpDigestAlgorithmSupported(
			XHTTP_DIGEST_ALGORITHM_MD5_SESSION
		),
		"HTTP Digest MD5 capability was not reported"
	);
	#else
	testRequire(
		!xrtHttpDigestAlgorithmSupported(
			XHTTP_DIGEST_ALGORITHM_MD5
		),
		"HTTP Digest metadata pulled the legacy MD5 backend"
	);
	#endif
}



int main(void)
{
	testHttpDigestAlgorithms();
	testHttpDigestCapabilities();
	puts("[PASS] HTTP Digest metadata");
	return 0;
}
