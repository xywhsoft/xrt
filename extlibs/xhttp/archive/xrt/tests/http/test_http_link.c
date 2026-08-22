#include "../test.h"

#include <xrt/http_link.h>



/* 按字节比较借用视图。 */
static bool testLinkViewEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 验证 RFC 属性、关系查询和 UTF-8 title* 优先级。 */
static void testLinkElements(void)
{
	xstrview Value = XRT_STR_LITERAL(
		"</TheBook/chapter2>; rel=\"previous next\";"
		"title=\"fallback\";"
		"title*=UTF-8'de'letztes%20Kapitel;"
		"hreflang=de;hreflang=\"en\";"
		"type=\"text/html\";anchor=\"#toc\";"
		"media=\"screen and (color)\", "
		"<https://example.test/a,b>; rel=canonical;"
		"REL=ignored"
	);
	xhttplinkcursor Cursor;
	xhttplink Link;
	xhttpparam Param;
	xurl Base;
	char sOutput[128];
	size_t iSize;

	testRequire(xrtHttpLinkValid(Value),
		"valid Link field was rejected");
	xrtHttpLinkCursorInit(&Cursor);
	testRequire(
		(xrtHttpLinkNext(
			Value, &Cursor, &Link
		) == XHTTP_NEXT_ITEM) &&
		testLinkViewEqual(
			Link.Target,
			XRT_STR_LITERAL("/TheBook/chapter2")
		) && (Link.HrefLangCount == 2u) &&
		(Link.ParamCount == 8u) &&
		(Link.Flags ==
		 (XHTTP_LINK_HAS_REL |
		  XHTTP_LINK_HAS_TITLE |
		  XHTTP_LINK_HAS_TITLE_EXT |
		  XHTTP_LINK_HAS_TYPE |
		  XHTTP_LINK_HAS_ANCHOR |
		  XHTTP_LINK_HAS_MEDIA)),
		"first Link element shape mismatch"
	);
	testRequire(
		(xrtHttpLinkRelationFind(
			&Link, XRT_STR_LITERAL("NEXT")
		) == XHTTP_NEXT_ITEM) &&
		(xrtHttpLinkRelationFind(
			&Link, XRT_STR_LITERAL("last")
		) == XHTTP_NEXT_END),
		"Link relation lookup mismatch"
	);
	testRequire(
		(xrtHttpLinkParam(
			&Link, XRT_STR_LITERAL("hreflang"), &Param
		) == XHTTP_NEXT_ITEM) &&
		testLinkViewEqual(
			Param.Value, XRT_STR_LITERAL("de")
		),
		"Link parameter lookup mismatch"
	);
	testRequire(
		xrtHttpLinkTitleWrite(
			&Link, sOutput, sizeof(sOutput), &iSize
		) && (iSize == 15u) &&
		(memcmp(sOutput, "letztes Kapitel", 15u) == 0),
		"Link title* decode mismatch"
	);
	testRequire(
		xrtHttpLinkAnchorWrite(
			&Link, sOutput, sizeof(sOutput), &iSize
		) && (iSize == 4u) &&
		(memcmp(sOutput, "#toc", 4u) == 0),
		"Link anchor decode mismatch"
	);
	testRequire(xrtUrlParse(
		XRT_STR_LITERAL(
			"https://example.test/book/chapter3"
		), &Base
	), "Link target base parse failed");
	testRequire(
		xrtHttpLinkTargetResolve(
			&Link, &Base,
			sOutput, sizeof(sOutput), &iSize
		) && (iSize == 37u) &&
		(memcmp(
			sOutput,
			"https://example.test/TheBook/chapter2",
			37u
		) == 0),
		"Link target resolve mismatch"
	);
	testRequire(
		(xrtHttpLinkNext(
			Value, &Cursor, &Link
		) == XHTTP_NEXT_ITEM) &&
		testLinkViewEqual(
			Link.Target,
			XRT_STR_LITERAL("https://example.test/a,b")
		) && (Link.ParamCount == 2u) &&
		testLinkViewEqual(
			Link.Rel.Value,
			XRT_STR_LITERAL("canonical")
		),
		"second Link element or first-rel rule mismatch"
	);
	testRequire(
		xrtHttpLinkNext(
			Value, &Cursor, &Link
		) == XHTTP_NEXT_END,
		"Link iterator did not end"
	);
}



/* 验证重复字段行保持链接的线路顺序。 */
static void testLinkFields(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Link"),
			XRT_STR_INIT("</first>; rel=next")
		},
		{
			XRT_STR_INIT("Other"),
			XRT_STR_INIT("ignored")
		},
		{
			XRT_STR_INIT("link"),
			XRT_STR_INIT("</second>; rel=last")
		}
	};
	static const xhttpfield OtherFields[] = {
		{
			XRT_STR_INIT("Link"),
			XRT_STR_INIT("</other>; rel=next")
		},
		{
			XRT_STR_INIT("Other"),
			XRT_STR_INIT("ignored")
		},
		{
			XRT_STR_INIT("Link"),
			XRT_STR_INIT("</bad>; title=x")
		}
	};
	xhttplinkfieldcursor Cursor;
	xhttplinkfieldcursor SavedCursor;
	xhttplink Link;
	xhttplink SavedLink;

	xrtHttpLinkFieldCursorInit(&Cursor);
	testRequire(
		(xrtHttpLinkFieldNext(
			Fields, 3u, &Cursor, &Link
		) == XHTTP_NEXT_ITEM) &&
		testLinkViewEqual(
			Link.Target, XRT_STR_LITERAL("/first")
		),
		"first repeated Link field mismatch"
	);
	SavedCursor = Cursor;
	SavedLink = Link;
	testRequire(
		(xrtHttpLinkFieldNext(
			OtherFields, 3u, &Cursor, &Link
		) == XHTTP_NEXT_ERROR) &&
		(memcmp(
			&Cursor, &SavedCursor, sizeof(Cursor)
		) == 0) &&
		(memcmp(&Link, &SavedLink, sizeof(Link)) == 0),
		"Link field cursor accepted a different source"
	);
	xrtClearError();
	testRequire(
		(xrtHttpLinkFieldNext(
			Fields, 3u, &Cursor, &Link
		) == XHTTP_NEXT_ITEM) &&
		testLinkViewEqual(
			Link.Target, XRT_STR_LITERAL("/second")
		) && (xrtHttpLinkFieldNext(
			Fields, 3u, &Cursor, &Link
		) == XHTTP_NEXT_END),
		"second repeated Link field mismatch"
	);
}



/* 验证严格语义、空列表和完整预校验的失败原子性。 */
static void testLinkFailure(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT("</missing-rel>; title=x"),
		XRT_STR_INIT("<http://[bad]>; rel=next"),
		XRT_STR_INIT("</x>; rel=Next"),
		XRT_STR_INIT("</x>; rel=\"next \""),
		XRT_STR_INIT("</x>; rel=next; hreflang=abcdefghi"),
		XRT_STR_INIT("</x>; rel=next; type=plain"),
		XRT_STR_INIT("</x>; rel=next; title*=\"UTF-8''x\""),
		XRT_STR_INIT("</x>; rel=next; anchor=bad%GG"),
		XRT_STR_INIT(
			"</x>; rel=next; anchor=\"http://[ba\\d]\""
		),
		XRT_STR_INIT(
			"</x>; rel=\"http://[ba\\d]/relation\""
		)
	};
	static const char FirstSource[] =
		"</a>; rel=next, </b>; rel=last";
	static const char OtherSource[] =
		"</c>; rel=next, </d>; title=x ";
	xhttplinkcursor Cursor;
	xhttplinkcursor SavedCursor;
	xhttplink Link;
	xhttplink SavedLink;
	xhttplink Borrowed;
	xhttplink Equivalent;
	union {
		xurl Base;
		size_t Size;
	} Shared;
	union {
		xhttplink Link;
		size_t Size;
	} LinkShared;
	char sField[] =
		"</x>; rel=next; anchor=\"#toc\"; title=\"name\"";
	size_t i;
	size_t iSize;

	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		testRequire(!xrtHttpLinkValid(Invalid[i]),
			"Link accepted invalid field");
		xrtClearError();
	}
	testRequire(xrtHttpLinkValid(XRT_STR_LITERAL(
		"</x>; rel=next; anchor=\"http://exa\\mple.test/a\""
	)), "Link rejected a valid escaped URI parameter");
	testRequire(xrtHttpLinkValid(
		XRT_STR_LITERAL(" , , \t")
	), "Link rejected an empty RFC list");
	xrtHttpLinkCursorInit(&Cursor);
	testRequire(
		xrtHttpLinkNext(
			XRT_STR_LITERAL(" , , \t"),
			&Cursor, &Link
		) == XHTTP_NEXT_END,
		"empty Link list did not end"
	);
	testRequire(xrtHttpLinkValid(
		XRT_STR_LITERAL(
			"</x>; rel=next; REL; title=first; TITLE"
		)
	), "Link did not ignore later singleton occurrences");
	testRequire(
		xrtHttpLinkElementParse(
			XRT_STR_LITERAL("</x>; rel=next"), &Link
		) && xrtUrlParse(
			XRT_STR_LITERAL("https://example.test/base"),
			&Shared.Base
		),
		"Link overlap fixture setup failed"
	);
	testRequire(!xrtHttpLinkTargetResolve(
		&Link, &Shared.Base, NULL, 0, &Shared.Size
	), "Link target resolve accepted Base/size overlap");
	xrtClearError();
	testRequire(
		xrtHttpLinkElementParse(
			(xstrview){ sField, sizeof(sField) - 1u },
			&Borrowed
		),
		"Link borrowed-output fixture setup failed"
	);
	memset(&Equivalent, 0xA5, sizeof(Equivalent));
	Equivalent.Element = Borrowed.Element;
	Equivalent.Target = Borrowed.Target;
	Equivalent.Parameters = Borrowed.Parameters;
	Equivalent.Rel = Borrowed.Rel;
	Equivalent.Anchor = Borrowed.Anchor;
	Equivalent.Rev = Borrowed.Rev;
	Equivalent.Media = Borrowed.Media;
	Equivalent.Title = Borrowed.Title;
	Equivalent.TitleExt = Borrowed.TitleExt;
	Equivalent.Type = Borrowed.Type;
	Equivalent.ParamCount = Borrowed.ParamCount;
	Equivalent.HrefLangCount = Borrowed.HrefLangCount;
	Equivalent.Flags = Borrowed.Flags;
	testRequire(
		xrtHttpLinkRelationFind(
			&Equivalent, XRT_STR_LITERAL("next")
		) == XHTTP_NEXT_ITEM,
		"Link descriptor depended on structure padding"
	);
	testRequire(!xrtHttpLinkAnchorWrite(
		&Borrowed, sField, 4u, &iSize
	), "Link anchor accepted output over borrowed field");
	xrtClearError();
	testRequire(!xrtHttpLinkTitleWrite(
		&Borrowed, sField, 4u, &iSize
	), "Link title accepted output over borrowed field");
	xrtClearError();
	LinkShared.Link = Borrowed;
	testRequire(
		xrtHttpLinkAnchorBuild(
			&LinkShared.Link, &LinkShared.Size
		) == NULL,
		"Link anchor build accepted descriptor/size overlap"
	);
	xrtClearError();
	testRequire(
		xrtHttpLinkTargetResolveBuild(
			&Borrowed, &Shared.Base, &Shared.Size
		) == NULL,
		"Link target build accepted Base/size overlap"
	);
	xrtClearError();
	xrtHttpLinkCursorInit(&Cursor);
	SavedCursor = Cursor;
	memset(&Link, 0xA5, sizeof(Link));
	SavedLink = Link;
	testRequire(
		(xrtHttpLinkNext(
			XRT_STR_LITERAL(
				"</ok>; rel=next, </bad>; title=x"
			), &Cursor, &Link
		) == XHTTP_NEXT_ERROR) &&
		(memcmp(
			&Cursor, &SavedCursor, sizeof(Cursor)
		) == 0) &&
		(memcmp(&Link, &SavedLink, sizeof(Link)) == 0),
		"Link later failure was not atomic"
	);
	xrtClearError();
	_Static_assert(
		sizeof(FirstSource) == sizeof(OtherSource),
		"Link source-binding fixtures must have equal size"
	);
	xrtHttpLinkCursorInit(&Cursor);
	testRequire(
		xrtHttpLinkNext(
			(xstrview){ FirstSource, sizeof(FirstSource) - 1u },
			&Cursor, &Link
		) == XHTTP_NEXT_ITEM,
		"Link source-binding fixture setup failed"
	);
	SavedCursor = Cursor;
	SavedLink = Link;
	testRequire(
		(xrtHttpLinkNext(
			(xstrview){ OtherSource, sizeof(OtherSource) - 1u },
			&Cursor, &Link
		) == XHTTP_NEXT_ERROR) &&
		(memcmp(
			&Cursor, &SavedCursor, sizeof(Cursor)
		) == 0) &&
		(memcmp(&Link, &SavedLink, sizeof(Link)) == 0),
		"Link cursor accepted a different equal-size source"
	);
	xrtClearError();
	testRequire(
		xrtHttpLinkNext(
			(xstrview){ FirstSource, sizeof(FirstSource) - 1u },
			&Cursor, &Link
		) == XHTTP_NEXT_ITEM,
		"Link cursor did not recover on its bound source"
	);
}



/* 执行 RFC 8288 Link 解析测试。 */
int main(void)
{
	testLinkElements();
	testLinkFields();
	testLinkFailure();
	printf("[PASS] http_link\n");
	return 0;
}
