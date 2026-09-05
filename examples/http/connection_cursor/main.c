/*
 * 范例：http/connection_cursor —— 重复 Connection 字段的游标迭代
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttpConnectionCursorInit   初始化跨字段选项游标
 *   xrtHttpConnectionNext         逐项发布选项（借用原字段值）
 * 模块宏：XRT_MODULE_HTTP_CONNECTION
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/http/connection_cursor/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   option: keep-alive
 *   option: TE
 *
 * 游标 vs Count/Find（connection 范例）：点查用 Find、
 *   统计用 Count；要"逐个处理每个选项"（如代理逐跳头
 *   清理）用游标——不必先数数量再开数组。
 */

#include <stdio.h>

#include <xrt/http_connection.h>



int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Connection"), XRT_STR_INIT("keep-alive") },
		{ XRT_STR_INIT("Connection"), XRT_STR_INIT("TE") }
	};
	xhttpfieldtokencursor Cursor;
	xstrview Option;

	xrtHttpConnectionCursorInit(&Cursor);
	while ( xrtHttpConnectionNext(Fields, 2u, &Cursor, &Option) ==
		XHTTP_NEXT_ITEM ) {
		printf("option: %.*s\n", (int)Option.Size, Option.Data);
	}
	return 0;
}
