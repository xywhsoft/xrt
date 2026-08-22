#include "../test.h"

#include <xrt/http_forwarded.h>



/* 验证标准参数、扩展参数和多元素规范写出。 */
static void testForwardedWrite(void)
{
	static const xhttpforwardedpairvalue Extensions[] = {
		{ XRT_STR_INIT("trace"), XRT_STR_INIT("edge,west") }
	};
	static const xhttpforwardedvalue Elements[] = {
		{
			XRT_STR_INIT("192.0.2.43"),
			XRT_STR_INIT(""),
			XRT_STR_INIT(""),
			XRT_STR_INIT(""),
			NULL,
			0,
			XHTTP_FORWARDED_HAS_FOR
		},
		{
			XRT_STR_INIT("[2001:db8::1]:443"),
			XRT_STR_INIT("_edge"),
			XRT_STR_INIT("example.com:8443"),
			XRT_STR_INIT("https"),
			Extensions,
			1,
			XHTTP_FORWARDED_HAS_FOR |
			XHTTP_FORWARDED_HAS_BY |
			XHTTP_FORWARDED_HAS_HOST |
			XHTTP_FORWARDED_HAS_PROTO
		}
	};
	static const char Expected[] =
		"for=192.0.2.43, "
		"for=\"[2001:db8::1]:443\";by=_edge;"
		"host=\"example.com:8443\";proto=https;"
		"trace=\"edge,west\"";
	char sOutput[192];
	size_t iSize;
	str sBuilt;

	testRequire(
		xrtHttpForwardedWrite(
			Elements, 2u, NULL, 0, &iSize
		) && (iSize == (sizeof(Expected) - 1u)),
		"Forwarded writer size query mismatch"
	);
	testRequire(
		xrtHttpForwardedWrite(
			Elements, 2u, sOutput,
			sizeof(sOutput), &iSize
		) && (iSize == (sizeof(Expected) - 1u)) &&
		(memcmp(sOutput, Expected, iSize) == 0),
		"Forwarded writer output mismatch"
	);
	sBuilt = xrtHttpForwardedBuild(Elements, 2u, &iSize);
	testRequire(
		(sBuilt != NULL) &&
		(iSize == (sizeof(Expected) - 1u)) &&
		(strcmp(sBuilt, Expected) == 0),
		"Forwarded build mismatch"
	);
	xrtFree(sBuilt);
}



/* 验证 writer 保留 Host ABNF 的空主机和无固定宽度端口。 */
static void testForwardedWriteHostGrammar(void)
{
	xhttpforwardedvalue Element;
	char sOutput[96];
	size_t iSize;

	memset(&Element, 0, sizeof(Element));
	Element.Host = XRT_STR_LITERAL("");
	Element.Flags = XHTTP_FORWARDED_HAS_HOST;
	testRequire(
		xrtHttpForwardedElementWrite(
			&Element, sOutput, sizeof(sOutput), &iSize
		) && (iSize == 7u) &&
		(memcmp(sOutput, "host=\"\"", 7u) == 0),
		"Forwarded writer rejected an empty Host"
	);
	Element.Host = XRT_STR_LITERAL(":12345678901234567890");
	testRequire(
		xrtHttpForwardedElementWrite(
			&Element, sOutput, sizeof(sOutput), &iSize
		) && xrtHttpForwardedValid(
			(xstrview){ sOutput, iSize }
		),
		"Forwarded writer narrowed the Host port grammar"
	);
}



/* 验证扩展重复、标准槽冲突和容量失败原子性。 */
static void testForwardedWriteFailure(void)
{
	static const xhttpforwardedpairvalue Duplicate[] = {
		{ XRT_STR_INIT("trace"), XRT_STR_INIT("a") },
		{ XRT_STR_INIT("TRACE"), XRT_STR_INIT("b") }
	};
	static const xhttpforwardedpairvalue Known[] = {
		{ XRT_STR_INIT("for"), XRT_STR_INIT("192.0.2.1") }
	};
	xhttpforwardedvalue Element;
	char sOutput[8];
	char sSaved[8];
	size_t iSize;

	memset(&Element, 0, sizeof(Element));
	Element.Extensions = Duplicate;
	Element.ExtensionCount = 2u;
	testRequire(
		!xrtHttpForwardedElementWrite(
			&Element, NULL, 0, &iSize
		),
		"Forwarded writer accepted duplicate extension"
	);
	xrtClearError();
	Element.Extensions = Known;
	Element.ExtensionCount = 1u;
	testRequire(
		!xrtHttpForwardedElementWrite(
			&Element, NULL, 0, &iSize
		),
		"Forwarded writer accepted standard extension name"
	);
	xrtClearError();
	memset(&Element, 0, sizeof(Element));
	Element.For = XRT_STR_LITERAL("192.0.2.1");
	Element.Flags = XHTTP_FORWARDED_HAS_FOR;
	memset(sOutput, 0xA5, sizeof(sOutput));
	memcpy(sSaved, sOutput, sizeof(sSaved));
	iSize = 0;
	testRequire(
		!xrtHttpForwardedElementWrite(
			&Element, sOutput, sizeof(sOutput), &iSize
		) && (iSize == 13u) &&
		(memcmp(sOutput, sSaved, sizeof(sOutput)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"Forwarded short output was not atomic"
	);
	xrtClearError();
}



/* 验证嵌套描述符、未对齐对象、回绕范围和别名的预检顺序。 */
static void testForwardedWriteMemory(void)
{
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpforwardedvalue) + 1u];
	} ElementStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(size_t) + 1u];
	} SizeStorage;
	xhttpforwardedvalue* pUnalignedElement =
		(xhttpforwardedvalue*)(ElementStorage.Bytes + 1u);
	size_t* pUnalignedSize = (size_t*)(SizeStorage.Bytes + 1u);
	xhttpforwardedpairvalue Extension;
	xhttpforwardedvalue Element;
	char sNode[] = "192.0.2.1";
	char sOutput[64];
	char sSaved[64];
	size_t iSize;

	memset(&Element, 0, sizeof(Element));
	Element.For = (xstrview){ sNode, sizeof(sNode) - 1u };
	Element.Flags = XHTTP_FORWARDED_HAS_FOR;
	memcpy(pUnalignedElement, &Element, sizeof(Element));
	testRequire(
		xrtHttpForwardedElementWrite(
			pUnalignedElement, sOutput,
			sizeof(sOutput), pUnalignedSize
		),
		"Forwarded writer rejected unaligned descriptors"
	);
	memcpy(&iSize, pUnalignedSize, sizeof(iSize));
	testRequire(
		(iSize == 13u) &&
		(memcmp(sOutput, "for=192.0.2.1", 13u) == 0),
		"Forwarded unaligned writer result mismatch"
	);
	iSize = 77u;
	xrtClearError();
	testRequire(
		!xrtHttpForwardedElementWrite(
			(const xhttpforwardedvalue*)(
				uintptr_t)(UINTPTR_MAX - 1u),
			NULL, 0, &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(iSize == 77u),
		"Forwarded writer accepted wrapped element storage"
	);

	memset(sOutput, 0xA5, sizeof(sOutput));
	memcpy(sSaved, sOutput, sizeof(sSaved));
	Element.Extensions = (const xhttpforwardedpairvalue*)(
		uintptr_t)(UINTPTR_MAX - 1u);
	Element.ExtensionCount = 1u;
	iSize = 77u;
	xrtClearError();
	testRequire(
		!xrtHttpForwardedElementWrite(
			&Element, sOutput, sizeof(sOutput), &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(iSize == 77u) &&
		(memcmp(sOutput, sSaved, sizeof(sOutput)) == 0),
		"Forwarded writer read a wrapped extension array"
	);

	Extension.Name = (xstrview){
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	Extension.Value = XRT_STR_LITERAL("value");
	Element.Extensions = &Extension;
	iSize = 77u;
	xrtClearError();
	testRequire(
		!xrtHttpForwardedElementWrite(
			&Element, NULL, 0, &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(iSize == 77u),
		"Forwarded writer accepted a wrapped extension name"
	);
	Extension.Name = XRT_STR_LITERAL("trace");
	Extension.Value = (xstrview){
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	iSize = 77u;
	xrtClearError();
	testRequire(
		!xrtHttpForwardedElementWrite(
			&Element, NULL, 0, &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(iSize == 77u),
		"Forwarded writer accepted a wrapped extension value"
	);

	memset(&Element, 0, sizeof(Element));
	Element.Flags = UINT32_MAX;
	iSize = 77u;
	xrtClearError();
	testRequire(
		!xrtHttpForwardedElementWrite(
			&Element,
			(void*)(uintptr_t)(UINTPTR_MAX - 1u),
			4u, &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(iSize == 77u),
		"Forwarded writer checked semantics before output memory"
	);
	xrtClearError();
	testRequire(
		!xrtHttpForwardedElementWrite(
			&Element, NULL, 0,
			(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Forwarded writer checked semantics before size memory"
	);

	Element.For = (xstrview){ sNode, sizeof(sNode) - 1u };
	Element.Flags = XHTTP_FORWARDED_HAS_FOR;
	iSize = 77u;
	xrtClearError();
	testRequire(
		!xrtHttpForwardedElementWrite(
			&Element, sNode, sizeof(sNode), &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(iSize == 77u),
		"Forwarded writer accepted source/output overlap"
	);
	xrtClearError();
	testRequire(
		!xrtHttpForwardedElementWrite(
			&Element, NULL, 0,
			(size_t*)((uint8*)&Element + 1u)
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Forwarded writer accepted descriptor/size overlap"
	);
	xrtClearError();
	testRequire(
		(xrtHttpForwardedBuild(
			&Element, 1u,
			(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
		) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Forwarded build accepted wrapped size memory"
	);
	xrtClearError();
	memcpy(sSaved, sNode, sizeof(sNode));
	testRequire(
		(xrtHttpForwardedBuild(
			&Element, 1u, (size_t*)(void*)sNode
		) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(sNode, sSaved, sizeof(sNode)) == 0),
		"Forwarded build accepted source/size overlap"
	);
	xrtClearError();
}



/* 验证大量扩展参数没有隐藏上限且可由解析器完整还原。 */
static void testForwardedWriteScale(void)
{
	xhttpforwardedpairvalue Extensions[1024];
	xhttpforwardedvalue Element;
	char sNames[1024][8];
	char sOutput[16384];
	xhttpforwarded Parsed;
	size_t iRequired;
	size_t i;
	int iWritten;

	for ( i = 0; i < 1024u; i++ ) {
		iWritten = snprintf(
			sNames[i], sizeof(sNames[i]),
			"x%04u", (unsigned)i
		);
		testRequire(iWritten == 5,
			"Forwarded writer scale name failed");
		Extensions[i].Name = (xstrview){
			sNames[i], (size_t)iWritten
		};
		Extensions[i].Value = XRT_STR_LITERAL("v");
	}
	memset(&Element, 0, sizeof(Element));
	Element.Extensions = Extensions;
	Element.ExtensionCount = 1024u;
	testRequire(
		xrtHttpForwardedElementWrite(
			&Element, NULL, 0, &iRequired
		) && (iRequired <= sizeof(sOutput)) &&
		xrtHttpForwardedElementWrite(
			&Element, sOutput, sizeof(sOutput), &iRequired
		) && xrtHttpForwardedElementParse(
			(xstrview){ sOutput, iRequired }, &Parsed
		) && (Parsed.PairCount == 1024u),
		"Forwarded writer rejected 1024 unique extensions"
	);
}



/* 执行 Forwarded writer 测试。 */
int main(void)
{
	testForwardedWrite();
	testForwardedWriteHostGrammar();
	testForwardedWriteFailure();
	testForwardedWriteMemory();
	testForwardedWriteScale();
	printf("[PASS] http_forwarded_write\n");
	return 0;
}
