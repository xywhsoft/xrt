#include <stdio.h>

#include <xrt/error.h>
#include <xrt/http_link.h>
#include <xrt/memory.h>



/* 查找 next 关系并相对当前资源解析链接目标。 */
int main(void)
{
	static const xhttplinkvalue Output[] = {
		{
			XRT_STR_INIT("/items?page=2"),
			XRT_STR_INIT("next alternate"),
			NULL,
			0
		}
	};
	xstrview Value = XRT_STR_LITERAL(
		"</items?page=2>; rel=\"next alternate\";"
		"title*=UTF-8'en'next%20page"
	);
	xhttplinkcursor Cursor;
	xhttplink Link;
	xurl Base;
	char sTarget[128];
	str sOutput;
	size_t iSize;

	if ( !xrtUrlParse(
		XRT_STR_LITERAL("https://api.example.test/items?page=1"),
		&Base
	) ) {
		return 1;
	}
	xrtHttpLinkCursorInit(&Cursor);
	while ( xrtHttpLinkNext(
		Value, &Cursor, &Link
	) == XHTTP_NEXT_ITEM ) {
		if ( xrtHttpLinkRelationFind(
			&Link, XRT_STR_LITERAL("next")
		) != XHTTP_NEXT_ITEM ) {
			continue;
		}
		if ( !xrtHttpLinkTargetResolve(
			&Link, &Base,
			sTarget, sizeof(sTarget), &iSize
		) ) {
			return 1;
		}
		printf("next = %.*s\n", (int)iSize, sTarget);
	}
	sOutput = xrtHttpLinkBuild(Output, 1u, &iSize);
	if ( sOutput == NULL ) {
		return 1;
	}
	printf("normalized = %.*s\n", (int)iSize, sOutput);
	xrtFree(sOutput);
	return xrtGetError() == NULL ? 0 : 1;
}
