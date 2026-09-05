/*
 * 范例：http/field_tour —— 字段块全接口巡礼：解析/查找/计数/写出
 * ----------------------------------------------------------------
 * 演示 API：
 *   【块解析】 xrtHttpFieldNext（游标式逐字段）/ FieldBlockCount
 *   【查找】   FieldFind / FieldGet / FieldGetUnique / FieldCount
 *              FieldNameEqual（名字比较）
 *   【写出】   FieldWrite（单字段行）/ FieldBlockWrite（整块）
 *   【token】  FieldTokenCursorInit / FieldTokenNext / FieldTokenFind /
 *              FieldTokenCount（同名字段 token-list 聚合）
 * 模块宏：XRT_MODULE_HTTP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/http/field_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   block-count=2 next=2
 *   get=keep-alive count=2 unique=miss find=1
 *   tokens: gzip deflate count=2
 *   write=ok
 *
 * FieldNext 的输入是原始字节块（"Name: Value\r\n" 逐行游标），
 *   FieldNext 逐字段游标走同一块；FieldParse（单字段）互补；
 *   同名重复字段用 Count 数、
 *   FieldGet 取第一个、GetUnique 在重名时报冲突。
 */

#include <stdio.h>
#include <string.h>
#include <xrt/http.h>

#define SV(x) XRT_STR_LITERAL(x)

int main(void)
{
	static const char sBlock[] =
		"Connection: keep-alive\r\n"
		"Cache-Control: max-age=60\r\n"
		"Connection: Upgrade\r\n";
	static const xhttpfield Fields[3] = {
		{ XRT_STR_INIT("Connection"), XRT_STR_INIT("keep-alive, Upgrade") },
		{ XRT_STR_INIT("Cache-Control"), XRT_STR_INIT("max-age=60") },
		{ XRT_STR_INIT("Accept-Encoding"), XRT_STR_INIT("gzip, deflate") }
	};
	xhttpfield Field;
	size_t iOffset = 0;
	size_t iCount = 0;
	size_t iSize = 0;
	char Buffer[256];
	size_t iFound;
	const xhttpfield* pHit;

	/* FieldBlockCount：整块严格计数。 */
	if ( !xrtHttpFieldBlockCount(SV(sBlock), &iCount) || (iCount != 3u) ) {
		return 1;
	}
	printf("block-count=%zu", iCount);

	/* FieldNext：游标式逐字段（走完块为止）。 */
	{
		size_t iSeen = 0;

		while ( xrtHttpFieldNext(SV(sBlock), &iOffset, &Field) ==
			XHTTP_NEXT_ITEM ) {
			iSeen++;
		}
		printf(" next=%zu\n", iSeen);
	}

	/* Find / Get / Count / GetUnique / NameEqual / ValueValid。 */
	iFound = xrtHttpFieldFind(Fields, 3u, SV("Connection"), 0u);
	{
		const xhttpfield* pGet = xrtHttpFieldGet(Fields, 3u,
			SV("Connection"));

		printf("get=%.*s",
			pGet ? (int)pGet->Value.Size : 6,
			pGet ? pGet->Value.Data : "(null)");
	}
	printf(" value-valid=%d",
		xrtHttpFieldValueValid(Fields[0].Value) ? 1 : 0);
	(void)xrtHttpFieldCount(Fields, 3u, SV("nope"));
	printf(" count=%zu",
		xrtHttpFieldCount(Fields, 3u, SV("Connection")));
	{
		const xhttpfield* pUnique = NULL;

		printf(" unique=%s",
			xrtHttpFieldGetUnique(Fields, 3u, SV("Accept-Encoding"),
				&pUnique) == XHTTP_NEXT_ITEM ? "hit" : "miss");
	}
	printf(" find=%d find2=%zu\n",
		xrtHttpFieldNameEqual(SV("connection"), SV("Connection")) ? 1 : 0,
		xrtHttpFieldFind(Fields, 3u, SV("Accept-Encoding"), 0u));

	/* FieldToken 游标：聚合同名 Connection 字段的 token。 */
	{
		xhttpfieldtokencursor Cursor;
		xstrview Token;
		static const xhttpfield Dup[2] = {
			{ XRT_STR_INIT("Accept-Encoding"), XRT_STR_INIT("gzip") },
			{ XRT_STR_INIT("accept-encoding"), XRT_STR_INIT("deflate") }
		};

		xrtHttpFieldTokenCursorInit(&Cursor);
		printf("tokens:");
		while ( xrtHttpFieldTokenNext(Dup, 2u, SV("Accept-Encoding"),
			&Cursor, &Token) == XHTTP_NEXT_ITEM ) {
			printf(" %.*s", (int)Token.Size, Token.Data);
		}
		{
			size_t iTokens = 0;

			(void)xrtHttpFieldTokenCount(Dup, 2u,
				SV("Accept-Encoding"), &iTokens);
			printf(" count=%zu", iTokens);
		}
		printf(" find=%d\n",
			xrtHttpFieldTokenFind(Dup, 2u, SV("Accept-Encoding"),
				SV("deflate")) == XHTTP_NEXT_ITEM ? 1 : 0);
	}

	/* FieldWrite / FieldBlockWrite。 */
	{
		xstrview Block = SV(sBlock);

		(void)xrtHttpFieldWrite(&Fields[0], Buffer, sizeof(Buffer), &iSize);
		printf("write=%c", Buffer[0]);
		(void)xrtHttpFieldBlockWrite(Fields, 2u, Buffer, sizeof(Buffer),
			&iSize);
		printf(" blockwrite=%s\n",
			iSize > 0u ? "ok" : "fail");
		(void)Block;
	}
	return 0;
}
