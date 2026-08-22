#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留重复 Forwarded 字段解析。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Forwarded"), XRT_STR_INIT("for=192.0.2.1") },
		{ XRT_STR_INIT("forwarded"), XRT_STR_INIT("for=_edge") }
	};
	xhttpforwardedfieldcursor Cursor;
	xhttpforwarded Forwarded;
	size_t iCount;

	xrtHttpForwardedFieldCursorInit(&Cursor);
	return xrtHttpForwardedFieldCount(
		Fields, 2u, &iCount
	) && (iCount == 2u) &&
		(xrtHttpForwardedFieldNext(
		Fields, 2u, &Cursor, &Forwarded
	) == XHTTP_NEXT_ITEM) &&
		(xrtHttpForwardedFieldNext(
			Fields, 2u, &Cursor, &Forwarded
		) == XHTTP_NEXT_ITEM) &&
		((Forwarded.Flags & XHTTP_FORWARDED_HAS_FOR) != 0) ?
		0 : 1;
}
