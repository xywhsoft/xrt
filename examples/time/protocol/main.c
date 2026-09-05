/*
 * 范例：time/protocol —— 协议专用时间格式：RFC 3339 与 HTTP-date
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTimeParseRFC3339  解析 RFC 3339（JSON/API 时间戳标准）
 *   xrtTimeRFC3339       生成 RFC 3339 文本（自选偏移秒）
 *   xrtTimeHTTPDate      生成 IMF-fixdate（HTTP 头专用，恒为 GMT）
 * 模块宏：XRT_MODULE_TIME
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/time/protocol/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   RFC 3339: 1994-11-06T16:49:37+08:00
 *   HTTP-date: Sun, 06 Nov 1994 08:49:37 GMT
 *
 * 为什么不用通用格式化：协议格式是"精确字符串契约"——
 *   HTTP/1.1 要求三种日期形式且互操作敏感，
 *   RFC 3339 的 Z/+hh:mm 后缀规则各异。专用入口保证
 *   一次生成即合法，省去手拼格式串的踩坑面。
 * 同一时刻、两种表达：+8 展示本地化时间，GMT 是协议不变量。
 */

#include <xrt.h>

#include <stdio.h>



int main(void)
{
	xtime iTime;
	str sRFC3339;
	str sHTTPDate;

	/* 解析 HTTP 文档里的经典时间戳（Z 后缀 = UTC）。 */
	if ( !xrtTimeParseRFC3339(
		XRT_STR_LITERAL("1994-11-06T08:49:37Z"), &iTime) ) {
		return 1;
	}

	/* 生成 RFC 3339：偏移 +8×3600 → 本地化表达。 */
	sRFC3339 = xrtTimeRFC3339(iTime, 8 * 3600);

	/* 生成 HTTP-date：协议要求恒定 GMT，无偏移参数。 */
	sHTTPDate = xrtTimeHTTPDate(iTime);
	if ( (sRFC3339 == NULL) || (sHTTPDate == NULL) ) {
		xrtFree(sRFC3339);
		xrtFree(sHTTPDate);
		return 1;
	}
	printf("RFC 3339: %s\n", sRFC3339);
	printf("HTTP-date: %s\n", sHTTPDate);
	xrtFree(sRFC3339);
	xrtFree(sHTTPDate);
	return 0;
}
