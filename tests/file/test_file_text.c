#include "../test.h"



/* 在系统临时目录构造文本测试路径。 */
static str testTextPath(cstr sName)
{
	str sDirectory = xrtPathTemp();
	str sPath;

	testRequire(sDirectory != NULL, "temporary directory query failed");
	sPath = xrtPathJoin(sDirectory, sName);
	xrtFree(sDirectory);
	testRequire(sPath != NULL, "file-text path allocation failed");
	return sPath;
}



/* 五种 Unicode 文件编码必须通过同一 UTF-8 文本契约往返。 */
static void testTextEncodings(cstr sPath)
{
	static const char arrText[] = {
		'A', 'S', 'C', 'I', 'I', ' ',
		(char)0xE4, (char)0xBD, (char)0xA0, ' ',
		(char)0xF0, (char)0x9F, (char)0x98, (char)0x80
	};
	const xencoding arrEncoding[] = {
		XENCODING_UTF8,
		XENCODING_UTF16_LE,
		XENCODING_UTF16_BE,
		XENCODING_UTF32_LE,
		XENCODING_UTF32_BE
	};
	size_t i;

	for ( i = 0; i < (sizeof(arrEncoding) / sizeof(arrEncoding[0])); i++ ) {
		str sText;
		size_t iSize;

		testRequire(xrtFileWriteText(sPath,
			(xstrview){ arrText, sizeof(arrText) }, arrEncoding[i],
			XUTF_STRICT, true), "encoded text write failed");
		sText = xrtFileReadText(sPath, XENCODING_UNKNOWN,
			XUTF_STRICT, &iSize);
		testRequire((sText != NULL) && (iSize == sizeof(arrText)) &&
			(memcmp(sText, arrText, iSize) == 0) && (sText[iSize] == '\0'),
			"BOM-detected text round trip failed");
		xrtFree(sText);
	}
}



/* 无 BOM 文本必须支持显式编码，并只在证据充分时自动检测。 */
static void testTextWithoutBom(cstr sPath)
{
	static const char sText[] = "ASCII text used for UTF-16 detection";
	str sRead;
	size_t iSize;

	testRequire(xrtFileWriteText(sPath, XRT_STR_LITERAL(sText),
		XENCODING_UTF16_LE, XUTF_STRICT, false),
		"BOM-less UTF-16 write failed");
	sRead = xrtFileReadText(sPath, XENCODING_UNKNOWN,
		XUTF_STRICT, &iSize);
	testRequire((sRead != NULL) && (iSize == (sizeof(sText) - 1u)) &&
		(strcmp(sRead, sText) == 0), "BOM-less UTF-16 detection failed");
	xrtFree(sRead);

	testRequire(xrtFileWriteText(sPath, XRT_STR_LITERAL("plain ASCII"),
		XENCODING_UTF8, XUTF_STRICT, false),
		"BOM-less UTF-8 write failed");
	sRead = xrtFileReadText(sPath, XENCODING_UNKNOWN,
		XUTF_STRICT, &iSize);
	testRequire((sRead != NULL) && (strcmp(sRead, "plain ASCII") == 0),
		"BOM-less ASCII UTF-8 detection failed");
	xrtFree(sRead);
}



/* 文本读取上限按源文件字节计数，并允许精确边界。 */
static void testTextReadLimit(cstr sPath)
{
	str sText;
	size_t iSize = XRT_NPOS;

	testRequire(xrtFileWriteAll(sPath,
		(xbytesview){ (cbytes)"text", 4u }),
		"text limit fixture write failed");
	sText = xrtFileReadTextLimit(sPath, XENCODING_UTF8,
		XUTF_STRICT, 3u, &iSize);
	testRequire((sText == NULL) && (iSize == 0u) &&
		(xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XFILE_ERROR_LIMIT),
		"text source limit did not reject oversized input");
	xrtClearError();

	sText = xrtFileReadTextLimit(sPath, XENCODING_UTF8,
		XUTF_STRICT, 4u, &iSize);
	testRequire((sText != NULL) && (iSize == 4u) &&
		(memcmp(sText, "text", 4u) == 0) && (sText[4] == '\0'),
		"text source limit rejected the exact boundary");
	xrtFree(sText);

	testRequire(xrtFileWriteText(sPath, XRT_STR_LITERAL("A"),
		XENCODING_UTF16_LE, XUTF_STRICT, true),
		"text BOM limit fixture write failed");
	sText = xrtFileReadTextLimit(sPath, XENCODING_UNKNOWN,
		XUTF_STRICT, 3u, &iSize);
	testRequire((sText == NULL) && (iSize == 0u) &&
		(xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XFILE_ERROR_LIMIT),
		"text source limit did not count the BOM bytes");
	xrtClearError();

	sText = xrtFileReadTextLimit(sPath, XENCODING_UNKNOWN,
		XUTF_STRICT, 4u, &iSize);
	testRequire((sText != NULL) && (iSize == 1u) &&
		(sText[0] == 'A') && (sText[1] == '\0'),
		"text BOM source limit rejected the exact boundary");
	xrtFree(sText);
}



/* BOM 冲突和未知二进制必须失败，不能悄悄选择本地代码页。 */
static void testTextDetectionErrors(cstr sPath)
{
	static const unsigned char arrBinary[] = { 0xFFu, 0x01u, 0xFEu };
	str sText;

	testRequire(xrtFileWriteText(sPath, XRT_STR_LITERAL("text"),
		XENCODING_UTF16_LE, XUTF_STRICT, true),
		"BOM conflict fixture write failed");
	testRequire(xrtFileReadText(sPath, XENCODING_UTF8,
		XUTF_STRICT, NULL) == NULL, "conflicting BOM was accepted");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XFILE_ERROR_TEXT),
		"BOM conflict reported the wrong error");
	xrtClearError();

	testRequire(xrtFileWriteAll(sPath,
		(xbytesview){ arrBinary, sizeof(arrBinary) }),
		"unknown binary fixture write failed");
	sText = xrtFileReadText(sPath, XENCODING_UNKNOWN, XUTF_STRICT, NULL);
	testRequire(sText == NULL, "unknown binary data was accepted as text");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XFILE_ERROR_TEXT),
		"unknown binary data reported the wrong error");
	xrtClearError();
}



/* 严格失败必须保留字符集原因，替换策略则写入 U+FFFD。 */
static void testTextInvalidUtf8(cstr sPath)
{
	static const unsigned char arrInvalid[] = { 'A', 0xC0u, 0xAFu, 'B' };
	static const unsigned char arrExpected[] = {
		'A', 0xEFu, 0xBFu, 0xBDu, 0xEFu, 0xBFu, 0xBDu, 'B'
	};
	str sText;
	size_t iSize;

	testRequire(xrtFileWriteAll(sPath,
		(xbytesview){ arrInvalid, sizeof(arrInvalid) }),
		"invalid UTF-8 fixture write failed");
	testRequire(xrtFileReadText(sPath, XENCODING_UTF8,
		XUTF_STRICT, NULL) == NULL, "strict text read accepted invalid UTF-8");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XFILE_ERROR_TEXT) &&
		(xrtErrorCause(xrtGetError()) != NULL),
		"text decode failure lost its charset cause");
	xrtClearError();

	sText = xrtFileReadText(sPath, XENCODING_UTF8, XUTF_REPLACE, &iSize);
	testRequire((sText != NULL) && (iSize == sizeof(arrExpected)) &&
		(memcmp(sText, arrExpected, iSize) == 0),
		"replacement text read produced incorrect UTF-8");
	xrtFree(sText);
}



/* 原子文本写必须先完成转码，失败时不能破坏旧文件。 */
static void testTextAtomic(cstr sPath)
{
	static const char arrInvalid[] = { 'A', (char)0xC0, (char)0xAF };
	str sText;
	size_t iSize;

	testRequire(xrtFileWriteTextAtomic(sPath, XRT_STR_LITERAL("stable"),
		XENCODING_UTF8, XUTF_STRICT, false),
		"atomic text write failed");
	testRequire(!xrtFileWriteTextAtomic(sPath,
		(xstrview){ arrInvalid, sizeof(arrInvalid) },
		XENCODING_UTF16_BE, XUTF_STRICT, true),
		"strict atomic text write accepted invalid UTF-8");
	xrtClearError();
	sText = xrtFileReadText(sPath, XENCODING_UTF8, XUTF_STRICT, &iSize);
	testRequire((sText != NULL) && (iSize == 6u) &&
		(memcmp(sText, "stable", 6) == 0),
		"failed atomic transcode changed the old file");
	xrtFree(sText);
}



/* 空文本仍必须返回独立拥有的零结尾 UTF-8 字符串。 */
static void testTextEmpty(cstr sPath)
{
	const xencoding arrEncoding[] = {
		XENCODING_UTF8,
		XENCODING_UTF16_LE,
		XENCODING_UTF16_BE,
		XENCODING_UTF32_LE,
		XENCODING_UTF32_BE
	};
	str sText;
	size_t iSize = XRT_NPOS;
	size_t i;

	testRequire(xrtFileWriteText(sPath, (xstrview){ NULL, 0 },
		XENCODING_UTF32_BE, XUTF_STRICT, false), "empty text write failed");
	sText = xrtFileReadText(sPath, XENCODING_UNKNOWN, XUTF_STRICT, &iSize);
	testRequire((sText != NULL) && (iSize == 0u) && (sText[0] == '\0'),
		"empty text read did not return owned UTF-8");
	xrtFree(sText);

	for ( i = 0; i < (sizeof(arrEncoding) / sizeof(arrEncoding[0])); i++ ) {
		testRequire(xrtFileWriteText(sPath, (xstrview){ NULL, 0 },
			arrEncoding[i], XUTF_STRICT, true),
			"BOM-only text write failed");
		sText = xrtFileReadText(sPath, XENCODING_UNKNOWN,
			XUTF_STRICT, &iSize);
		testRequire((sText != NULL) && (iSize == 0u) &&
			(sText[0] == '\0'), "BOM-only text read was not empty");
		xrtFree(sText);
	}
}



/* 文件文本层回归入口。 */
int main(void)
{
	str sPath = testTextPath("xrt-file-text.tmp");

	(void)xrtFileDelete(sPath);
	xrtClearError();
	testTextEncodings(sPath);
	testTextWithoutBom(sPath);
	testTextReadLimit(sPath);
	testTextDetectionErrors(sPath);
	testTextInvalidUtf8(sPath);
	testTextAtomic(sPath);
	testTextEmpty(sPath);
	testRequire(xrtFileDelete(sPath), "file-text cleanup failed");
	xrtFree(sPath);
	return 0;
}
