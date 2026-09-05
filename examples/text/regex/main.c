/*
 * 范例：text/regex —— 正则主线：不可变表达式、复用匹配器、命名捕获
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtRegexCompile            编译为不可变对象（引用计数）
 *   xrtRegexMatcherCreate/Free 每线程/每次遍历一个匹配器（独占缓存）
 *   xrtRegexMatcherFind        从指定偏移找首个匹配
 *   xrtRegexMatcherNext        推进到下一个匹配（配合循环全扫）
 *   xrtRegexMatcherCaptureNamed 按组名读取捕获（文本 + 范围）
 *   xrtRegexEscape             把任意文本转义为"字面匹配"的正则
 *   xrtRegexFullTest           整串是否完全匹配
 *   xrtStrViewN                由 (指针, 长度) 组装字符串视图
 * 模块宏：XRT_MODULE_REGEX
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/text/regex/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   width = 128
 *   height = 72
 *
 * 生命周期模型：
 *   编译对象不可变可共享（Ref/Release 引用计数）；
 *   匹配器持有独占执行缓存——同一文本反复扫描时复用，
 *   跨匹配零重复分配。非回溯引擎保证最坏线性时间，
 *   不可信输入也不会灾难回溯。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/* 命名组 (?<name>...) (?<value>...)：语义比数字下标清晰。 */
	xregex* pRegex = xrtRegexCompile(
		XRT_STR_LITERAL("(?<name>[A-Za-z]+)=(?<value>\\d+)")
	);
	xregexmatcher* pMatcher;
	xregexcapture Name;
	xregexcapture Value;
	xstrview Text = XRT_STR_LITERAL("width=128 height=72");
	str sLiteral;
	size_t iLiteralSize;

	if ( pRegex == NULL ) {
		return 1;
	}

	/* 匹配器持有正则的自身引用：这里可先释放本地引用。 */
	pMatcher = xrtRegexMatcherCreate(pRegex);
	xrtRegexRelease(pRegex);
	if ( pMatcher == NULL ) {
		return 2;
	}

	/*
	 * 全扫循环：Find 定位首个，Next 依次推进；
	 * 返回三态（MATCH/NONE/ERROR），本例 MATCH 时读两个命名组。
	 */
	for ( xregexresult Result = xrtRegexMatcherFind(pMatcher, Text, 0);
		 Result == XREGEX_MATCH;
		 Result = xrtRegexMatcherNext(pMatcher) ) {
		if ( !xrtRegexMatcherCaptureNamed(pMatcher, XRT_STR_LITERAL("name"), &Name) ||
			 !xrtRegexMatcherCaptureNamed(pMatcher, XRT_STR_LITERAL("value"), &Value) ) {
			xrtRegexMatcherFree(pMatcher);
			return 3;
		}
		printf(
			"%.*s = %.*s\n",
			(int)Name.Text.Size,
			Name.Text.Data,
			(int)Value.Text.Size,
			Value.Text.Data
		);
	}
	xrtRegexMatcherFree(pMatcher);

	/*
	 * 字面匹配场景：用户输入 "file[1].txt" 含正则元字符，
	 * Escape 把 [ ] 转义后编译，即可安全做"完全匹配"判断。
	 */
	sLiteral = xrtRegexEscape(XRT_STR_LITERAL("file[1].txt"), &iLiteralSize);
	if ( sLiteral == NULL ) {
		return 4;
	}
	pRegex = xrtRegexCompile(xrtStrViewN(sLiteral, iLiteralSize));
	xrtFree(sLiteral);
	if ( pRegex == NULL ) {
		return 5;
	}
	if ( xrtRegexFullTest(pRegex, XRT_STR_LITERAL("file[1].txt")) != XREGEX_MATCH ) {
		xrtRegexRelease(pRegex);
		return 6;
	}
	xrtRegexRelease(pRegex);
	return 0;
}
