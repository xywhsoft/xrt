#include "../test.h"

#include <xrt/http_origin.h>



/* 按字节比较借用视图。 */
static bool testOriginText(xstrview Text, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		((iSize == 0) ||
		 (memcmp(Text.Data, sExpected, iSize) == 0));
}



/* 验证 Origin 解析、默认端口和 null 身份规则。 */
static void testHttpOriginParseAndSame(void)
{
	xhttporigin Explicit;
	xhttporigin Default;
	xhttporigin Other;
	xhttporigin Null;

	testRequire(
		xrtHttpOriginParse(
			XRT_STR_LITERAL(" HTTPS://Example.COM:443 "),
			&Explicit
		) && testOriginText(Explicit.Url.Scheme, "HTTPS") &&
		testOriginText(Explicit.Url.Host, "Example.COM") &&
		(Explicit.Url.Port == 443u),
		"Origin tuple parse mismatch"
	);
	testRequire(
		xrtHttpOriginParse(
			XRT_STR_LITERAL("https://example.com"), &Default
		) && xrtHttpOriginSame(&Explicit, &Default),
		"explicit default port was not same-origin"
	);
	testRequire(
		xrtHttpOriginParse(
			XRT_STR_LITERAL("https://example.com:444"), &Other
		) && !xrtHttpOriginSame(&Default, &Other) &&
		(xrtGetError() == NULL),
		"different Origin port compared equal"
	);
	testRequire(
		xrtHttpOriginParse(XRT_STR_LITERAL("null"), &Null) &&
		((Null.Flags & XHTTP_ORIGIN_NULL) != 0) &&
		!xrtHttpOriginSame(&Null, &Null) &&
		(xrtGetError() == NULL),
		"null Origin acquired a stable identity"
	);
}



/* 验证从完整 URL 提取 Origin 时忽略资源路径。 */
static void testHttpOriginFromUrl(void)
{
	xurl Url;
	xhttporigin Origin;

	testRequire(
		xrtUrlParse(
			XRT_STR_LITERAL(
				"https://example.test:8443/path?q=1#part"
			), &Url
		) && xrtHttpOriginFromUrl(&Url, &Origin) &&
		(Origin.Url.Path.Size == 0) &&
		((Origin.Url.Flags &
		 (XURL_HAS_QUERY | XURL_HAS_FRAGMENT)) == 0) &&
		(Origin.Url.Port == 8443u),
		"URL to Origin extraction mismatch"
	);
}



/* 验证显式空端口按省略端口参与同源比较。 */
static void testHttpOriginEmptyPort(void)
{
	xhttporigin Empty;
	xhttporigin Default;

	testRequire(
		xrtHttpOriginParse(
			XRT_STR_LITERAL("https://Example.Test:"), &Empty
		) && ((Empty.Url.Flags & XURL_PORT_EMPTY) != 0) &&
		xrtHttpOriginParse(
			XRT_STR_LITERAL("https://example.test"), &Default
		) && xrtHttpOriginSame(&Empty, &Default),
		"empty Origin port was not treated as an omitted port"
	);
}



/* 验证公开 Origin 描述符不能混入资源路径或查询组件。 */
static void testHttpOriginDescriptor(void)
{
	xhttporigin Valid;
	xhttporigin Invalid;

	testRequire(
		xrtHttpOriginParse(
			XRT_STR_LITERAL("https://example.test"), &Valid
		),
		"Origin descriptor setup failed"
	);
	Invalid = Valid;
	Invalid.Url.Path = XRT_STR_LITERAL("/resource");
	testRequire(
		!xrtHttpOriginSame(&Invalid, &Valid) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Origin descriptor accepted a resource path"
	);
	xrtClearError();
	Invalid = Valid;
	Invalid.Url.Flags |= XURL_HAS_QUERY;
	testRequire(
		!xrtHttpOriginSame(&Invalid, &Valid) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Origin descriptor accepted a query presence flag"
	);
	xrtClearError();
}



/* 验证历史 Origin 列表和完整预校验。 */
static void testHttpOriginList(void)
{
	xstrview Value = XRT_STR_LITERAL(
		" http://first.test https://second.test:8443 "
	);
	xhttporigincursor Cursor;
	xhttporigin Origin;
	size_t iCount = 0;

	testRequire(xrtHttpOriginValid(Value),
		"valid Origin list was rejected");
	xrtHttpOriginCursorInit(&Cursor);
	while ( xrtHttpOriginNext(
		Value, &Cursor, &Origin
	) == XHTTP_NEXT_ITEM ) {
		iCount++;
	}
	testRequire(
		(iCount == 2u) && (xrtGetError() == NULL),
		"Origin list iteration mismatch"
	);
	xrtHttpOriginCursorInit(&Cursor);
	testRequire(
		xrtHttpOriginNext(
			XRT_STR_LITERAL("http://valid.test bad"),
			&Cursor,
			&Origin
		) == XHTTP_NEXT_ERROR,
		"Origin cursor published a valid prefix before bad suffix"
	);
	xrtClearError();
}



/* 验证 Origin 列表游标不能在迭代中切换等长输入或改变长度。 */
static void testHttpOriginCursorBinding(void)
{
	static char ValueA[] =
		"https://one.test https://two.test";
	static char ValueB[] =
		"https://one.test https://two.test";
	xhttporigincursor Cursor;
	xhttporigincursor Saved;
	xhttporigin Origin;

	xrtHttpOriginCursorInit(&Cursor);
	testRequire(
		xrtHttpOriginNext(
			XRT_STR_LITERAL(ValueA), &Cursor, &Origin
		) == XHTTP_NEXT_ITEM,
		"Origin source-binding setup failed"
	);
	Saved = Cursor;
	memset(&Origin, 0xA5, sizeof(Origin));
	testRequire(
		(xrtHttpOriginNext(
			XRT_STR_LITERAL(ValueB), &Cursor, &Origin
		) == XHTTP_NEXT_ERROR) &&
		(memcmp(&Cursor, &Saved, sizeof(Cursor)) == 0) &&
		(Origin.Text.Data == NULL) &&
		(Origin.Text.Size == 0),
		"Origin cursor accepted an equal-size source switch"
	);
	xrtClearError();
	testRequire(
		(xrtHttpOriginNext(
			(xstrview){ ValueA, sizeof(ValueA) - 2u },
			&Cursor,
			&Origin
		) == XHTTP_NEXT_ERROR) &&
		(memcmp(&Cursor, &Saved, sizeof(Cursor)) == 0),
		"Origin cursor accepted a source-size change"
	);
	xrtClearError();
}



/* 验证唯一 Origin 字段快捷路径。 */
static void testHttpOriginFields(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Content-Type"),
			XRT_STR_INIT("application/json")
		},
		{
			XRT_STR_INIT("Origin"),
			XRT_STR_INIT("https://app.test")
		}
	};
	static const xhttpfield Duplicate[] = {
		{
			XRT_STR_INIT("Origin"),
			XRT_STR_INIT("https://one.test")
		},
		{
			XRT_STR_INIT("origin"),
			XRT_STR_INIT("https://two.test")
		}
	};
	static const xhttpfield List = {
		XRT_STR_INIT("Origin"),
		XRT_STR_INIT("https://one.test https://two.test")
	};
	xhttporigin Origin;

	testRequire(
		(xrtHttpOriginFields(
			Fields, 2u, &Origin
		) == XHTTP_NEXT_ITEM) &&
		testOriginText(Origin.Url.Host, "app.test"),
		"unique Origin field lookup mismatch"
	);
	testRequire(
		xrtHttpOriginFields(
			Duplicate, 2u, &Origin
		) == XHTTP_NEXT_ERROR,
		"duplicate Origin fields were accepted"
	);
	xrtClearError();
	testRequire(
		xrtHttpOriginFields(
			&List, 1u, &Origin
		) == XHTTP_NEXT_ERROR,
		"legacy Origin list was accepted by single-value helper"
	);
	xrtClearError();
}



/* 验证非法 Origin 语法和内存边界。 */
static void testHttpOriginInvalid(void)
{
	static const cstr Invalid[] = {
		"",
		"NULL",
		"null https://example.test",
		"https://one.test  https://two.test",
		"https://one.test\thttps://two.test",
		"https://",
		"https://user@example.test",
		"https://example.test:65536",
		"https://example.test/",
		"https://example.test?q=1",
		"https://example.test#part"
	};
	uint8 Storage[sizeof(xhttporigin) + 1u];
	xhttporigin* pOrigin = (xhttporigin*)(void*)(Storage + 1u);
	xhttporigin Origin;
	size_t i;

	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		testRequire(
			!xrtHttpOriginValid((xstrview){
				Invalid[i], strlen(Invalid[i])
			}),
			"invalid Origin value was accepted"
		);
		xrtClearError();
	}
	testRequire(
		xrtHttpOriginParse(
			XRT_STR_LITERAL("https://unaligned.test"), pOrigin
		),
		"unaligned Origin output failed"
	);
	memcpy(&Origin, pOrigin, sizeof(Origin));
	testRequire(testOriginText(Origin.Url.Host, "unaligned.test"),
		"unaligned Origin output content mismatch");
}



int main(void)
{
	testHttpOriginParseAndSame();
	testHttpOriginFromUrl();
	testHttpOriginEmptyPort();
	testHttpOriginDescriptor();
	testHttpOriginList();
	testHttpOriginCursorBinding();
	testHttpOriginFields();
	testHttpOriginInvalid();
	printf("[PASS] http_origin\n");
	return 0;
}
