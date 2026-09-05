/*
 * 范例：string/compare —— 比较与判定全家：Equal/Compare/Starts/Ends/Contains/Blank/Empty
 * ----------------------------------------------------------------
 * 演示 API（本目录新增，覆盖 string 模块比较族全部变体）：
 *   xrtStrEqual / xrtStrCaseEqual     精确相等 / 大小写不敏感相等
 *   xrtStrCompare / xrtStrCaseCompare 三态比较（字典序）
 *   xrtStrStarts / xrtStrCaseStarts   前缀判定
 *   xrtStrEnds / xrtStrCaseEnds       后缀判定
 *   xrtStrContains / xrtStrCaseContains   包含子串
 *   xrtStrContainsAny                 包含集合中任意字节
 *   xrtStrCount / xrtStrCaseCount     子串出现次数（不重叠）
 *   xrtStrBlank                       是否全空白
 *   xrtStrEmpty                       是否空视图
 * 模块宏：XRT_MODULE_STRING
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/string/compare/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   equal=0 case-equal=1 cmp=1 blank=1 empty=1
 *   starts=1 ends=1 contains=1 contains-any=1 count=2 case-count=3
 *   starts=1 case-ends=1 case-contains=1 case-cmp=1
 *
 * 大小写变体一律按 ASCII 折叠（C 库 strcmp 风格），
 *   非 ASCII 字节原样参与比较——Unicode 大小写需先做
 *   charset 模块的折叠（见 charset 范例）。
 */

#include <stdio.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

int main(void)
{
	xstrview Hello = SV("Hello XRT");
	xstrview HelloUp = SV("HELLO XRT");

	/* 相等与大小写不敏感相等：同一对输入两种答案。 */
	printf("equal=%d case-equal=%d\n",
		xrtStrEqual(Hello, HelloUp) ? 1 : 0,
		xrtStrCaseEqual(Hello, HelloUp) ? 1 : 0);

	/* 三态比较：'h'(0x68) < 'H'(0x48)? 否——Hello > HELLO，返回正数 1。 */
	printf("cmp=%d\n", xrtStrCompare(Hello, HelloUp) > 0 ? 1 : -1);

	/* 空白与空：全空白（含空格/制表）与零长度视图。 */
	printf("blank=%d empty=%d\n",
		xrtStrBlank(SV("  \t ")) ? 1 : 0,
		xrtStrEmpty(SV("")) ? 1 : 0);

	/* 前缀 / 后缀（各带大小写变体，变体命中）。 */
	printf("starts=%d ends=%d\n",
		xrtStrCaseStarts(Hello, SV("hello")) ? 1 : 0,
		xrtStrEnds(Hello, SV("XRT")) ? 1 : 0);

	/* 包含子串 / 包含集合任意字节。 */
	printf("contains=%d contains-any=%d\n",
		xrtStrContains(Hello, SV("lo X")) ? 1 : 0,
		xrtStrContainsAny(SV("host:8080"), SV(";:/")) ? 1 : 0);

	/* 计数：不重叠出现次数——"ababab" 中 "abab" 只算 1 次。 */
	/* 计数：精确版（区分大小写）与 Case 版各一次。 */
	printf("count=%d case-count=%d\n",
		(int)xrtStrCount(SV("ab aB ab"), SV("ab")),
		(int)xrtStrCaseCount(SV("Ab aB ab"), SV("ab")));

	/* 精确前缀 + Case 后缀/包含/三态比较。 */
	printf("starts=%d case-ends=%d case-contains=%d case-cmp=%d\n",
		xrtStrStarts(HelloUp, SV("HELLO")) ? 1 : 0,
		xrtStrCaseEnds(Hello, SV("xrt")) ? 1 : 0,
		xrtStrCaseContains(SV("Config"), SV("FIG")) ? 1 : 0,
		xrtStrCaseCompare(Hello, HelloUp) == 0 ? 1 : -1);
	return 0;
}
