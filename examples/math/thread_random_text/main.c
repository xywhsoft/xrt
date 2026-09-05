/*
 * 范例：math/thread_random_text —— 自定义字母表的线程级随机文本
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtRandStringFrom  从指定字符集生成 n 字符随机串（线程 RNG）
 * 模块宏：XRT_MODULE_RANDOM
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/math/thread_random_text/main.c -lws2_32 -liphlpapi
 * 预期输出（无种子重置则随线程状态变化；仅含指定字符集）：
 *   0531e8d6a63050654b5bd081
 *
 * 自定义字母表的典型用途：
 *   小写十六进制（本例）——订单号、验证码、短链后缀；
 *   去混淆字符集（无 0/O/1/l）——人工抄写场景；
 *   数字集——短信验证码。
 * 线程级 Rand* 前缀 + From 后缀 = "一次性生成"最短路径；
 *   要复现请用显式 RngStringFrom + 固定种子。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/* 字符集 "abcdef0123456789"：小写十六进制风格标识。 */
	str sText = xrtRandStringFrom(XRT_STR_LITERAL("abcdef0123456789"), 24);

	if ( sText == NULL ) {
		return 1;
	}
	printf("%s\n", sText);
	xrtFree(sText);
	return 0;
}
