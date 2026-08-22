#include "../test.h"



/* 要求当前错误属于稳定的正则错误域和代码。 */
static void testRegexError(xregexerror Code, cstr sMessage)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, sMessage);
	testRequire(
		(xrtErrorDomain(pError) != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.regex") == 0),
		sMessage
	);
	testRequire(xrtErrorCode(pError) == (int32)Code, sMessage);
}



/* 验证明示长度、空模式和原始模式元数据。 */
static void testRegexCompile(void)
{
	const char arrPattern[] = { 'a', '+' };
	xregex* pRegex = xrtRegexCompile((xstrview){ arrPattern, sizeof(arrPattern) });
	xstrview Pattern;

	testRequire(pRegex != NULL, "explicit-length regex compile failed");
	Pattern = xrtRegexPattern(pRegex);
	testRequire(
		(Pattern.Size == sizeof(arrPattern)) &&
		(memcmp(Pattern.Data, arrPattern, sizeof(arrPattern)) == 0),
		"compiled regex pattern metadata mismatch"
	);
	testRequire(xrtRegexFlags(pRegex) == 0, "default regex flags mismatch");
	testRequire(xrtRegexCaptureCount(pRegex) == 1u, "regex group zero is missing");
	xrtRegexRelease(xrtRegexRef(pRegex));
	xrtRegexRelease(pRegex);

	pRegex = xrtRegexCompile((xstrview){ NULL, 0 });
	testRequire(pRegex != NULL, "empty regex pattern compile failed");
	xrtRegexRelease(pRegex);
}



/* 验证正则字面量转义的长度、缓冲区、原地和二进制契约。 */
static void testRegexEscape(void)
{
	static const char sText[] = "a+b[c].(d){2}^$?\\";
	static const char sExpected[] = "a\\+b\\[c\\]\\.\\(d\\)\\{2\\}\\^\\$\\?\\\\";
	static const char arrBinary[] = { 0, '*' };
	char arrOutput[64];
	char arrInPlace[32] = "a+b";
	char arrPartial[16] = "a+b";
	size_t iSize = 0;
	str sEscaped;
	xregex* pRegex;

	testRequire(xrtRegexEscapeSize(XRT_STR_LITERAL(sText), &iSize) &&
		(iSize == strlen(sExpected)), "regex escape size mismatch");
	testRequire(xrtRegexEscapeWrite(
		XRT_STR_LITERAL(sText),
		NULL,
		0,
		&iSize
	) && (iSize == strlen(sExpected)), "regex escape query mismatch");
	testRequire(xrtRegexEscapeWrite(
		XRT_STR_LITERAL(sText),
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, sExpected) == 0), "regex escape output mismatch");

	testRequire(xrtRegexEscapeWrite(
		xrtStrViewN(arrBinary, sizeof(arrBinary)),
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (iSize == 3u) && (arrOutput[0] == 0) &&
		(arrOutput[1] == '\\') && (arrOutput[2] == '*') && (arrOutput[3] == 0),
		"binary regex escape mismatch");
	testRequire(xrtRegexEscapeWrite(
		xrtStrViewN(arrInPlace, 3u),
		arrInPlace,
		sizeof(arrInPlace),
		&iSize
	) && (strcmp(arrInPlace, "a\\+b") == 0), "in-place regex escape mismatch");

	memcpy(arrOutput, "keep", 5u);
	xrtClearError();
	testRequire(!xrtRegexEscapeWrite(
		XRT_STR_LITERAL("a+b"),
		arrOutput,
		4u,
		&iSize
	) && (iSize == 4u) && (memcmp(arrOutput, "keep", 5u) == 0),
		"short regex escape buffer published partial output");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"short regex escape buffer error mismatch");
	xrtClearError();
	testRequire(!xrtRegexEscapeWrite(
		xrtStrViewN(arrPartial, 3u),
		arrPartial + 1u,
		sizeof(arrPartial) - 1u,
		&iSize
	), "partially overlapping regex escape was accepted");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"regex escape overlap error mismatch");
	xrtClearError();

	sEscaped = xrtRegexEscape(XRT_STR_LITERAL(sText), &iSize);
	testRequire((sEscaped != NULL) && (strcmp(sEscaped, sExpected) == 0),
		"allocated regex escape mismatch");
	pRegex = xrtRegexCompile(xrtStrViewN(sEscaped, iSize));
	testRequire(pRegex != NULL, "escaped regex literal did not compile");
	xrtRegexRelease(pRegex);
	xrtFree(sEscaped);
}



/* 验证标志、命名捕获及名称到索引映射。 */
static void testRegexMetadata(void)
{
	xregexconfig Config;
	xregex* pRegex;
	xstrview Name;

	xrtRegexConfigInit(&Config);
	Config.Flags = XREGEX_IGNORE_CASE | XREGEX_MULTILINE;
	pRegex = xrtRegexCompileConfig(
		XRT_STR_LITERAL("(?<word>[a-z]+)-(\\d+)"),
		&Config
	);
	testRequire(pRegex != NULL, "configured regex compile failed");
	testRequire(xrtRegexFlags(pRegex) == Config.Flags, "regex flags were not retained");
	testRequire(xrtRegexCaptureCount(pRegex) == 3u, "regex capture count mismatch");
	testRequire(
		xrtRegexCaptureName(pRegex, 1, &Name) &&
		(Name.Size == 4u) &&
		(memcmp(Name.Data, "word", 4u) == 0),
		"named capture metadata mismatch"
	);
	testRequire(
		xrtRegexCaptureName(pRegex, 2, &Name) && (Name.Size == 0),
		"unnamed capture should expose an empty name"
	);
	testRequire(
		xrtRegexCaptureIndex(pRegex, XRT_STR_LITERAL("word")) == 1u,
		"named capture lookup mismatch"
	);
	testRequire(
		xrtRegexCaptureIndex(pRegex, XRT_STR_LITERAL("missing")) == XRT_NPOS,
		"missing capture name did not return XRT_NPOS"
	);
	xrtRegexRelease(pRegex);
}



/* 验证语法错误包含稳定代码和可读取的字节位置。 */
static void testRegexSyntaxError(void)
{
	xregex* pRegex;
	size_t iOffset = XRT_NPOS;

	xrtClearError();
	pRegex = xrtRegexCompile(XRT_STR_LITERAL("(abc"));
	testRequire(pRegex == NULL, "invalid regex pattern was accepted");
	testRegexError(XREGEX_ERROR_PATTERN, "regex syntax error metadata mismatch");
	testRequire(
		xrtRegexErrorOffset(xrtGetError(), &iOffset) && (iOffset <= 4u),
		"regex syntax error offset is unavailable"
	);
	xrtClearError();
	testRequire(!xrtRegexValid(XRT_STR_LITERAL("[")), "unfinished class was accepted");
	testRegexError(XREGEX_ERROR_PATTERN, "regex validation error mismatch");
}



/* 验证模式字节和捕获数量预算均为硬限制。 */
static void testRegexLimits(void)
{
	xregexconfig Config;
	xregex* pRegex;

	xrtRegexConfigInit(&Config);
	Config.MaxPatternBytes = 2u;
	xrtClearError();
	pRegex = xrtRegexCompileConfig(XRT_STR_LITERAL("abc"), &Config);
	testRequire(pRegex == NULL, "regex pattern byte limit was ignored");
	testRegexError(XREGEX_ERROR_LIMIT, "regex pattern limit error mismatch");

	xrtRegexConfigInit(&Config);
	Config.MaxCaptures = 2u;
	xrtClearError();
	pRegex = xrtRegexCompileConfig(XRT_STR_LITERAL("(a)(b)"), &Config);
	testRequire(pRegex == NULL, "regex capture limit was ignored");
	testRegexError(XREGEX_ERROR_LIMIT, "regex capture limit error mismatch");

	xrtRegexConfigInit(&Config);
	Config.Reserved[0] = 1u;
	xrtClearError();
	pRegex = xrtRegexCompileConfig(XRT_STR_LITERAL("a"), &Config);
	testRequire(pRegex == NULL, "regex reserved config field was accepted");
	testRegexError(XREGEX_ERROR_CONFIG, "regex config error mismatch");
}



/* 运行正则编译层全部契约测试。 */
int main(void)
{
	testRegexCompile();
	testRegexEscape();
	testRegexMetadata();
	testRegexSyntaxError();
	testRegexLimits();
	printf("[PASS] regex core\n");
	return 0;
}
