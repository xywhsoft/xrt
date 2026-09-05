#include <stdio.h>

#include <xrt/http_connection.h>



/*
 * 范例：http/connection —— Connection 选项与持久性判定
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttpConnectionCount      统计选项 token 数（跨重复字段）
 *   xrtHttpConnectionFind       大小写不敏感查找选项
 *   xrtHttpConnectionPersistence 按协议版本判定连接持久性
 *   XHTTP_CONNECTION_PERSIST / CLOSE   判定结果枚举
 * 模块宏：XRT_MODULE_HTTP_CONNECTION
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/http/connection/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   options=2 persistent=yes
 *
 * 重复字段的语义：RFC 允许 Connection 出现多次，选项等于
 *   逻辑并集——Count/Find 都按多字段数组处理。
 * 持久性规则内置：HTTP/1.1 默认持久（无 close 即 PERSIST）、
 *   HTTP/1.0 默认关闭（需 keep-alive 才持久）——
 *   调用方不背协议版本差异。
 */


/* 演示跨重复字段查询选项并判断 HTTP/1.1 持久性。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Connection"),
			XRT_STR_INIT("keep-alive")
		},
		{
			XRT_STR_INIT("Connection"),
			XRT_STR_INIT("TE")
		}
	};
	xhttpconnectionstatus Status;
	size_t iCount;

	if ( !xrtHttpConnectionCount(
		Fields, 2u, &iCount
	) || (iCount != 2u) ||
		xrtHttpConnectionFind(
		Fields, 2u, XRT_STR_LITERAL("te")
	) != XHTTP_NEXT_ITEM ) {
		return 1;
	}
	Status = xrtHttpConnectionPersistence(
		XHTTP_VERSION_1_1, Fields, 2u, 0
	);
	if ( Status != XHTTP_CONNECTION_PERSIST ) {
		return 1;
	}
	printf("options=%zu persistent=yes\n", iCount);
	return 0;
}
