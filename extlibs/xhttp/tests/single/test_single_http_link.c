#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_LINK
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留 Link 重复字段解析和关系查询。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Link"),
			XRT_STR_INIT("</next>; rel=next")
		},
		{
			XRT_STR_INIT("link"),
			XRT_STR_INIT("</last>; rel=last")
		}
	};
	xhttplinkfieldcursor Cursor;
	xhttplink Link;

	xrtHttpLinkFieldCursorInit(&Cursor);
	return (xrtHttpLinkFieldNext(
		Fields, 2u, &Cursor, &Link
	) == XHTTP_NEXT_ITEM) &&
		(xrtHttpLinkRelationFind(
			&Link, XRT_STR_LITERAL("NEXT")
		) == XHTTP_NEXT_ITEM) ? 0 : 1;
}
