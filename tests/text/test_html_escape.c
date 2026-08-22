#include "../test.h"



/* 验证文本与引号属性两种上下文的基础向量。 */
static void testHtmlEscapeVectors(void)
{
	static const char TextInput[] = "<b title=\"x'y\">A&B</b>";
	static const char TextOutput[] =
		"&lt;b title=\"x'y\"&gt;A&amp;B&lt;/b&gt;";
	static const char AttributeInput[] = "A&B<\"'雪";
	static const char AttributeOutput[] =
		"A&amp;B&lt;&quot;&#39;雪";
	char Buffer[128];
	size_t iSize;

	testRequire(xrtHtmlEscapeSize(
		XRT_STR_LITERAL(TextInput), XHTML_ESCAPE_TEXT, &iSize
	) && (iSize == sizeof(TextOutput) - 1u),
		"HTML text size mismatch");
	testRequire(xrtHtmlEscapeWrite(
		XRT_STR_LITERAL(TextInput), XHTML_ESCAPE_TEXT,
		Buffer, sizeof(Buffer), &iSize
	) && (iSize == sizeof(TextOutput) - 1u) &&
		(memcmp(Buffer, TextOutput, sizeof(TextOutput)) == 0),
		"HTML text escape mismatch");
	testRequire(xrtHtmlEscapeWrite(
		XRT_STR_LITERAL(AttributeInput), XHTML_ESCAPE_ATTRIBUTE,
		Buffer, sizeof(Buffer), &iSize
	) && (iSize == sizeof(AttributeOutput) - 1u) &&
		(memcmp(Buffer, AttributeOutput, sizeof(AttributeOutput)) == 0),
		"HTML attribute escape mismatch");
}



/* 验证明示长度会保留嵌入零、Unicode 和空文本。 */
static void testHtmlEscapeExplicitLength(void)
{
	static const char Input[] = "A\0&雪";
	static const char Expected[] = "A\0&amp;雪";
	char Buffer[32];
	str sAllocated;
	size_t iSize;

	testRequire(xrtHtmlEscapeWrite(
		(xstrview){ Input, sizeof(Input) - 1u }, XHTML_ESCAPE_TEXT,
		Buffer, sizeof(Buffer), &iSize
	) && (iSize == sizeof(Expected) - 1u) &&
		(memcmp(Buffer, Expected, sizeof(Expected)) == 0),
		"HTML embedded-zero escape mismatch");
	testRequire(xrtHtmlEscapeWrite(
		(xstrview){ NULL, 0 }, XHTML_ESCAPE_TEXT,
		NULL, 0, &iSize
	) && (iSize == 0), "HTML empty query mismatch");
	sAllocated = xrtHtmlEscape(
		(xstrview){ NULL, 0 }, XHTML_ESCAPE_TEXT, &iSize
	);
	testRequire(
		(sAllocated != NULL) && (iSize == 0) && (sAllocated[0] == '\0'),
		"HTML empty allocation mismatch"
	);
	xrtFree(sAllocated);
}



/* 验证分配便捷层与可选长度输出。 */
static void testHtmlEscapeAllocation(void)
{
	str sOutput;
	size_t iSize;

	sOutput = xrtHtmlEscape(
		XRT_STR_LITERAL("<雪 & 雨>"), XHTML_ESCAPE_TEXT, &iSize
	);
	testRequire(
		(sOutput != NULL) &&
		(iSize == strlen("&lt;雪 &amp; 雨&gt;")) &&
		(strcmp(sOutput, "&lt;雪 &amp; 雨&gt;") == 0),
		"HTML allocated escape mismatch"
	);
	xrtFree(sOutput);
	sOutput = xrtHtmlEscape(
		XRT_STR_LITERAL("\"x\""), XHTML_ESCAPE_ATTRIBUTE, NULL
	);
	testRequire(
		(sOutput != NULL) && (strcmp(sOutput, "&quot;x&quot;") == 0),
		"HTML optional allocation size mismatch"
	);
	xrtFree(sOutput);
}



/* 验证同址扩张与非同址部分重叠边界。 */
static void testHtmlEscapeOverlap(void)
{
	typedef union testhtmlalias {
		size_t Size;
		char Buffer[64];
	} testhtmlalias;
	testhtmlalias Alias;
	char Buffer[64];
	char Before[64];
	char AliasBefore[sizeof(Alias)];
	size_t iSize;
	str sOutput;

	memcpy(Buffer, "A<&'\"B", 6u);
	testRequire(xrtHtmlEscapeWrite(
		(xstrview){ Buffer, 6u }, XHTML_ESCAPE_ATTRIBUTE,
		Buffer, sizeof(Buffer), &iSize
	) && (strcmp(Buffer, "A&lt;&amp;&#39;&quot;B") == 0),
		"HTML in-place expansion mismatch");

	memset(Buffer, 0xA5, sizeof(Buffer));
	memcpy(Buffer, "A&B", 3u);
	memcpy(Before, Buffer, sizeof(Buffer));
	iSize = 77u;
	testRequire(
		!xrtHtmlEscapeWrite(
			(xstrview){ Buffer, 3u }, XHTML_ESCAPE_TEXT,
			Buffer + 1u, sizeof(Buffer) - 1u, &iSize
		) && (iSize == 77u) &&
		(memcmp(Buffer, Before, sizeof(Buffer)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTML accepted partial overlap"
	);

	memset(&Alias, 0xA5, sizeof(Alias));
	memcpy(Alias.Buffer, "A&B", 3u);
	memcpy(AliasBefore, &Alias, sizeof(Alias));
	testRequire(
		!xrtHtmlEscapeSize(
			(xstrview){ Alias.Buffer, 3u }, XHTML_ESCAPE_TEXT,
			&Alias.Size
		) && (memcmp(&Alias, AliasBefore, sizeof(Alias)) == 0),
		"HTML accepted a size output inside input"
	);
	testRequire(
		!xrtHtmlEscapeWrite(
			XRT_STR_LITERAL("A&B"), XHTML_ESCAPE_TEXT,
			Alias.Buffer, sizeof(Alias.Buffer), &Alias.Size
		) && (memcmp(&Alias, AliasBefore, sizeof(Alias)) == 0),
		"HTML accepted a size output inside target"
	);
	sOutput = xrtHtmlEscape(
		(xstrview){ Alias.Buffer, 3u }, XHTML_ESCAPE_TEXT, &Alias.Size
	);
	testRequire(
		(sOutput == NULL) &&
		(memcmp(&Alias, AliasBefore, sizeof(Alias)) == 0),
		"HTML allocation accepted a size output inside input"
	);
}



/* 验证格式、模式和容量失败保持输出原子性。 */
static void testHtmlEscapeFailures(void)
{
	static const char InvalidUtf8[] = { (char)0xC0, (char)0xAF };
	char Buffer[32];
	char Before[32];
	size_t iSize;

	memset(Buffer, 0xA5, sizeof(Buffer));
	memcpy(Before, Buffer, sizeof(Buffer));
	iSize = 77u;
	testRequire(
		!xrtHtmlEscapeWrite(
			XRT_STR_LITERAL("A&B"), XHTML_ESCAPE_TEXT,
			Buffer, 7u, &iSize
		) && (iSize == 7u) &&
		(memcmp(Buffer, Before, sizeof(Buffer)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTML short output was not atomic"
	);

	iSize = 77u;
	testRequire(
		!xrtHtmlEscapeWrite(
			(xstrview){ InvalidUtf8, sizeof(InvalidUtf8) },
			XHTML_ESCAPE_TEXT, Buffer, sizeof(Buffer), &iSize
		) && (iSize == 77u) &&
		(memcmp(Buffer, Before, sizeof(Buffer)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.html") == 0) &&
		(xrtErrorCode(xrtGetError()) == XHTML_ERROR_UTF8),
		"HTML invalid UTF-8 contract mismatch"
	);

	iSize = 77u;
	testRequire(
		!xrtHtmlEscapeWrite(
			XRT_STR_LITERAL("x"), (xhtmlescapemode)99,
			Buffer, sizeof(Buffer), &iSize
		) && (iSize == 77u) &&
		(memcmp(Buffer, Before, sizeof(Buffer)) == 0) &&
		(xrtErrorCode(xrtGetError()) == XHTML_ERROR_MODE),
		"HTML invalid mode contract mismatch"
	);

	testRequire(
		!xrtHtmlEscapeSize(
			(xstrview){ NULL, 1u }, XHTML_ESCAPE_TEXT, &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTML accepted a null non-empty input"
	);
	testRequire(
		!xrtHtmlEscapeWrite(
			XRT_STR_LITERAL("x"), XHTML_ESCAPE_TEXT,
			NULL, 1u, &iSize
		),
		"HTML accepted a null non-empty output"
	);
	testRequire(
		!xrtHtmlEscapeSize(
			XRT_STR_LITERAL("x"), XHTML_ESCAPE_TEXT, NULL
		),
		"HTML accepted a null size output"
	);
}



/* 验证所有显式地址范围在读取或写入前拒绝回绕。 */
static void testHtmlEscapeWrappingRanges(void)
{
	cstr sWrapping = (cstr)(uintptr_t)(UINTPTR_MAX - 1u);
	char* sWrappingOutput = (char*)(uintptr_t)(UINTPTR_MAX - 1u);
	size_t* pWrappingSize =
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u);
	char Buffer[8];
	size_t iSize = 77u;

	testRequire(
		!xrtHtmlEscapeSize(
			(xstrview){ sWrapping, 2u }, XHTML_ESCAPE_TEXT, &iSize
		) && (iSize == 77u),
		"HTML accepted a wrapping input"
	);
	testRequire(
		!xrtHtmlEscapeWrite(
			XRT_STR_LITERAL("x"), XHTML_ESCAPE_TEXT,
			sWrappingOutput, 2u, &iSize
		) && (iSize == 77u),
		"HTML accepted a wrapping output"
	);
	testRequire(
		!xrtHtmlEscapeWrite(
			XRT_STR_LITERAL("x"), XHTML_ESCAPE_TEXT,
			Buffer, sizeof(Buffer), pWrappingSize
		),
		"HTML accepted a wrapping size output"
	);
	testRequire(
		xrtHtmlEscape(
			XRT_STR_LITERAL("x"), XHTML_ESCAPE_TEXT, pWrappingSize
		) == NULL,
		"HTML allocation accepted a wrapping size output"
	);
}



/* 验证短缓冲后的相邻长度字段不会被误判为输出重叠。 */
static void testHtmlEscapeAdjacentSize(void)
{
	typedef struct testhtmloutput {
		char Buffer[4];
		size_t Size;
	} testhtmloutput;
	testhtmloutput Output;
	char Before[sizeof(Output.Buffer)];

	memset(&Output, 0xA5, sizeof(Output));
	memcpy(Before, Output.Buffer, sizeof(Before));
	Output.Size = 77u;
	testRequire(
		!xrtHtmlEscapeWrite(
			XRT_STR_LITERAL("&&"), XHTML_ESCAPE_TEXT,
			Output.Buffer, sizeof(Output.Buffer), &Output.Size
		) && (Output.Size == 10u) &&
		(memcmp(Output.Buffer, Before, sizeof(Before)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTML short output rejected an adjacent size field"
	);
}



/* 执行 HTML 转义的向量、所有权和边界契约测试。 */
int main(void)
{
	testHtmlEscapeVectors();
	testHtmlEscapeExplicitLength();
	testHtmlEscapeAllocation();
	testHtmlEscapeOverlap();
	testHtmlEscapeFailures();
	testHtmlEscapeWrappingRanges();
	testHtmlEscapeAdjacentSize();
	printf("[PASS] html_escape\n");
	return 0;
}
