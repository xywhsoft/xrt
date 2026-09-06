/*
 * 范例：http/method_tour —— 方法/状态/长度/质量族
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttpMethodParse / MethodEqual   方法解析 / 文本比较
 *   xrtHttpMethodSafe / Idempotent    安全/幂等语义判定
 *   xrtHttpStatusText                  状态码 → 标准文本
 *   xrtHttpResponseContentAllowed     响应是否允许携带内容
 *   xrtHttpContentLengthParse          Content-Length 严格解析
 *   xrtHttpQualityParse                q 值解析（千分位整数）
 * 模块宏：XRT_MODULE_HTTP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/http/method_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   parse=2 eq=1/0 safe=1 idem=1
 *   status=OK content-allowed=0
 *   cl=42 q=500
 */

#include <stdio.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

int main(void)
{
	uint64 iLength = 0;
	uint16 iQuality = 0;

	/* MethodParse 返回位枚举（GET=0x2）；Equal 按 HTTP 规则
	 * 大小写敏感——同形真/异形假各演示一次。 */
	printf("parse=%u", (unsigned)xrtHttpMethodParse(SV("GET")));
	printf(" eq=%d/%d",
		xrtHttpMethodEqual(SV("PATCH"), SV("PATCH")) ? 1 : 0,
		xrtHttpMethodEqual(SV("PATCH"), SV("patch")) ? 1 : 0);
	printf(" safe=%d", xrtHttpMethodSafe(SV("GET")) ? 1 : 0);
	printf(" idem=%d\n", xrtHttpMethodIdempotent(SV("DELETE")) ? 1 : 0);

	/* StatusText / ResponseContentAllowed（204 无内容）。 */
	{
		xstrview Text = xrtHttpStatusText(200);

		printf("status=%.*s", (int)Text.Size, Text.Data);
		printf(" content-allowed=%d\n",
			xrtHttpResponseContentAllowed(SV("HEAD"), 204) ? 1 : 0);
	}

	/* ContentLength / Quality 严格数值解析。 */
	if ( !xrtHttpContentLengthParse(SV("42"), &iLength) ) {
		return 1;
	}
	printf("cl=%llu", (unsigned long long)iLength);
	if ( !xrtHttpQualityParse(SV("0.5"), &iQuality) ) {
		return 2;
	}
	printf(" q=%zu\n", iQuality);
	return 0;
}
