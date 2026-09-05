#include <stdio.h>

#include <xrt/http_trailer.h>
#include <xrt/memory.h>



/*
 * 范例：http/trailer —— Trailer 声明值构建：规范、去重
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttpTrailerNamesBuild   由实际 trailer 字段生成声明列表
 * 模块宏：XRT_MODULE_HTTP_TRAILER
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/http/trailer/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   Trailer: Content-Digest, X-Result
 *
 * 协议约定：发送方必须在头里声明会发哪些 trailer 字段
 *  （Trailer: A, B），接收方才能缓存/校验它们——
 *   本入口从"实际要发的字段集合"反推声明值，天然不漏不多，
 *   重复名自动去重、顺序稳定。
 */


/* 从实际 trailer 集合构建规范且去重的声明值。 */
int main(void)
{
	static const xhttpfield Trailers[] = {
		{
			XRT_STR_INIT("Content-Digest"),
			XRT_STR_INIT("sha-256=:...:")
		},
		{
			XRT_STR_INIT("X-Result"),
			XRT_STR_INIT("complete")
		}
	};
	str sNames;

	sNames = xrtHttpTrailerNamesBuild(Trailers, 2u, NULL);
	if ( sNames == NULL ) {
		return 1;
	}
	printf("Trailer: %s\n", sNames);
	xrtFree(sNames);
	return 0;
}
