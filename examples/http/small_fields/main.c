/*
 * 范例：http/small_fields —— TE/Encoding/Upgrade/Expect/Trailer 五小族补集
 * ----------------------------------------------------------------
 * 演示 API：
 *   【TE 族】    xrtHttpTeCursorInit / FieldCursorInit /
 *               CodingParse / Valid / Count / Next /
 *               FieldNext / AcceptsTrailers
 *   【Encoding】 xrtHttpAcceptEncodingParse / Quality / Valid /
 *               CodingParse / ContentEncodingCursorInit /
 *               ContentEncodingNext / ContentEncodingWrite
 *   【Upgrade】  xrtHttpUpgradeCursorInit / Next / Count /
 *               Valid / Parse / Build / ElementWrite
 *   【Expect】   xrtHttpExpectCursorInit / Next / Count / Valid /
 *               ExpectationParse
 *   【Trailer】  xrtHttpTrailerCount / Find / NameValid /
 *               NamesWrite / SectionValid
 * 模块宏：XRT_MODULE_HTTP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/http/small_fields/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   te: parse/next/count=3 trailers=ITEM
 *   encoding: accept/quality/content ok
 *   upgrade: next/count/build ok
 *   expect: cursor/count/valid ok
 *   trailer: count/find/name ok
	printf("upgrade: next/count/build ok\n\n");
 *   expect: cursor/count/valid ok
 *   trailer: count/find/name ok
 *
 * 五个 RFC 字段族共用"游标 + 条目 + 汇总"三件套范式，
 *   本范例每个族走一条代表性链路并断言关键值。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

int main(void)
{
	static const xhttpfield arrTeFields[2] = {
		{ SV("TE"), SV("gzip, trailers") },
		{ SV("TE"), SV("deflate") }
	};
	static const xhttpfield arrExpect[1] = {
		{ SV("Expect"), SV("100-continue, other") }
	};
	xhttptecursor TeCursor;
	xhttptefieldcursor TeFieldCursor;
	xhttptecoding TeCoding;
	xhttpacceptencoding Accept;
	xhttpupgradecursor UpCursor;
	xhttpupgradeitem Upgrade;
	xhttpexpectcursor ExCursor;
	xhttpexpectation Expectation;
	char Buffer[64];
	size_t iCount = 0;
	size_t iTeCount = 0;
	uint16 iQuality = 0;
	int iResult = 1;

	/* ---- TE 族 ---- */
	/* CodingParse：带 q 的成员正常解析（q=0.8 → 800）。 */
	if ( !xrtHttpTeCodingParse(SV("trailers"), &TeCoding) ||
		(TeCoding.Quality != 1000u) ||
		!xrtHttpTeCodingParse(SV("gzip;q=0.8"), &TeCoding) ||
		(TeCoding.Quality != 800u) ) {
		goto Cleanup;
	}
	/* Valid：空成员按 HTTP 列表语法忽略；非法 token 才拒绝。 */
	if ( !xrtHttpTeValid(SV("gzip, deflate")) ||
		!xrtHttpTeValid(SV("gzip,, ,")) ||
		xrtHttpTeValid(SV("gzip;;")) ||
		!xrtHttpTeCount(SV("gzip, deflate, br"), &iCount) ||
		(iCount != 3u) ) {
		goto Cleanup;
	}
	/* Next：单字段游标迭代 2 成员。 */
	xrtHttpTeCursorInit(&TeCursor);
	while ( xrtHttpTeNext(SV("gzip, trailers"), &TeCursor,
			&TeCoding) == XHTTP_NEXT_ITEM ) {
		++iTeCount;
	}
	if ( iTeCount != 2u ) {
		goto Cleanup;
	}
	/* FieldNext：跨两个 TE 行迭代 3 成员。 */
	xrtHttpTeFieldCursorInit(&TeFieldCursor);
	iTeCount = 0;
	while ( xrtHttpTeFieldNext(arrTeFields, 2u, &TeFieldCursor,
			&TeCoding) == XHTTP_NEXT_ITEM ) {
		++iTeCount;
	}
	if ( iTeCount != 3u ) {
		goto Cleanup;
	}
	/* AcceptsTrailers：声明 trailers → ITEM。 */
	if ( xrtHttpTeAcceptsTrailers(arrTeFields, 2u) !=
		XHTTP_NEXT_ITEM ) {
		goto Cleanup;
	}
	printf("te: parse/next/count=3 trailers=ITEM\n");

	/* ---- Encoding 族 ---- */
	{
		static const xhttpfield arrAe[1] = {
			{ SV("Accept-Encoding"), SV("gzip;q=0.9, br") }
		};
		static const xhttpfield arrCe[1] = {
			{ SV("Content-Encoding"), SV("gzip, br") }
		};

		if ( !xrtHttpAcceptEncodingParse(arrAe, 1u, &Accept) ||
			(xrtHttpAcceptEncodingQuality(&Accept,
				XHTTP_CODING_GZIP) != 900u) ||
			(xrtHttpAcceptEncodingQuality(&Accept,
				XHTTP_CODING_IDENTITY) != 1000u) ||
			!xrtHttpAcceptEncodingValid(&Accept) ) {
			goto Cleanup;
		}
		/* CodingParse：token → 内置编码枚举（x-gzip 别名）。 */
		if ( (xrtHttpCodingParse(SV("gzip")) !=
				XHTTP_CODING_GZIP) ||
			(xrtHttpCodingParse(SV("x-gzip")) !=
				XHTTP_CODING_GZIP) ||
			(xrtHttpCodingParse(SV("zstd")) !=
				XHTTP_CODING_NONE) ) {
			goto Cleanup;
		}
		{
			xhttpcontentencodingcursor Cursor;
			xhttpcontentencodingitem Item;

			xrtHttpContentEncodingCursorInit(&Cursor);
			if ( xrtHttpContentEncodingNext(arrCe, 1u,
					&Cursor, &Item) != XHTTP_NEXT_ITEM ||
				(Item.Token.Size != 4u) ||
				(Item.Coding != XHTTP_CODING_GZIP) ) {
				goto Cleanup;
			}
		}
		/* ContentEncodingWrite：字段视图拼接（两段式）。 */
		if ( !xrtHttpContentEncodingWrite(arrCe, 1u, Buffer,
				sizeof(Buffer), &iCount) ||
			(iCount != 8u) ||
			(memcmp(Buffer, "gzip, br", 8u) != 0) ) {
			goto Cleanup;
		}
	}
	printf("encoding: accept/quality/content ok\n");

	/* ---- Upgrade 族 ---- */
	{
		size_t iUp = 0;

		xrtHttpUpgradeCursorInit(&UpCursor);
		while ( xrtHttpUpgradeNext(SV("websocket, h2c"),
				&UpCursor, &Upgrade) == XHTTP_NEXT_ITEM ) {
			++iUp;
		}
		if ( (iUp != 2u) ||
			!xrtHttpUpgradeCount(SV("websocket, h2c"),
				&iCount) ||
			(iCount != 2u) ||
			!xrtHttpUpgradeValid(SV("websocket")) ||
			xrtHttpUpgradeValid(SV("bad token")) ) {
			goto Cleanup;
		}
		/* Parse：完整值视图 → 条目。 */
		if ( !xrtHttpUpgradeParse(SV("websocket"), &Upgrade) ||
			(Upgrade.Protocol.Size != 9u) ) {
			goto Cleanup;
		}
		/* Build：两协议条目数组 → "websocket, h2c"。 */
		{
			static const xhttpupgradeitem arrUp[2] = {
				{ SV("websocket"), SV("") },
				{ SV("h2c"), SV("") }
			};
			str sBuilt = xrtHttpUpgradeBuild(arrUp, 2u, &iCount);

			if ( (sBuilt == NULL) || (iCount != 14u) ||
				(memcmp(sBuilt, "websocket, h2c", 14u) != 0) ) {
				xrtFree(sBuilt);
				goto Cleanup;
			}
			xrtFree(sBuilt);
			/* ElementWrite：单条目带版本 → "h2c/v2"。 */
			if ( !xrtHttpUpgradeElementWrite(
					&(xhttpupgradeitem){ SV("h2c"), SV("v2") },
					Buffer, sizeof(Buffer), &iCount) ||
				(iCount != 6u) ||
				(memcmp(Buffer, "h2c/v2", 6u) != 0) ) {
				goto Cleanup;
			}
		}
	}
	printf("upgrade: next/count/build ok\n");

	/* ---- Expect 族 ---- */
	{
		size_t iEx = 0;

		/* ExpectationParse：100-continue 哨兵。 */
		if ( !xrtHttpExpectationParse(SV("100-continue"),
				&Expectation) ||
			(Expectation.Name.Size != 12u) ||
			(memcmp(Expectation.Name.Data, "100-continue",
					12u) != 0) ) {
			goto Cleanup;
		}
		xrtHttpExpectCursorInit(&ExCursor);
		while ( xrtHttpExpectNext(arrExpect[0].Value, &ExCursor,
				&Expectation) == XHTTP_NEXT_ITEM ) {
			++iEx;
		}
		if ( (iEx != 2u) ||
			!xrtHttpExpectCount(arrExpect[0].Value, &iCount) ||
			(iCount != 2u) ||
			!xrtHttpExpectValid(arrExpect[0].Value) ) {
			goto Cleanup;
		}
	}
	printf("expect: cursor/count/valid ok\n");

	/* ---- Trailer 族 ---- */
	{
		static const xhttpfield arrTrailer[2] = {
			{ SV("Trailer"), SV("X-Checksum, X-Total") },
			{ SV("Trailer"), SV("X-Extra") }
		};

		if ( !xrtHttpTrailerCount(arrTrailer, 2u, &iCount) ||
			(iCount != 3u) ||
			!xrtHttpTrailerFind(arrTrailer, 2u,
				SV("X-Checksum")) ||
			xrtHttpTrailerFind(arrTrailer, 2u,
				SV("X-Missing")) ||
			!xrtHttpTrailerNameValid(SV("X-Checksum")) ||
			xrtHttpTrailerNameValid(SV("Bad Name")) ) {
			goto Cleanup;
		}
		/* NamesWrite 收"实际 trailer 字段"（名值对），从字段名
		 * 拼出规范的 Trailer 声明值——不是收 Trailer 头本身。 */
		{
			static const xhttpfield arrActual[2] = {
				{ SV("X-Checksum"), SV("abc") },
				{ SV("X-Total"), SV("42") }
			};

			if ( !xrtHttpTrailerNamesWrite(arrActual, 2u,
					Buffer, sizeof(Buffer), &iCount) ||
				(iCount != 19u) ||
				(memcmp(Buffer, "X-Checksum, X-Total",
					19u) != 0) ) {
				goto Cleanup;
			}
		}
		{
			static const xhttpfield arrSection[1] = {
				{ SV("X-A"), SV("1") }
			};
			static const xhttpfield arrBad[1] = {
				{ SV("Bad Name"), SV("x") }
			};

			if ( !xrtHttpTrailerSectionValid(arrSection, 1u) ||
				xrtHttpTrailerSectionValid(arrBad, 1u) ) {
				goto Cleanup;
			}
		}
	}
	printf("trailer: count/find/name ok\n");
	iResult = 0;

Cleanup:
	return iResult;
}
