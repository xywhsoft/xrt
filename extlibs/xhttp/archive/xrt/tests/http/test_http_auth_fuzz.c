#include "../test.h"

#include "../../fuzz/http_auth_protocol.c"



#ifndef XRT_HTTP_AUTH_FUZZ_ROUNDS
	#define XRT_HTTP_AUTH_FUZZ_ROUNDS 2000u
#endif

#define XRT_HTTP_AUTH_FUZZ_TEST_MAX 4096u



/* 生成可重复的认证协议噪声。 */
static uint32 testHttpAuthFuzzNext(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 先执行语法边界种子，再运行固定种子的变长随机输入。 */
int main(void)
{
	static const cstr Seeds[] = {
		"",
		"Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==",
		"Basic realm=\"api\", charset=\"UTF-8\"",
		"Bearer mF_9.B5f-4.1JqM",
		"Bearer realm=\"api\", error=\"invalid_token\", "
			"error_uri=\"https://example.com/help\"",
		"Digest realm=\"api\", nonce=\"n,1\", algorithm=SHA-256",
		"Digest realm=\"api\", nonce=\"n\", algorithm=SHA-256, "
			"qop=\"auth,auth-int\"",
		"Digest username=\"Mufasa\", realm=\"api\", uri=\"/\", "
			"algorithm=SHA-256, nonce=\"n\", nc=00000001, "
			"cnonce=\"c\", qop=auth, "
			"response=\"0123456789abcdef0123456789abcdef"
			"0123456789abcdef0123456789abcdef\"",
		"nextnonce=\"next\", qop=auth, "
			"rspauth=\"0123456789abcdef0123456789abcdef"
			"0123456789abcdef0123456789abcdef\", "
			"cnonce=\"c\", nc=00000001",
		"Digest realm =",
		"Digest realm=\"unterminated",
		"Basic abc, Bearer def",
		", , Custom, Basic dXNlcjpwYXNzd29yZA=="
	};
	uint8 Data[XRT_HTTP_AUTH_FUZZ_TEST_MAX];
	uint32 iState = UINT32_C(0x510E527F);

	testRequire(
		xrtHttpAuthFuzzerTestOneInput(NULL, 0) == 0,
		"HTTP authentication empty fuzz seed failed"
	);
	for ( size_t i = 0; i < (sizeof(Seeds) / sizeof(Seeds[0])); i++ ) {
		testRequire(
			xrtHttpAuthFuzzerTestOneInput(
				(const uint8*)Seeds[i],
				strlen(Seeds[i])
			) == 0,
			"HTTP authentication fixed fuzz seed failed"
		);
	}
	for ( size_t iRound = 0;
		iRound < XRT_HTTP_AUTH_FUZZ_ROUNDS;
		iRound++ ) {
		size_t iSize = (size_t)(
			testHttpAuthFuzzNext(&iState) %
			(XRT_HTTP_AUTH_FUZZ_TEST_MAX + 1u)
		);

		for ( size_t i = 0; i < iSize; i++ ) {
			Data[i] = (uint8)(
				testHttpAuthFuzzNext(&iState) >> 24u
			);
		}
		testRequire(
			xrtHttpAuthFuzzerTestOneInput(Data, iSize) == 0,
			"HTTP authentication deterministic fuzz round failed"
		);
	}
	printf(
		"[PASS] HTTP authentication protocol fuzz (%u rounds)\n",
		(unsigned int)XRT_HTTP_AUTH_FUZZ_ROUNDS
	);
	return 0;
}
