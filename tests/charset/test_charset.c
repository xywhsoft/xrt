#include "../test.h"



/* 五种 Unicode 编码方案必须通过同一标量管线完整往返。 */
static void testAllEncodings(void)
{
	static const unsigned char arrUtf8[] = {
		'A', (unsigned char)0xE4, (unsigned char)0xBD, (unsigned char)0xA0,
		(unsigned char)0xF0, (unsigned char)0x9F,
		(unsigned char)0x98, (unsigned char)0x80
	};
	const xencoding arrEncoding[] = {
		XENCODING_UTF8,
		XENCODING_UTF16_LE,
		XENCODING_UTF16_BE,
		XENCODING_UTF32_LE,
		XENCODING_UTF32_BE
	};

	for ( size_t i = 0; i < (sizeof(arrEncoding) / sizeof(arrEncoding[0])); i++ ) {
		bytes pEncoded;
		bytes pRoundTrip;
		size_t iEncodedSize = 0;
		size_t iRoundTripSize = 0;

		pEncoded = xrtTranscode((xbytesview){ arrUtf8, sizeof(arrUtf8) },
			XENCODING_UTF8, arrEncoding[i], XUTF_STRICT, false, &iEncodedSize);
		testRequire(pEncoded != NULL, "Unicode encoding transcode failed");
		pRoundTrip = xrtTranscode((xbytesview){ pEncoded, iEncodedSize },
			arrEncoding[i], XENCODING_UTF8, XUTF_STRICT, false, &iRoundTripSize);
		testRequire((pRoundTrip != NULL) && (iRoundTripSize == sizeof(arrUtf8)),
			"Unicode encoding round trip failed");
		testRequire(memcmp(pRoundTrip, arrUtf8, sizeof(arrUtf8)) == 0,
			"Unicode encoding round trip changed data");
		xrtFree(pRoundTrip);
		xrtFree(pEncoded);
	}
}



/* 显式端序输出必须与主机端序无关。 */
static void testEndianBytes(void)
{
	static const unsigned char arrUtf8[] = {
		'A', (unsigned char)0xF0, (unsigned char)0x9F,
		(unsigned char)0x98, (unsigned char)0x80
	};
	static const unsigned char arrUtf16Le[] = {
		0x41u, 0x00u, 0x3Du, 0xD8u, 0x00u, 0xDEu
	};
	static const unsigned char arrUtf16Be[] = {
		0x00u, 0x41u, 0xD8u, 0x3Du, 0xDEu, 0x00u
	};
	bytes pLe;
	bytes pBe;
	size_t iSize = 0;

	pLe = xrtTranscode((xbytesview){ arrUtf8, sizeof(arrUtf8) },
		XENCODING_UTF8, XENCODING_UTF16_LE, XUTF_STRICT, false, &iSize);
	testRequire((pLe != NULL) && (iSize == sizeof(arrUtf16Le)) &&
		(memcmp(pLe, arrUtf16Le, sizeof(arrUtf16Le)) == 0),
		"UTF-16 LE bytes are wrong");
	pBe = xrtTranscode((xbytesview){ arrUtf8, sizeof(arrUtf8) },
		XENCODING_UTF8, XENCODING_UTF16_BE, XUTF_STRICT, false, &iSize);
	testRequire((pBe != NULL) && (iSize == sizeof(arrUtf16Be)) &&
		(memcmp(pBe, arrUtf16Be, sizeof(arrUtf16Be)) == 0),
		"UTF-16 BE bytes are wrong");
	xrtFree(pBe);
	xrtFree(pLe);
}



/* BOM 检查和写出必须区分 UTF-16 LE 与 UTF-32 LE 的公共前缀。 */
static void testBom(void)
{
	static const unsigned char arrUtf32Le[] = { 0xFFu, 0xFEu, 0, 0, 'A', 0, 0, 0 };
	unsigned char arrOutput[4];
	unsigned char arrShort[2] = { 0xA5u, 0x5Au };
	size_t iSize = 0;

	testRequire(xrtEncodingBom((xbytesview){ arrUtf32Le, sizeof(arrUtf32Le) },
		&iSize) == XENCODING_UTF32_LE, "UTF-32 LE BOM misdetected");
	testRequire(iSize == 4, "UTF-32 LE BOM size is wrong");
	testRequire(xrtEncodingWriteBom(XENCODING_UTF8, arrOutput,
		sizeof(arrOutput)) == 3, "UTF-8 BOM write failed");
	testRequire((arrOutput[0] == 0xEFu) && (arrOutput[1] == 0xBBu) &&
		(arrOutput[2] == 0xBFu), "UTF-8 BOM bytes are wrong");
	testRequire(xrtEncodingUnitSize(XENCODING_UTF32_BE) == 4,
		"UTF-32 unit size is wrong");
	xrtClearError();
	testRequire(xrtEncodingWriteBom(XENCODING_UTF8, arrShort,
		sizeof(arrShort)) == 0, "short BOM target was accepted");
	testRequire((arrShort[0] == 0xA5u) && (arrShort[1] == 0x5Au),
		"short BOM target was modified");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"short BOM target did not report range error");
}



/* 截断码元和未配对代理项遵循严格或替换策略。 */
static void testInvalidInput(void)
{
	static const unsigned char arrHighSurrogate[] = { 0x3Du, 0xD8u };
	static const unsigned char arrReplacement[] = { 0xEFu, 0xBFu, 0xBDu };
	bytes pText;
	size_t iSize = 0;

	pText = xrtTranscode((xbytesview){ arrHighSurrogate, sizeof(arrHighSurrogate) },
		XENCODING_UTF16_LE, XENCODING_UTF8, XUTF_STRICT, false, &iSize);
	testRequire(pText == NULL, "strict transcode accepted unpaired surrogate");
	pText = xrtTranscode((xbytesview){ arrHighSurrogate, sizeof(arrHighSurrogate) },
		XENCODING_UTF16_LE, XENCODING_UTF8, XUTF_REPLACE, false, &iSize);
	testRequire((pText != NULL) && (iSize == 3) &&
		(memcmp(pText, arrReplacement, 3) == 0),
		"replacement transcode mishandled unpaired surrogate");
	xrtFree(pText);
}



/* 空转码仍按目标码元宽度补零并返回独立所有权。 */
static void testOwnedEmpty(void)
{
	bytes pText;
	size_t iSize = XRT_NPOS;

	pText = xrtTranscode((xbytesview){ NULL, 0 }, XENCODING_UTF8,
		XENCODING_UTF32_BE, XUTF_STRICT, false, &iSize);
	testRequire((pText != NULL) && (iSize == 0), "empty transcode failed");
	testRequire((pText[0] == 0) && (pText[1] == 0) &&
		(pText[2] == 0) && (pText[3] == 0),
		"empty UTF-32 terminator is incomplete");
	xrtFree(pText);
}



/* 输出参数不得借用输入存储，失败时输入字节必须保持不变。 */
static void testOutputAliasing(void)
{
	union {
		size_t Alignment;
		unsigned char Data[16];
	} Input;
	unsigned char arrBefore[sizeof(Input.Data)];

	memset(Input.Data, 0xA5, sizeof(Input.Data));
	Input.Data[0] = 0xEFu;
	Input.Data[1] = 0xBBu;
	Input.Data[2] = 0xBFu;
	memcpy(arrBefore, Input.Data, sizeof(arrBefore));
	xrtClearError();
	testRequire(xrtEncodingBom(
		(xbytesview){ Input.Data, sizeof(Input.Data) },
		&Input.Alignment
	) == XENCODING_UNKNOWN, "BOM size alias was accepted");
	testRequire(memcmp(Input.Data, arrBefore, sizeof(arrBefore)) == 0,
		"BOM size alias modified input");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"BOM size alias did not report argument error");

	memset(Input.Data, 'A', sizeof(Input.Data));
	memcpy(arrBefore, Input.Data, sizeof(arrBefore));
	xrtClearError();
	testRequire(xrtTranscode(
		(xbytesview){ Input.Data, sizeof(Input.Data) },
		XENCODING_UTF8,
		XENCODING_UTF16_LE,
		XUTF_STRICT,
		false,
		&Input.Alignment
	) == NULL, "transcode size alias was accepted");
	testRequire(memcmp(Input.Data, arrBefore, sizeof(arrBefore)) == 0,
		"transcode size alias modified input");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"transcode size alias did not report argument error");
}



/* 执行显式编码方案和 BOM 测试。 */
int main(void)
{
	testAllEncodings();
	testEndianBytes();
	testBom();
	testInvalidInput();
	testOwnedEmpty();
	testOutputAliasing();
	return 0;
}
