/*
 * 范例：number/format —— 数值格式化：分组、进制、百分比与字符
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtIntFormat   有符号整数 + 格式串（分组/字符等）
 *   xrtUIntFormat  无符号整数 + 格式串（进制/前缀/大写）
 *   xrtNumFormat   浮点 + 格式串（定点/百分比/精度）
 * 模块宏：XRT_MODULE_NUMBER
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/number/format/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   -123,456,789
 *   0XDEAD_BEEF
 *   1,234,567.90
 *   12.5%
 *   你
 *
 * 格式串速记（类 Python format 风格）：
 *   ",d"  千分位十进制；"#_X" 0X 前缀+下划线分组+大写十六进制；
 *   ",.2f" 千分位 + 2 位小数定点；".1%" 百分比（自动 ×100 加 %）；
 *   "c"  把整数码点渲染成 UTF-8 字符（20320 = U+4F60 = "你"）。
 * 全部返回拥有式字符串，用 xrtFree 释放。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	str sInteger = xrtIntFormat(
		INT64_C(-123456789), XRT_STR_LITERAL(",d"));
	str sHex = xrtUIntFormat(
		UINT64_C(0xDEADBEEF), XRT_STR_LITERAL("#_X"));
	str sFloat = xrtNumFormat(
		1234567.895, XRT_STR_LITERAL(",.2f"));
	str sPercent = xrtNumFormat(
		0.125, XRT_STR_LITERAL(".1%"));
	str sCharacter = xrtIntFormat(
		20320, XRT_STR_LITERAL("c"));

	/* 任何一个失败都统一走清理（xrtFree 允许 NULL）。 */
	if ( (sInteger == NULL) || (sHex == NULL) ||
		 (sFloat == NULL) || (sPercent == NULL) ||
		 (sCharacter == NULL) ) {
		xrtFree(sInteger);
		xrtFree(sHex);
		xrtFree(sFloat);
		xrtFree(sPercent);
		xrtFree(sCharacter);
		return 1;
	}
	printf("%s\n%s\n%s\n%s\n%s\n",
		sInteger, sHex, sFloat, sPercent, sCharacter);
	xrtFree(sInteger);
	xrtFree(sHex);
	xrtFree(sFloat);
	xrtFree(sPercent);
	xrtFree(sCharacter);
	return 0;
}
