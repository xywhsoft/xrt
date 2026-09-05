/*
 * 范例：text/regex_set —— 正则集合：一次编译多条分类规则、多命中报告
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtRegexSetCompile           数组一次编译为集合对象
 *   xrtRegexSetMatcherCreate/Free 集合匹配器（持有集合引用）
 *   xrtRegexSetMatcherMatch      对文本执行多规则匹配
 *   xrtRegexSetMatcherCount      命中规则数量
 *   xrtRegexSetMatcherIndex      按命中序号取规则编号
 * 模块宏：XRT_MODULE_REGEX（SET 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/text/regex_set/main.c -lws2_32 -liphlpapi
 * 预期输出（规则编号从 0 起）：
 *   matched rule 1
 *   matched rule 2
 *
 * 集合 vs 逐条匹配：N 条规则一次扫描全判完（共享一次文本遍历），
 *   日志分类、WAF 规则、路由分发的标准武器；
 *   命中可多条（本例 "disk timeout" 同时命中规则 1 与 2），
 *   编号即数组下标——不需要再回查模式文本。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/* 三条分类规则；下标 0/1/2 就是命中报告里的规则编号。 */
	const xstrview arrPattern[] = {
		{ "error", 5u },
		{ "timeout", 7u },
		{ "disk", 4u }
	};
	xregexset* pSet = xrtRegexSetCompile(arrPattern, 3u);
	xregexsetmatcher* pMatcher;
	xregexresult Result;

	if ( pSet == NULL ) {
		return 1;
	}

	/* 匹配器持有集合自身引用：本地引用可先释放。 */
	pMatcher = xrtRegexSetMatcherCreate(pSet);
	xrtRegexSetRelease(pSet);
	if ( pMatcher == NULL ) {
		return 2;
	}

	/* 一次扫描：同时命中 timeout(1) 与 disk(2)。 */
	Result = xrtRegexSetMatcherMatch(
		pMatcher,
		XRT_STR_LITERAL("disk timeout"),
		0
	);
	if ( Result == XREGEX_MATCH ) {
		for ( size_t i = 0; i < xrtRegexSetMatcherCount(pMatcher); i++ ) {
			printf("matched rule %zu\n", xrtRegexSetMatcherIndex(pMatcher, i));
		}
	}
	xrtRegexSetMatcherFree(pMatcher);
	return Result == XREGEX_ERROR ? 3 : 0;
}
