/*
 * 范例：text/regex_replace —— 模板替换：命名组与数字组混用
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtRegexReplace   全部替换并返回拥有式结果（xrtFree 释放）
 * 模块宏：XRT_MODULE_REGEX（REPLACE 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/text/regex_replace/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   width: 128 height: 72
 *
 * 替换模板两种引用：
 *   ${name}  命名组（(?<name>...) 捕获）；
 *   $2       数字组（按左括号顺序编号，组 2 即 value）。
 * 同族：ReplaceFirst 只换第一处；ReplaceTo 追加到 xstrbuf（零拷贝拼装）；
 *   ReplaceFuncTo 用回调生成每次替换内容（动态改写场景）。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/* 两个命名组：name 与 value（数字编号分别是 1、2）。 */
	xregex* pRegex = xrtRegexCompile(
		XRT_STR_LITERAL("(?<name>[A-Za-z_]+)=(?<value>\\d+)")
	);
	str sResult;

	if ( pRegex == NULL ) {
		return 1;
	}

	/* "name=value" → "name: value"：$1 组与 $2 组各就各位。 */
	sResult = xrtRegexReplace(
		pRegex,
		XRT_STR_LITERAL("width=128 height=72"),
		XRT_STR_LITERAL("${name}: $2")
	);
	xrtRegexRelease(pRegex);
	if ( sResult == NULL ) {
		return 2;
	}
	puts(sResult);
	xrtFree(sResult);
	return 0;
}
