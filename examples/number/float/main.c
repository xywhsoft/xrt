/*
 * 范例：number/float —— 浮点严格解析与最短往返输出
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtNumParse   严格解析 double（空白/下划线需显式允许）
 *   xrtNumString  最短往返表示（precision=0 表示自动）
 * 模块宏：XRT_MODULE_NUMBER
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/number/float/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   -12.3456789
 *
 * 最短往返（shortest round-trip）：输出的十进制位数
 *   恰好保证 Parse 回来与原 double 逐位相等——
 *   比 "%.17g" 短得多，又不像 "%.6f" 丢精度，
 *   是序列化浮点的事实标准做法。
 * 输入 " -1_234.567_890e-2 " 带空白/下划线/科学计数法，
 *   一次验证三种宽松度（由两个标志显式开启）。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	double fValue;
	str sText;

	/* 解析：SPACE 允许首尾空白，SEPARATOR 允许下划线分组。 */
	if ( !xrtNumParse(
		XRT_STR_LITERAL(" -1_234.567_890e-2 "),
		(uint32)XNUMBER_PARSE_SPACE |
		(uint32)XNUMBER_PARSE_SEPARATOR,
		&fValue
	) ) {
		return 1;
	}

	/* 最短往返输出；也可传正整数强制固定位数。 */
	sText = xrtNumString(fValue, 0);
	if ( sText == NULL ) {
		return 1;
	}
	printf("%s\n", sText);
	xrtFree(sText);
	return 0;
}
