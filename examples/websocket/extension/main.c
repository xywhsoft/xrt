#include <stdio.h>

#include <xrt.h>



/* 展示逐项读取 Sec-WebSocket-Extensions 和扩展参数。 */
int main(void)
{
	xstrview Text = XRT_STR_LITERAL(
		"permessage-deflate; client_max_window_bits, x-trace"
	);
	xwsextension Extension;
	xhttpnext Next;
	size_t iOffset = 0;

	for ( ;; ) {
		xhttpparam Param;
		size_t iParam = 0;

		Next = xrtWsExtensionNext(
			Text,
			&iOffset,
			&Extension
		);
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			return 1;
		}
		printf(
			"extension=%.*s\n",
			(int)Extension.Name.Size,
			Extension.Name.Data
		);
		while ( xrtWsExtensionParamNext(
			&Extension,
			&iParam,
			&Param
		) == XHTTP_NEXT_ITEM ) {
			printf(
				"  parameter=%.*s\n",
				(int)Param.Name.Size,
				Param.Name.Data
			);
		}
	}
	return 0;
}
