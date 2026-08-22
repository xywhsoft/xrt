#include <stdio.h>

#include <xrt/error.h>
#include <xrt/http_forwarded.h>
#include <xrt/memory.h>



/* 按代理链路顺序读取 Forwarded，并解码常用 for 值。 */
int main(void)
{
	static const xhttpforwardedvalue Output[] = {
		{
			XRT_STR_INIT("[2001:db8::1]:443"),
			XRT_STR_INIT("_edge"),
			XRT_STR_INIT("api.example.test"),
			XRT_STR_INIT("https"),
			NULL,
			0,
			XHTTP_FORWARDED_HAS_FOR |
				XHTTP_FORWARDED_HAS_BY |
				XHTTP_FORWARDED_HAS_HOST |
				XHTTP_FORWARDED_HAS_PROTO
		}
	};
	xstrview Value = XRT_STR_LITERAL(
		"for=192.0.2.43, "
		"for=\"[2001:db8::1]:443\";by=_edge;proto=https"
	);
	xhttpforwardedcursor Cursor;
	xhttpforwarded Forwarded;
	char sNode[80];
	str sOutput;
	size_t iSize;
	size_t iCount;

	if ( !xrtHttpForwardedCount(Value, &iCount) ) {
		return 1;
	}
	printf("elements = %zu\n", iCount);
	xrtHttpForwardedCursorInit(&Cursor);
	while ( xrtHttpForwardedNext(
		Value, &Cursor, &Forwarded
	) == XHTTP_NEXT_ITEM ) {
		if ( (Forwarded.Flags & XHTTP_FORWARDED_HAS_FOR) == 0 ) {
			continue;
		}
		if ( !xrtHttpParamValueWrite(
			&Forwarded.For, sNode, sizeof(sNode), &iSize
		) ) {
			return 1;
		}
		printf("for = %.*s\n", (int)iSize, sNode);
	}
	sOutput = xrtHttpForwardedBuild(Output, 1u, &iSize);
	if ( sOutput == NULL ) {
		return 1;
	}
	printf("normalized = %.*s\n", (int)iSize, sOutput);
	xrtFree(sOutput);
	return xrtGetError() == NULL ? 0 : 1;
}
