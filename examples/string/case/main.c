/*
 * 范例：string/case —— 大小写转换族：Upper/Lower 与 To 变体
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtStrUpper / xrtStrLower      转换为新字符串（拥有式）
 *   xrtStrUpperTo / xrtStrLowerTo  转换到调用方缓冲（零分配）
 * 模块宏：XRT_MODULE_STRING
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/string/case/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   upper=HELLO XRT
 *   lower=hello xrt
 *   to-upper=BUFFER-WAY
 *   to-lower=buffer-way
 */

#include <stdio.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

static void printOwned(cstr pTag, str sText)
{
	printf("%s=%s\n", pTag, sText ? sText : "(null)");
	xrtFree(sText);
}

int main(void)
{
	char Buffer[16];

	/* 拥有式：结果由 xrtFree 释放。 */
	printOwned("upper", xrtStrUpper(SV("hello xrt")));
	printOwned("lower", xrtStrLower(SV("HELLO XRT")));

	/* 缓冲式：零分配热路径；返回值即缓冲（失败 NULL）。 */
	if ( xrtStrUpperTo(SV("buffer-way"), Buffer, sizeof(Buffer)) ) {
		printf("to-upper=%s\n", Buffer);
	}
	if ( xrtStrLowerTo(SV("BUFFER-WAY"), Buffer, sizeof(Buffer)) ) {
		printf("to-lower=%s\n", Buffer);
	}
	return 0;
}
