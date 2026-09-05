#include <xrt.h>

#include <stdio.h>



/*
 * 范例：http/te —— TE 字段：传输编码协商与 trailer 能力
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttpTeParse       汇总（可重复的）TE 字段为能力信息
 *   XHTTP_TE_ACCEPTS_TRAILERS   客户端接受 trailer 的标志
 *   xrtHttpTeQuality     查询某编码的 q 值（千分位整数）
 * 模块宏：XRT_MODULE_HTTP_TE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/http/te/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   codings=1 trailers=yes gzip=500
 *
 * q 值读法：gzip;q=0.5 → 500（千分位整数，避免浮点比较）；
 *   未提及的编码 q=0。服务端据此选双方都可接受的传输编码，
 *   trailers=yes 时才允许在 chunked 尾部发 Trailer 字段
 *   （见 trailer 范例）。
 */


/* 汇总重复 TE 字段并读取客户端的传输能力。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("TE"), XRT_STR_INIT("trailers") },
		{ XRT_STR_INIT("TE"), XRT_STR_INIT("gzip;q=0.5") }
	};
	xhttpteinfo Info;

	if ( !xrtHttpTeParse(Fields, 2u, &Info) ) {
		return 1;
	}
	printf(
		"codings=%zu trailers=%s gzip=%u\n",
		Info.TransferCodingCount,
		(Info.Flags & XHTTP_TE_ACCEPTS_TRAILERS) != 0 ?
			"yes" : "no",
		(unsigned int)xrtHttpTeQuality(
			Fields, 2u, XRT_STR_LITERAL("gzip")
		)
	);
	return 0;
}
