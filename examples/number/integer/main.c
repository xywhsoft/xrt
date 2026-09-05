/*
 * 范例：number/integer —— 严格整数解析与多基数输出
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtIntParse   严格解析：基数 + 标志（空白/下划线分隔符）
 *   xrtIntString  整数 → 指定进制字符串（可加前缀/大写）
 *   XNUMBER_PARSE_SPACE / XNUMBER_PARSE_SEPARATOR  解析标志
 *   XNUMBER_PREFIX / XNUMBER_UPPER                 输出标志
 * 模块宏：XRT_MODULE_NUMBER
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/number/integer/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   -9223372036854775808
 *   -0X8000000000000000
 *
 * 严格语义：strtoll 会"解析到哪算哪"，本 API 要么整段合法、
 *   要么失败——首尾空白与数字内下划线必须用标志显式允许。
 * 测试输入是 INT64_MIN：任何中间溢出（如先转正数再取反）
 *   都会算错，这条边界值能暴露实现缺陷。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	int64 iValue;
	str sDecimal;
	str sHex;

	/*
	 * 解析 " -9_223_372_036_854_775_808 "（INT64_MIN）：
	 *   SPACE     允许首尾空白；
	 *   SEPARATOR 允许下划线千分位（财务/日志格式常见）。
	 * 基数 10 显式给定（也支持 2/8/16 自动前缀识别模式）。
	 */
	if ( !xrtIntParse(XRT_STR_LITERAL(" -9_223_372_036_854_775_808 "),
		10, (uint32)XNUMBER_PARSE_SPACE |
		(uint32)XNUMBER_PARSE_SEPARATOR, &iValue) ) {
		return 1;
	}

	/* 十进制原样输出；十六进制加 0X 前缀并大写。 */
	sDecimal = xrtIntString(iValue, 10, 0);
	sHex = xrtIntString(iValue, 16,
		(uint32)XNUMBER_PREFIX | (uint32)XNUMBER_UPPER);
	if ( (sDecimal == NULL) || (sHex == NULL) ) {
		xrtFree(sHex);
		xrtFree(sDecimal);
		return 1;
	}
	printf("%s\n%s\n", sDecimal, sHex);
	xrtFree(sHex);
	xrtFree(sDecimal);
	return 0;
}
