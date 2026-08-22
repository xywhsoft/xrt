#include "../test.h"



/* 验证可裁剪 HEX 和 Base64 缓冲构造器。 */
int main(void)
{
	#if defined(XRT_FEATURE_BUFFER_HEX)
		xbuffer* pHex;
		const unsigned char pExpectedHex[] = {
			UINT8_C(0x00), UINT8_C(0x7f), UINT8_C(0xa5), UINT8_C(0xff)
		};

		pHex = xrtBufferFromHex(XRT_STR_LITERAL("00 7f a5 ff"), XHEX_IGNORE_SPACE);
		testRequire(pHex != NULL, "HEX buffer construction failed");
		testRequire(
			(pHex->Size == sizeof(pExpectedHex)) &&
			(memcmp(pHex->Data, pExpectedHex, sizeof(pExpectedHex)) == 0),
			"HEX buffer content mismatch"
		);
		xrtBufferDestroy(pHex);

		xrtClearError();
		testRequire(
			xrtBufferFromHex(XRT_STR_LITERAL("abc"), 0) == NULL,
			"invalid HEX buffer text should fail"
		);
		testRequire(
			xrtErrorKind(xrtGetError()) == XERR_PROTOCOL,
			"invalid HEX buffer error mismatch"
		);
	#endif

	#if defined(XRT_FEATURE_BUFFER_BASE64)
		xbuffer* pBase64;
		xbase64config tConfig;
		const unsigned char pExpectedBase64[] = {
			UINT8_C(0x00), UINT8_C(0x01), UINT8_C(0x02), UINT8_C(0xff)
		};

		pBase64 = xrtBufferFromBase64(XRT_STR_LITERAL("AAEC/w=="), NULL);
		testRequire(pBase64 != NULL, "Base64 buffer construction failed");
		testRequire(
			(pBase64->Size == sizeof(pExpectedBase64)) &&
			(memcmp(
				pBase64->Data,
				pExpectedBase64,
				sizeof(pExpectedBase64)
			) == 0),
			"Base64 buffer content mismatch"
		);
		xrtBufferDestroy(pBase64);

		tConfig.Alphabet = NULL;
		tConfig.Flags = XBASE64_URL | XBASE64_NO_PADDING;
		pBase64 = xrtBufferFromBase64(XRT_STR_LITERAL("-_8"), &tConfig);
		testRequire(pBase64 != NULL, "URL Base64 buffer construction failed");
		testRequire(
			(pBase64->Size == 2) && (pBase64->Data[0] == UINT8_C(0xfb)) &&
			(pBase64->Data[1] == UINT8_C(0xff)),
			"URL Base64 buffer content mismatch"
		);
		xrtBufferDestroy(pBase64);
	#endif

	printf("[PASS] buffer codec\n");
	return 0;
}
