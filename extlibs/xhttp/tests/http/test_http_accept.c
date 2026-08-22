#include "../test.h"

#include <xrt/http_accept.h>



/* 验证媒体范围保留完整参数，并把任意位置的 q 独立为权重。 */
static void testHttpAcceptRanges(void)
{
	xstrview List = XRT_STR_LITERAL(
		", text/*; level=1; q=0.3; foo=\"a,b\", "
		"application/json; charset=UTF-8, */*;q=0.1,"
	);
	xhttpmediarange Range;
	xhttpparam Param;
	size_t iOffset = 0;
	size_t iParamOffset = 0;

	testRequire(
		(xrtHttpMediaRangeNext(
			List, &iOffset, &Range
		) == XHTTP_NEXT_ITEM) &&
		(Range.Specificity == XHTTP_MEDIA_RANGE_TYPE) &&
		xrtHttpTokenEqual(
			Range.Type, XRT_STR_LITERAL("text")
		) && (Range.Subtype.Size == 1u) &&
		(Range.Subtype.Data[0] == '*') &&
		(Range.ParameterCount == 2u) &&
		(Range.Quality == 300u) &&
		(xrtHttpMediaRangeParamNext(
			&Range, &iParamOffset, &Param
		) == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(
			Param.Name, XRT_STR_LITERAL("level")
		) && (xrtHttpMediaRangeParamNext(
			&Range, &iParamOffset, &Param
		) == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(
			Param.Name, XRT_STR_LITERAL("foo")
		) && (xrtHttpMediaRangeParamNext(
			&Range, &iParamOffset, &Param
		) == XHTTP_NEXT_END),
		"Accept media range parameter iteration mismatch"
	);
	testRequire(
		(xrtHttpMediaRangeNext(
			List, &iOffset, &Range
		) == XHTTP_NEXT_ITEM) &&
		(Range.Specificity == XHTTP_MEDIA_RANGE_EXACT) &&
		(Range.ParameterCount == 1u) &&
		(Range.Quality == XHTTP_QUALITY_MAX) &&
		xrtHttpTokenEqual(
			Range.Subtype, XRT_STR_LITERAL("json")
		),
		"Accept exact media range mismatch"
	);
	testRequire(
		(xrtHttpMediaRangeNext(
			List, &iOffset, &Range
		) == XHTTP_NEXT_ITEM) &&
		(Range.Specificity == XHTTP_MEDIA_RANGE_ANY) &&
		(Range.Quality == 100u),
		"Accept wildcard media range mismatch"
	);
	testRequire(
		xrtHttpMediaRangeNext(
			List, &iOffset, &Range
		) == XHTTP_NEXT_END,
		"Accept media range list did not end"
	);
}



/* 验证非法通配符、参数、质量值和 quoted-string 失败原子性。 */
static void testHttpAcceptRangeErrors(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT("*/json"),
		XRT_STR_INIT("text/plain; p"),
		XRT_STR_INIT("text/plain; p=1; P=2"),
		XRT_STR_INIT("text/plain; q=\"0.5\""),
		XRT_STR_INIT("text/plain; q=.5"),
		XRT_STR_INIT("text/plain; q=0.5; Q=0.2"),
		XRT_STR_INIT("text/plain; q=0.5; x"),
		XRT_STR_INIT("text/plain; p=1; q=0.5; P=2"),
		XRT_STR_INIT("text/plain; q=0.5; x=\"unterminated")
	};
	xhttpmediarange Range;
	size_t iOffset;
	size_t i;

	for ( i = 0; i <
		(sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		memset(&Range, 0xA5, sizeof(Range));
		iOffset = 0;
		testRequire(
			xrtHttpMediaRangeNext(
				Invalid[i], &iOffset, &Range
			) == XHTTP_NEXT_ERROR,
			"Accept accepted malformed media range"
		);
		testRequire(
			(iOffset == 0) &&
			(Range.Type.Data == NULL) &&
			(Range.Type.Size == 0),
			"Accept range failure changed outputs"
		);
		xrtClearError();
	}
}



/* 验证重复 Accept 字段按线路顺序迭代。 */
static void testHttpAcceptFields(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Host"),
			XRT_STR_INIT("example.test")
		},
		{
			XRT_STR_INIT("Accept"),
			XRT_STR_INIT("text/html;q=0.8")
		},
		{
			XRT_STR_INIT("accept"),
			XRT_STR_INIT("application/json")
		}
	};
	xhttpacceptcursor Cursor;
	xhttpmediarange Range;

	xrtHttpAcceptCursorInit(&Cursor);
	testRequire(
		(xrtHttpAcceptNext(
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			&Cursor,
			&Range
		) == XHTTP_NEXT_ITEM) &&
		(Cursor.Field == 1u) &&
		(Range.Quality == 800u),
		"Accept repeated field first item mismatch"
	);
	testRequire(
		(xrtHttpAcceptNext(
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			&Cursor,
			&Range
		) == XHTTP_NEXT_ITEM) &&
		(Cursor.Field == 2u) &&
		xrtHttpTokenEqual(
			Range.Subtype, XRT_STR_LITERAL("json")
		),
		"Accept repeated field second item mismatch"
	);
	testRequire(
		xrtHttpAcceptNext(
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			&Cursor,
			&Range
		) == XHTTP_NEXT_END,
		"Accept repeated fields did not end"
	);
	Cursor.Field = sizeof(Fields) / sizeof(Fields[0]);
	Cursor.Offset = 1u;
	testRequire(
		xrtHttpAcceptNext(
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			&Cursor,
			&Range
		) == XHTTP_NEXT_ERROR,
		"Accept accepted an invalid terminal cursor"
	);
	xrtClearError();
}



/* 验证 RFC 具体度优先规则、媒体参数和缺失字段默认值。 */
static void testHttpAcceptMatch(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Accept"),
			XRT_STR_INIT(
				"text/*;q=0.3, text/plain;q=0.7, "
				"text/plain;format=flowed, */*;q=0.5"
			)
		}
	};
	static const xhttpfield Charset[] = {
		{
			XRT_STR_INIT("Accept"),
			XRT_STR_INIT(
				"text/plain;charset=UTF-8;q=0.9, "
				"text/plain;q=0.2"
			)
		}
	};
	static const xhttpfield QualityMiddle[] = {
		{
			XRT_STR_INIT("Accept"),
			XRT_STR_INIT(
				"text/plain;q=0.9;format=flowed, "
				"text/plain;q=0.2"
			)
		}
	};
	xhttpacceptmatch Match;

	testRequire(
		xrtHttpAcceptQuality(
			Fields, 1, XRT_STR_LITERAL("text/html")
		) == 300u,
		"Accept type wildcard quality mismatch"
	);
	testRequire(
		xrtHttpAcceptQuality(
			Fields, 1, XRT_STR_LITERAL("image/jpeg")
		) == 500u,
		"Accept any wildcard quality mismatch"
	);
	testRequire(
		xrtHttpAcceptQuality(
			Fields, 1, XRT_STR_LITERAL("text/plain")
		) == 700u,
		"Accept exact quality mismatch"
	);
	testRequire(
		xrtHttpAcceptMatch(
			Fields,
			1,
			XRT_STR_LITERAL("text/plain; format=flowed"),
			&Match
		) && (Match.Quality == XHTTP_QUALITY_MAX) &&
		(Match.ParameterCount == 1u) &&
		(Match.Specificity == XHTTP_MEDIA_RANGE_EXACT),
		"Accept parameter precedence mismatch"
	);
	testRequire(
		xrtHttpAcceptQuality(
			Charset,
			1,
			XRT_STR_LITERAL("text/plain;charset=utf-8")
		) == 900u,
		"Accept charset comparison was case-sensitive"
	);
	testRequire(
		xrtHttpAcceptQuality(
			QualityMiddle,
			1,
			XRT_STR_LITERAL("text/plain;format=flowed")
		) == 900u,
		"Accept ignored a media parameter after q"
	);
	testRequire(
		xrtHttpAcceptQuality(
			QualityMiddle,
			1,
			XRT_STR_LITERAL("text/plain")
		) == 200u,
		"Accept treated a parameter after q as an extension"
	);
	testRequire(
		xrtHttpAcceptMatch(
			NULL,
			0,
			XRT_STR_LITERAL("application/json"),
			&Match
		) && (Match.Quality == XHTTP_QUALITY_MAX) &&
		(Match.Field == XRT_NPOS),
		"missing Accept field default mismatch"
	);
}



/* 验证空字段、同具体度首项优先和服务端偏好稳定选择。 */
static void testHttpAcceptSelect(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Accept"),
			XRT_STR_INIT(
				"text/html;q=0.8, application/json;q=0.8, "
				"image/*;q=0"
			)
		}
	};
	static const xhttpfield Duplicate[] = {
		{
			XRT_STR_INIT("Accept"),
			XRT_STR_INIT("text/plain;q=0.2, text/plain;q=0.9")
		}
	};
	static const xhttpfield Empty[] = {
		{
			XRT_STR_INIT("Accept"),
			XRT_STR_INIT("")
		}
	};
	static const xstrview Available[] = {
		XRT_STR_INIT("application/json"),
		XRT_STR_INIT("text/html"),
		XRT_STR_INIT("image/png")
	};
	size_t iIndex;

	testRequire(
		(xrtHttpAcceptSelect(
			Fields,
			1,
			Available,
			sizeof(Available) / sizeof(Available[0]),
			&iIndex
		) == XHTTP_NEXT_ITEM) && (iIndex == 0),
		"Accept selection did not preserve server preference"
	);
	testRequire(
		xrtHttpAcceptQuality(
			Duplicate, 1, XRT_STR_LITERAL("text/plain")
		) == 200u,
		"Accept equal-specificity order mismatch"
	);
	testRequire(
		(xrtHttpAcceptSelect(
			Empty,
			1,
			Available,
			sizeof(Available) / sizeof(Available[0]),
			&iIndex
		) == XHTTP_NEXT_END) && (iIndex == XRT_NPOS),
		"empty Accept field selected a representation"
	);
}



/* 验证固定描述符、游标、候选数组及其借用范围的完整内存契约。 */
static void testHttpAcceptMemoryEdges(void)
{
	static const xhttpfield LateMalformed[] = {
		{
			XRT_STR_INIT("Accept"),
			XRT_STR_INIT("application/json")
		},
		{
			XRT_STR_INIT("Accept"),
			XRT_STR_INIT("text/plain;q=0.5;broken")
		}
	};
	static const xstrview Available[] = {
		XRT_STR_INIT("application/json"),
		XRT_STR_INIT("text/plain")
	};
	static const xstrview InvalidAvailable[] = {
		XRT_STR_INIT("application/json"),
		XRT_STR_INIT("text/")
	};
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpacceptmatch) + 1u];
	} MatchStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpmediarange) + 1u];
	} RangeStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpparam) + 1u];
	} ParamStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(size_t) + 1u];
	} OffsetStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(Available) + 1u];
	} MediaStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(size_t) + 1u];
	} IndexStorage;
	xhttpacceptmatch Match;
	xhttpparam Param;
	xhttpmediarange Range;
	xmediatype Type;
	xhttpmediarange* pRange = (xhttpmediarange*)(
		RangeStorage.Bytes + 1u
	);
	xhttpmediarange* pOverlap = (xhttpmediarange*)
		MatchStorage.Bytes;
	xhttpacceptmatch* pMatch = (xhttpacceptmatch*)(
		MatchStorage.Bytes + 1u
	);
	xhttpparam* pParam = (xhttpparam*)(ParamStorage.Bytes + 1u);
	size_t* pParamOffset = (size_t*)(OffsetStorage.Bytes + 1u);
	xstrview* pMediaTypes = (xstrview*)(MediaStorage.Bytes + 1u);
	size_t* pIndex = (size_t*)(IndexStorage.Bytes + 1u);
	xstrview List = XRT_STR_LITERAL(
		"application/json;q=0.5;charset=UTF-8"
	);
	xstrview Wrapped = {
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	size_t iIndex;
	size_t iOffset = 0;

	testRequire(
		(xrtHttpMediaRangeNext(
			List, &iOffset, pRange
		) == XHTTP_NEXT_ITEM),
		"Accept range rejected unaligned output"
	);
	iOffset = 0;
	memcpy(pParamOffset, &iOffset, sizeof(iOffset));
	testRequire(
		(xrtHttpMediaRangeParamNext(
			pRange, pParamOffset, pParam
		) == XHTTP_NEXT_ITEM),
		"Accept parameter rejected unaligned storage"
	);
	memcpy(&Param, pParam, sizeof(Param));
	testRequire(
		xrtHttpTokenEqual(
			Param.Name, XRT_STR_LITERAL("charset")
		),
		"Accept parameter iterator did not skip q"
	);
	testRequire(
		(xrtHttpMediaRangeParamNext(
			pRange, pParamOffset, pParam
		) == XHTTP_NEXT_END),
		"Accept parameter iterator did not end"
	);
	memcpy(&Param, pParam, sizeof(Param));
	testRequire(
		(Param.Name.Data == NULL) && (Param.Name.Size == 0),
		"Accept parameter end did not clear the result"
	);

	/* 公开结构被篡改后不能绕过解析器不变量。 */
	testRequire(
		xrtHttpMediaTypeParse(
			XRT_STR_LITERAL("application/json;charset=UTF-8"),
			&Type
		) && (xrtHttpMediaRangeMatch(
			pRange, &Type
		) == XHTTP_NEXT_ITEM),
		"Accept rejected a valid public range descriptor"
	);
	memcpy(&Range, pRange, sizeof(Range));
	Range.Quality = 700u;
	testRequire(
		xrtHttpMediaRangeMatch(
			&Range, &Type
		) == XHTTP_NEXT_ERROR,
		"Accept accepted a range with inconsistent quality"
	);
	xrtClearError();
	memcpy(&Range, pRange, sizeof(Range));
	Range.ParameterCount = 0;
	testRequire(
		xrtHttpMediaRangeMatch(
			&Range, &Type
		) == XHTTP_NEXT_ERROR,
		"Accept accepted a range with inconsistent parameter count"
	);
	xrtClearError();
	testRequire(
		xrtHttpAcceptMatch(
			NULL,
			0,
			XRT_STR_LITERAL("application/json"),
			pMatch
		),
		"Accept match rejected unaligned output"
	);
	memcpy(&Match, pMatch, sizeof(Match));
	testRequire(
		Match.Quality == XHTTP_QUALITY_MAX,
		"Accept unaligned match output mismatch"
	);
	iOffset = 0;
	testRequire(
		xrtHttpMediaRangeNext(
			(xstrview){
				(char*)&MatchStorage,
				sizeof(MatchStorage)
			},
			&iOffset,
			pOverlap
		) == XHTTP_NEXT_ERROR,
		"Accept range accepted overlapping output"
	);
	xrtClearError();

	/* 回绕输入必须在读取任何字节前失败并保持游标。 */
	iOffset = 0;
	testRequire(
		(xrtHttpMediaRangeNext(
			Wrapped, &iOffset, pRange
		) == XHTTP_NEXT_ERROR) && (iOffset == 0),
		"Accept accepted a wrapped list range"
	);
	xrtClearError();
	testRequire(
		xrtHttpMediaRangeParamNext(
			(const xhttpmediarange*)(uintptr_t)UINTPTR_MAX,
			&iOffset,
			&Param
		) == XHTTP_NEXT_ERROR,
		"Accept accepted a wrapped range descriptor"
	);
	xrtClearError();

	/* 候选描述符和结果索引支持打包存储。 */
	memcpy(pMediaTypes, Available, sizeof(Available));
	testRequire(
		(xrtHttpAcceptSelect(
			NULL, 0, pMediaTypes, 2, pIndex
		) == XHTTP_NEXT_ITEM),
		"Accept selection rejected unaligned storage"
	);
	memcpy(&iIndex, pIndex, sizeof(iIndex));
	testRequire(
		iIndex == 0,
		"Accept unaligned selection mismatch"
	);
	iIndex = 73u;
	testRequire(
		(xrtHttpAcceptSelect(
			NULL,
			0,
			InvalidAvailable,
			2,
			&iIndex
		) == XHTTP_NEXT_ERROR) && (iIndex == 73u),
		"Accept malformed late candidate changed the index"
	);
	xrtClearError();
	testRequire(
		xrtHttpAcceptSelect(
			NULL,
			0,
			(const xstrview*)(uintptr_t)UINTPTR_MAX,
			1,
			&iIndex
		) == XHTTP_NEXT_ERROR,
		"Accept accepted a wrapped candidate array"
	);
	xrtClearError();

	/* 后续重复字段畸形时，早期匹配不能掩盖协议错误。 */
	testRequire(
		!xrtHttpAcceptMatch(
			LateMalformed,
			2,
			XRT_STR_LITERAL("application/json"),
			&Match
		),
		"Accept ignored a malformed trailing field"
	);
	xrtClearError();
}



/* 运行 HTTP Accept 内容协商测试。 */
int main(void)
{
	testHttpAcceptRanges();
	testHttpAcceptRangeErrors();
	testHttpAcceptFields();
	testHttpAcceptMatch();
	testHttpAcceptSelect();
	testHttpAcceptMemoryEdges();
	printf("[PASS] http_accept\n");
	return 0;
}
