#include <stdio.h>

#include <xrt/http_via.h>
#include <xrt/memory.h>



/* 按线路顺序读取代理链路。 */
int main(void)
{
	static const xhttpviavalue Output[] = {
		{
			XRT_STR_INIT("HTTP"),
			XRT_STR_INIT("1.1"),
			XRT_STR_INIT("edge.example"),
			XRT_STR_INIT("8080"),
			XRT_STR_INIT("gateway"),
			XHTTP_VIA_HAS_PROTOCOL_NAME |
				XHTTP_VIA_HAS_PORT |
				XHTTP_VIA_HAS_COMMENT
		}
	};
	xstrview Value = XRT_STR_LITERAL(
		"1.0 first, HTTP/1.1 edge.example:8080 (gateway)"
	);
	xhttpviacursor Cursor;
	xhttpvia Via;
	str sOutput;
	size_t iSize;

	xrtHttpViaCursorInit(&Cursor);
	while ( xrtHttpViaNext(
		Value, &Cursor, &Via
	) == XHTTP_NEXT_ITEM ) {
		printf(
			"%.*s via %.*s\n",
			(int)Via.ProtocolVersion.Size,
			Via.ProtocolVersion.Data,
			(int)Via.ReceivedBy.Size,
			Via.ReceivedBy.Data
		);
	}
	sOutput = xrtHttpViaBuild(Output, 1u, &iSize);
	if ( sOutput == NULL ) {
		return 1;
	}
	printf("normalized = %.*s\n", (int)iSize, sOutput);
	xrtFree(sOutput);
	return 0;
}
