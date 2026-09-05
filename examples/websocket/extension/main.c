#include <stdio.h>

#include <xrt.h>



/*
 * 范例：websocket/extension —— Sec-WebSocket-Extensions 逐项解析
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtWsExtensionNext       迭代扩展项（名字 + 参数区）
 *   xrtWsExtensionParamNext  迭代该项的参数（复用 http param）
 * 模块宏：XRT_MODULE_WEBSOCKET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/websocket/extension/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   extension=permessage-deflate
 *     parameter=client_max_window_bits
 *   extension=x-trace
 *
 * 两层迭代结构：外层按"，"切扩展，内层按"；"切参数——
 *   与 http/param 同一套原语（HTTP_NEXT 三态）。
 *   服务端拿到 (名字, 参数) 后分派给各扩展的协商器
 *   （deflate 范例就是其中一个分派目标）。
 */


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
