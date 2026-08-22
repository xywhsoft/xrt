#include "../test.h"

#include <xrt/http_language.h>



/* 验证 basic language range 和基本语言标签的语法边界。 */
static void testHttpLanguageSyntax(void)
{
	static const xstrview ValidRange[] = {
		XRT_STR_INIT("en"),
		XRT_STR_INIT("zh-Hant"),
		XRT_STR_INIT("de-CH-1996"),
		XRT_STR_INIT("*")
	};
	static const xstrview InvalidRange[] = {
		XRT_STR_INIT(""),
		XRT_STR_INIT("9en"),
		XRT_STR_INIT("en--US"),
		XRT_STR_INIT("en_US"),
		XRT_STR_INIT("en-abcdefghi"),
		XRT_STR_INIT("en-*")
	};
	xstrview Wrapped = {
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	size_t i;

	for ( i = 0; i <
		(sizeof(ValidRange) / sizeof(ValidRange[0])); i++ ) {
		testRequire(
			xrtHttpLanguageRangeValid(ValidRange[i]),
			"valid basic language range was rejected"
		);
	}
	for ( i = 0; i <
		(sizeof(InvalidRange) / sizeof(InvalidRange[0])); i++ ) {
		testRequire(
			!xrtHttpLanguageRangeValid(InvalidRange[i]),
			"invalid basic language range was accepted"
		);
	}
	testRequire(
		xrtHttpLanguageTagValid(
			XRT_STR_LITERAL("zh-Hant-CN-x-private1")
		) && !xrtHttpLanguageTagValid(
			XRT_STR_LITERAL("*")
		),
		"language tag syntax mismatch"
	);
	xrtClearError();
	testRequire(!xrtHttpLanguageTagValid(Wrapped) &&
		(xrtGetError() == NULL),
		"language tag predicate changed the error slot"
	);
}



/* 验证字段值迭代、质量值和失败原子性。 */
static void testHttpLanguageRanges(void)
{
	xstrview List = XRT_STR_LITERAL(
		", da, en-GB;q=0.8, en;q=0.7, *;q=0.1,"
	);
	xhttplanguagerange Range;
	size_t iOffset = 0;
	size_t iSaved;

	testRequire(
		(xrtHttpLanguageRangeNext(
			List, &iOffset, &Range
		) == XHTTP_NEXT_ITEM) &&
		(Range.Quality == XHTTP_QUALITY_MAX) &&
		(Range.SubtagCount == 1u),
		"Accept-Language first range mismatch"
	);
	testRequire(
		(xrtHttpLanguageRangeNext(
			List, &iOffset, &Range
		) == XHTTP_NEXT_ITEM) &&
		(Range.Quality == 800u) &&
		(Range.SubtagCount == 2u),
		"Accept-Language weighted range mismatch"
	);
	testRequire(
		(xrtHttpLanguageRangeNext(
			List, &iOffset, &Range
		) == XHTTP_NEXT_ITEM) &&
		(Range.Quality == 700u),
		"Accept-Language fallback range mismatch"
	);
	testRequire(
		(xrtHttpLanguageRangeNext(
			List, &iOffset, &Range
		) == XHTTP_NEXT_ITEM) &&
		(Range.SubtagCount == 0) &&
		(Range.Quality == 100u),
		"Accept-Language wildcard range mismatch"
	);
	testRequire(
		xrtHttpLanguageRangeNext(
			List, &iOffset, &Range
		) == XHTTP_NEXT_END,
		"Accept-Language list did not end"
	);
	iOffset = 0;
	iSaved = iOffset;
	memset(&Range, 0xA5, sizeof(Range));
	testRequire(
		xrtHttpLanguageRangeNext(
			XRT_STR_LITERAL("en-*"),
			&iOffset,
			&Range
		) == XHTTP_NEXT_ERROR,
		"Accept-Language accepted an extended range"
	);
	testRequire(
		(iOffset == iSaved) &&
		(Range.Range.Data == NULL) &&
		(Range.Range.Size == 0),
		"Accept-Language failure changed outputs"
	);
	xrtClearError();
}



/* 验证畸形字段、终态游标、未对齐输出和别名边界。 */
static void testHttpLanguageMemoryEdges(void)
{
	static const xhttpfield Malformed[] = {
		{
			XRT_STR_INIT("Accept-Language"),
			XRT_STR_INIT("en;q=0.5;foo=1")
		}
	};
	static const xhttpfield LateMalformed[] = {
		{
			XRT_STR_INIT("Accept-Language"),
			XRT_STR_INIT("en;q=0.8")
		},
		{
			XRT_STR_INIT("Accept-Language"),
			XRT_STR_INIT("fr;q=0.5;foo=1")
		}
	};
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttplanguagematch) + 1u];
	} MatchStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xstrview) * 2u + 1u];
	} TagStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(size_t) + 1u];
	} IndexStorage;
	xhttplanguagematch* pMatch = (xhttplanguagematch*)(
		MatchStorage.Bytes + 1u
	);
	xstrview* pTags = (xstrview*)(TagStorage.Bytes + 1u);
	size_t* pIndex = (size_t*)(IndexStorage.Bytes + 1u);
	static const xstrview Tags[] = {
		XRT_STR_INIT("en-US"),
		XRT_STR_INIT("fr")
	};
	xstrview Wrapped = {
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	xhttplanguagematch Match;
	xhttplanguagecursor Cursor;
	xhttplanguagerange Range;
	size_t iIndex;
	size_t iOffset;

	testRequire(
		xrtHttpAcceptLanguageMatch(
			NULL,
			0,
			XRT_STR_LITERAL("en-US"),
			pMatch
		),
		"Accept-Language rejected unaligned match output"
	);
	memcpy(&Match, pMatch, sizeof(Match));
	testRequire(
		Match.Quality == XHTTP_QUALITY_MAX,
		"Accept-Language unaligned match mismatch"
	);
	testRequire(
		!xrtHttpAcceptLanguageMatch(
			Malformed,
			1,
			XRT_STR_LITERAL("en"),
			pMatch
		),
		"Accept-Language accepted an extension parameter"
	);
	xrtClearError();
	testRequire(
		!xrtHttpAcceptLanguageMatch(
			LateMalformed,
			2,
			XRT_STR_LITERAL("en"),
			pMatch
		),
		"Accept-Language ignored a malformed trailing field"
	);
	xrtClearError();
	Cursor.Field = 1u;
	Cursor.Offset = 1u;
	testRequire(
		xrtHttpAcceptLanguageNext(
			Malformed, 1, &Cursor, &Range
		) == XHTTP_NEXT_ERROR,
		"Accept-Language accepted an invalid terminal cursor"
	);
	xrtClearError();

	/* 包装输入地址必须在任何字节访问前被拒绝。 */
	iOffset = 0;
	testRequire(
		xrtHttpLanguageRangeNext(
			Wrapped, &iOffset, &Range
		) == XHTTP_NEXT_ERROR,
		"Accept-Language accepted a wrapped list range"
	);
	testRequire(
		iOffset == 0,
		"Accept-Language wrapped list advanced the cursor"
	);
	xrtClearError();

	/* 可用标签描述符和结果索引均允许位于打包存储中。 */
	memcpy(pTags, Tags, sizeof(Tags));
	testRequire(
		xrtHttpAcceptLanguageSelect(
			LateMalformed,
			1,
			pTags,
			2,
			pIndex
		) == XHTTP_NEXT_ITEM,
		"Accept-Language rejected unaligned tag storage"
	);
	memcpy(&iIndex, pIndex, sizeof(iIndex));
	testRequire(
		iIndex == 0,
		"Accept-Language unaligned selection mismatch"
	);
	testRequire(
		xrtHttpAcceptLanguageSelect(
			LateMalformed,
			1,
			(const xstrview*)(uintptr_t)UINTPTR_MAX,
			1,
			&iIndex
		) == XHTTP_NEXT_ERROR,
		"Accept-Language accepted a wrapped tag array"
	);
	xrtClearError();
}



/* 验证 RFC 4647 Basic Filtering 的前缀与分段边界。 */
static void testHttpLanguageBasic(void)
{
	testRequire(
		xrtHttpLanguageBasicMatch(
			XRT_STR_LITERAL("de-DE"),
			XRT_STR_LITERAL("de-de-1996")
		) == XHTTP_NEXT_ITEM,
		"basic filtering rejected a valid prefix"
	);
	testRequire(
		xrtHttpLanguageBasicMatch(
			XRT_STR_LITERAL("de-DE"),
			XRT_STR_LITERAL("de-Deva")
		) == XHTTP_NEXT_END,
		"basic filtering ignored a subtag boundary"
	);
	testRequire(
		xrtHttpLanguageBasicMatch(
			XRT_STR_LITERAL("*"),
			XRT_STR_LITERAL("fr-CH")
		) == XHTTP_NEXT_ITEM,
		"basic filtering wildcard mismatch"
	);
}



/* 验证具体范围覆盖通配符，缺失与空字段语义保持不同。 */
static void testHttpAcceptLanguageMatch(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Accept-Language"),
			XRT_STR_INIT("*;q=0.1, en;q=0.5")
		},
		{
			XRT_STR_INIT("accept-language"),
			XRT_STR_INIT("en-US;q=0.9")
		}
	};
	static const xhttpfield Empty[] = {
		{
			XRT_STR_INIT("Accept-Language"),
			XRT_STR_INIT("")
		}
	};
	xhttplanguagematch Match;

	testRequire(
		xrtHttpAcceptLanguageQuality(
			Fields, 2, XRT_STR_LITERAL("en-US")
		) == 900u,
		"Accept-Language exact quality mismatch"
	);
	testRequire(
		xrtHttpAcceptLanguageQuality(
			Fields, 2, XRT_STR_LITERAL("en-GB")
		) == 500u,
		"Accept-Language prefix quality mismatch"
	);
	testRequire(
		xrtHttpAcceptLanguageQuality(
			Fields, 2, XRT_STR_LITERAL("fr")
		) == 100u,
		"Accept-Language wildcard quality mismatch"
	);
	testRequire(
		xrtHttpAcceptLanguageMatch(
			NULL,
			0,
			XRT_STR_LITERAL("fr"),
			&Match
		) && (Match.Quality == XHTTP_QUALITY_MAX) &&
		(Match.Field == XRT_NPOS),
		"missing Accept-Language default mismatch"
	);
	testRequire(
		xrtHttpAcceptLanguageMatch(
			Empty,
			1,
			XRT_STR_LITERAL("fr"),
			&Match
		) && (Match.Quality == 0) &&
		(Match.Field == XRT_NPOS),
		"empty Accept-Language field default mismatch"
	);
}



/* 验证 Basic Filtering 选择按质量值和服务端顺序稳定决策。 */
static void testHttpAcceptLanguageSelect(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Accept-Language"),
			XRT_STR_INIT("fr;q=0.8, en;q=0.8, de;q=0")
		}
	};
	static const xstrview Available[] = {
		XRT_STR_INIT("en-US"),
		XRT_STR_INIT("fr-CH"),
		XRT_STR_INIT("de-DE")
	};
	size_t iIndex;

	testRequire(
		(xrtHttpAcceptLanguageSelect(
			Fields, 1, Available, 3, &iIndex
		) == XHTTP_NEXT_ITEM) && (iIndex == 0),
		"Accept-Language selection order mismatch"
	);
}



/* 验证 Lookup 逐级回退、singleton 移除、质量优先和通配符默认语义。 */
static void testHttpAcceptLanguageLookup(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Accept-Language"),
			XRT_STR_INIT(
				"zh-Hant-CN-x-private1-private2;q=0.9, "
				"fr-FR;q=0.8"
			)
		}
	};
	static const xstrview Available[] = {
		XRT_STR_INIT("fr"),
		XRT_STR_INIT("zh"),
		XRT_STR_INIT("zh-Hant"),
		XRT_STR_INIT("zh-Hant-CN")
	};
	static const xhttpfield ExactOnly[] = {
		{
			XRT_STR_INIT("Accept-Language"),
			XRT_STR_INIT("de-CH")
		}
	};
	static const xstrview ExactAvailable[] = {
		XRT_STR_INIT("de-CH-1996"),
		XRT_STR_INIT("de")
	};
	static const xhttpfield Wildcard[] = {
		{
			XRT_STR_INIT("Accept-Language"),
			XRT_STR_INIT("*")
		}
	};
	size_t iIndex;

	testRequire(
		(xrtHttpAcceptLanguageLookup(
			Fields, 1, Available, 4, &iIndex
		) == XHTTP_NEXT_ITEM) && (iIndex == 3u),
		"Accept-Language singleton lookup mismatch"
	);
	testRequire(
		(xrtHttpAcceptLanguageLookup(
			ExactOnly, 1, ExactAvailable, 2, &iIndex
		) == XHTTP_NEXT_ITEM) && (iIndex == 1u),
		"Accept-Language lookup selected a more specific tag"
	);
	testRequire(
		(xrtHttpAcceptLanguageLookup(
			Wildcard, 1, Available, 4, &iIndex
		) == XHTTP_NEXT_END) && (iIndex == XRT_NPOS),
		"Accept-Language lookup used wildcard as an implicit default"
	);
	testRequire(
		(xrtHttpAcceptLanguageLookup(
			NULL, 0, Available, 4, &iIndex
		) == XHTTP_NEXT_ITEM) && (iIndex == 0),
		"missing Accept-Language lookup default mismatch"
	);
}



/* 运行 HTTP 语言协商测试。 */
int main(void)
{
	testHttpLanguageSyntax();
	testHttpLanguageRanges();
	testHttpLanguageMemoryEdges();
	testHttpLanguageBasic();
	testHttpAcceptLanguageMatch();
	testHttpAcceptLanguageSelect();
	testHttpAcceptLanguageLookup();
	printf("[PASS] http_language\n");
	return 0;
}
