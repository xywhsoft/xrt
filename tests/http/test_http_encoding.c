#include "../test.h"

#include <xrt/http_encoding.h>



/* 验证 Header 缺失、空值和显式 identity 的 RFC 默认值。 */
static void testHttpEncodingDefaults(void)
{
	xhttpacceptencoding Accept;

	xrtHttpAcceptEncodingInit(&Accept);
	testRequire(
		xrtHttpAcceptEncodingValid(&Accept) &&
		((Accept.Flags & XHTTP_ACCEPT_ENCODING_PRESENT) == 0) &&
		(xrtHttpAcceptEncodingQuality(
			&Accept, XHTTP_CODING_GZIP
		 ) == XHTTP_QUALITY_MAX) &&
		(xrtHttpAcceptEncodingQuality(
			&Accept, XHTTP_CODING_DEFLATE
		 ) == XHTTP_QUALITY_MAX) &&
		(xrtHttpAcceptEncodingQuality(
			&Accept, XHTTP_CODING_IDENTITY
		 ) == XHTTP_QUALITY_MAX) &&
		(xrtHttpAcceptEncodingSelect(
			&Accept,
			XHTTP_CODING_IDENTITY |
				XHTTP_CODING_GZIP |
				XHTTP_CODING_DEFLATE,
			XHTTP_CODING_GZIP
		 ) == XHTTP_CODING_GZIP),
		"missing Accept-Encoding defaults mismatch"
	);
	testRequire(
		xrtHttpAcceptEncodingAdd(
			&Accept, XRT_STR_LITERAL("")
		) &&
		((Accept.Flags & XHTTP_ACCEPT_ENCODING_PRESENT) != 0) &&
		(xrtHttpAcceptEncodingQuality(
			&Accept, XHTTP_CODING_GZIP
		 ) == 0) &&
		(xrtHttpAcceptEncodingQuality(
			&Accept, XHTTP_CODING_IDENTITY
		 ) == XHTTP_QUALITY_MAX) &&
		(xrtHttpAcceptEncodingSelect(
			&Accept,
			XHTTP_CODING_IDENTITY |
				XHTTP_CODING_GZIP,
			XHTTP_CODING_GZIP
		 ) == XHTTP_CODING_IDENTITY),
		"empty Accept-Encoding did not select identity"
	);
	Accept.Flags = UINT32_MAX;
	testRequire(
		!xrtHttpAcceptEncodingValid(&Accept),
		"Accept-Encoding validator accepted unknown flags"
	);
	Accept.Flags = XHTTP_ACCEPT_ENCODING_PRESENT;
	Accept.Gzip = XHTTP_QUALITY_MAX + 1u;
	testRequire(
		!xrtHttpAcceptEncodingValid(&Accept),
		"Accept-Encoding validator accepted invalid quality"
	);
	xrtHttpAcceptEncodingInit(&Accept);
	testRequire(
		xrtHttpAcceptEncodingAdd(
			&Accept, XRT_STR_LITERAL("gzip")
		) &&
		(xrtHttpAcceptEncodingSelect(
			&Accept,
			XHTTP_CODING_IDENTITY |
				XHTTP_CODING_GZIP,
			XHTTP_CODING_GZIP
		 ) == XHTTP_CODING_GZIP),
		"explicit gzip tie did not honor preferred coding"
	);
}



/* 验证旧版协商矩阵和 wildcard 的显式覆盖规则。 */
static void testHttpEncodingSelection(void)
{
	xhttpacceptencoding Accept;

	xrtHttpAcceptEncodingInit(&Accept);
	testRequire(
		xrtHttpAcceptEncodingAdd(
			&Accept,
			XRT_STR_LITERAL(
				"gzip;q=0.2, deflate;q=1"
			)
		) &&
		(xrtHttpAcceptEncodingSelect(
			&Accept,
			XHTTP_CODING_IDENTITY |
				XHTTP_CODING_GZIP |
				XHTTP_CODING_DEFLATE,
			XHTTP_CODING_GZIP
		 ) == XHTTP_CODING_DEFLATE),
		"Accept-Encoding qvalue preference mismatch"
	);
	xrtHttpAcceptEncodingInit(&Accept);
	testRequire(
		xrtHttpAcceptEncodingAdd(
			&Accept,
			XRT_STR_LITERAL(
				"*;q=0.5, identity;q=0.1"
			)
		) &&
		(xrtHttpAcceptEncodingSelect(
			&Accept,
			XHTTP_CODING_IDENTITY |
				XHTTP_CODING_GZIP |
				XHTTP_CODING_DEFLATE,
			XHTTP_CODING_GZIP
		 ) == XHTTP_CODING_GZIP),
		"Accept-Encoding wildcard selection mismatch"
	);
	xrtHttpAcceptEncodingInit(&Accept);
	testRequire(
		xrtHttpAcceptEncodingAdd(
			&Accept,
			XRT_STR_LITERAL(
				"gzip;q=0.2, identity;q=1"
			)
		) &&
		(xrtHttpAcceptEncodingSelect(
			&Accept,
			XHTTP_CODING_IDENTITY |
				XHTTP_CODING_GZIP,
			XHTTP_CODING_GZIP
		 ) == XHTTP_CODING_IDENTITY),
		"Accept-Encoding identity preference mismatch"
	);
	xrtHttpAcceptEncodingInit(&Accept);
	testRequire(
		xrtHttpAcceptEncodingAdd(
			&Accept,
			XRT_STR_LITERAL(
				"gzip;q=0, deflate;q=0, identity;q=0"
			)
		) &&
		(xrtHttpAcceptEncodingSelect(
			&Accept,
			XHTTP_CODING_IDENTITY |
				XHTTP_CODING_GZIP |
				XHTTP_CODING_DEFLATE,
			XHTTP_CODING_GZIP
		 ) == XHTTP_CODING_NONE),
		"Accept-Encoding unacceptable set mismatch"
	);
	xrtHttpAcceptEncodingInit(&Accept);
	testRequire(
		xrtHttpAcceptEncodingAdd(
			&Accept,
			XRT_STR_LITERAL(
				"gzip;q=0, *;q=0.5, identity;q=0.25"
			)
		) &&
		(xrtHttpAcceptEncodingQuality(
			&Accept, XHTTP_CODING_GZIP
		 ) == 0) &&
		(xrtHttpAcceptEncodingQuality(
			&Accept, XHTTP_CODING_DEFLATE
		 ) == 500) &&
		(xrtHttpAcceptEncodingQuality(
			&Accept, XHTTP_CODING_IDENTITY
		 ) == 250),
		"Accept-Encoding explicit wildcard override mismatch"
	);
	xrtHttpAcceptEncodingInit(&Accept);
	testRequire(
		xrtHttpAcceptEncodingAdd(
			&Accept, XRT_STR_LITERAL("*;q=0")
		) &&
		(xrtHttpAcceptEncodingQuality(
			&Accept, XHTTP_CODING_IDENTITY
		 ) == 0),
		"Accept-Encoding wildcard identity exclusion mismatch"
	);
}



/* 验证重复字段合并、未知编码和失败原子性。 */
static void testHttpEncodingFields(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Host"),
			XRT_STR_INIT("example.test")
		},
		{
			XRT_STR_INIT("Accept-Encoding"),
			XRT_STR_INIT("br, gzip;q=0.2")
		},
		{
			XRT_STR_INIT("accept-encoding"),
			XRT_STR_INIT("gzip;q=0.8, deflate;q=0.4")
		}
	};
	static const xhttpfield Invalid[] = {
		{
			XRT_STR_INIT("Accept-Encoding"),
			XRT_STR_INIT("gzip")
		},
		{
			XRT_STR_INIT("accept-encoding"),
			XRT_STR_INIT("deflate;q=1.0000")
		}
	};
	xhttpacceptencoding Accept;
	xhttpacceptencoding Before;

	testRequire(
		xrtHttpAcceptEncodingParse(
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			&Accept
		) &&
		(Accept.Gzip == 800) &&
		(Accept.Deflate == 400) &&
		(xrtHttpAcceptEncodingSelect(
			&Accept,
			XHTTP_CODING_IDENTITY |
				XHTTP_CODING_GZIP,
			XHTTP_CODING_GZIP
		 ) == XHTTP_CODING_IDENTITY),
		"Accept-Encoding repeated field merge mismatch"
	);
	Before = Accept;
	testRequire(
		!xrtHttpAcceptEncodingAdd(
			&Accept,
			XRT_STR_LITERAL("gzip;q=1.0000")
		) &&
		(memcmp(&Accept, &Before, sizeof(Accept)) == 0),
		"Accept-Encoding malformed add changed prior state"
	);
	xrtClearError();
	Before = Accept;
	testRequire(
		!xrtHttpAcceptEncodingParse(
			Invalid,
			sizeof(Invalid) / sizeof(Invalid[0]),
			&Accept
		) &&
		(memcmp(&Accept, &Before, sizeof(Accept)) == 0),
		"Accept-Encoding malformed field set changed output"
	);
	xrtClearError();
	testRequire(
		xrtHttpAcceptEncodingParse(NULL, 0, &Accept) &&
		((Accept.Flags & XHTTP_ACCEPT_ENCODING_PRESENT) == 0),
		"Accept-Encoding absent field parse mismatch"
	);
	testRequire(
		xrtHttpCodingName(XHTTP_CODING_GZIP).Size == 4u &&
		(xrtHttpCodingParse(
			XRT_STR_LITERAL("GZIP")
		 ) == XHTTP_CODING_GZIP) &&
		(xrtHttpCodingParse(
			XRT_STR_LITERAL("x-gzip")
		 ) == XHTTP_CODING_GZIP) &&
		(xrtHttpCodingParse(
			XRT_STR_LITERAL("br")
		 ) == XHTTP_CODING_NONE) &&
		xrtHttpTokenEqual(
			xrtHttpCodingName(XHTTP_CODING_DEFLATE),
			XRT_STR_LITERAL("deflate")
		) &&
		(xrtHttpCodingName(XHTTP_CODING_NONE).Size == 0),
		"HTTP coding token mapping mismatch"
	);
}



/* 验证重复 Content-Encoding 字段形成可扩展、无分配的有序计划。 */
static void testHttpContentEncodingPlan(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Host"),
			XRT_STR_INIT("example.test")
		},
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("gzip, identity")
		},
		{
			XRT_STR_INIT("content-encoding"),
			XRT_STR_INIT("")
		},
		{
			XRT_STR_INIT("CONTENT-ENCODING"),
			XRT_STR_INIT("deflate, x-gzip, br")
		}
	};
	static const xhttpcoding Expected[] = {
		XHTTP_CODING_GZIP,
		XHTTP_CODING_IDENTITY,
		XHTTP_CODING_DEFLATE,
		XHTTP_CODING_GZIP,
		XHTTP_CODING_NONE
	};
	static const char Joined[] =
		"gzip, identity, , deflate, x-gzip, br";
	xhttpcontentencodingplan Plan;
	xhttpcontentencodingcursor Cursor;
	xhttpcontentencodingitem Item;
	xhttpnext Next;
	char Output[sizeof(Joined)];
	size_t iSize;
	size_t i = 0;

	testRequire(
		xrtHttpContentEncodingPlan(
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			&Plan
		) &&
		(Plan.FieldCount == 3) &&
		(Plan.CodingCount == 5) &&
		(Plan.DecoderCount == 3) &&
		(Plan.UnknownCount == 1) &&
		(Plan.JoinedSize == (sizeof(Joined) - 1u)) &&
		((Plan.Flags & (
			XHTTP_CONTENT_ENCODING_PRESENT |
			XHTTP_CONTENT_ENCODING_IDENTITY |
			XHTTP_CONTENT_ENCODING_UNKNOWN |
			XHTTP_CONTENT_ENCODING_LEGACY
		 )) == (
			XHTTP_CONTENT_ENCODING_PRESENT |
			XHTTP_CONTENT_ENCODING_IDENTITY |
			XHTTP_CONTENT_ENCODING_UNKNOWN |
			XHTTP_CONTENT_ENCODING_LEGACY
		 )),
		"Content-Encoding plan facts mismatch"
	);
	xrtHttpContentEncodingCursorInit(&Cursor);
	while ( (Next = xrtHttpContentEncodingNext(
		Fields,
		sizeof(Fields) / sizeof(Fields[0]),
		&Cursor,
		&Item
	)) == XHTTP_NEXT_ITEM ) {
		testRequire(
			(i < (sizeof(Expected) / sizeof(Expected[0]))) &&
			(Item.Coding == Expected[i]),
			"Content-Encoding iterator order mismatch"
		);
		i++;
	}
	testRequire(
		(Next == XHTTP_NEXT_END) &&
		(i == (sizeof(Expected) / sizeof(Expected[0]))),
		"Content-Encoding iterator count mismatch"
	);
	testRequire(
		xrtHttpContentEncodingWrite(
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			Output,
			sizeof(Output),
			&iSize
		) &&
		(iSize == (sizeof(Joined) - 1u)) &&
		(memcmp(Output, Joined, iSize) == 0),
		"Content-Encoding joined value mismatch"
	);
	memset(Output, 'x', sizeof(Output));
	testRequire(
		!xrtHttpContentEncodingWrite(
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			Output,
			sizeof(Joined) - 2u,
			&iSize
		) &&
		(iSize == (sizeof(Joined) - 1u)) &&
		(Output[0] == 'x'),
		"Content-Encoding short output was not failure atomic"
	);
	xrtClearError();
}



/* 验证缺失、空列表和畸形列表不会产生含糊的部分计划。 */
static void testHttpContentEncodingEdges(void)
{
	char AliasValue[] = "gzip";
	xhttpfield Alias = {
		XRT_STR_LITERAL("Content-Encoding"),
		{ AliasValue, sizeof(AliasValue) - 1u }
	};
	static const xhttpfield Empty[] = {
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("")
		}
	};
	static const xhttpfield Invalid[] = {
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("gzip;q=1")
		}
	};
	xhttpcontentencodingplan Plan;
	xhttpcontentencodingplan Before;
	size_t iSize = 0;

	testRequire(
		xrtHttpContentEncodingPlan(NULL, 0, &Plan) &&
		(Plan.Flags == XHTTP_CONTENT_ENCODING_NONE) &&
		(Plan.FieldCount == 0) &&
		(Plan.CodingCount == 0),
		"missing Content-Encoding plan mismatch"
	);
	testRequire(
		xrtHttpContentEncodingPlan(
			Empty, 1, &Plan
		) &&
		((Plan.Flags &
		  XHTTP_CONTENT_ENCODING_PRESENT) != 0) &&
		(Plan.FieldCount == 1) &&
		(Plan.CodingCount == 0) &&
		(Plan.JoinedSize == 0),
		"empty Content-Encoding plan mismatch"
	);
	Before = Plan;
	testRequire(
		!xrtHttpContentEncodingPlan(
			Invalid, 1, &Plan
		) &&
		(memcmp(&Plan, &Before, sizeof(Plan)) == 0),
		"malformed Content-Encoding changed prior plan"
	);
	xrtClearError();
	testRequire(
		!xrtHttpContentEncodingWrite(
			&Alias,
			1,
			AliasValue,
			sizeof(AliasValue) - 1u,
			&iSize
		) &&
		(iSize == (sizeof(AliasValue) - 1u)) &&
		(memcmp(
			AliasValue,
			"gzip",
			sizeof(AliasValue) - 1u
		) == 0),
		"Content-Encoding write accepted overlapping input"
	);
	xrtClearError();
}



/* 运行内容编码协商和表示编码计划测试。 */
int main(void)
{
	testHttpEncodingDefaults();
	testHttpEncodingSelection();
	testHttpEncodingFields();
	testHttpContentEncodingPlan();
	testHttpContentEncodingEdges();
	printf("[PASS] http_encoding\n");
	return 0;
}
