/*
 * 范例：text/regex_split —— 流式分割：任意正则作分隔符、跳过空段
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtRegexSplitConfigInit     默认分割配置（保留空段）
 *   XREGEX_SPLIT_SKIP_EMPTY     标志：跳过空段（本例启用）
 *   xrtRegexSplitterCreate/Free 流式分割器（借用文本与正则引用）
 *   xrtRegexSplitterNext        逐段取值（三态 MATCH/NONE/ERROR）
 * 模块宏：XRT_MODULE_REGEX（SPLIT 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/text/regex_split/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   alpha
 *   beta
 *   gamma
 *
 * 分隔符是正则：[,;]\s* 一次处理逗号/分号 + 任意空白——
 *   手写 strtok 要循环两遍且破坏原串；Splitter 只借用文本、
 *   零拷贝出段视图。整段收集可用 xrtRegexSplit（直接返回列表）。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/* 分隔符：逗号或分号后跟任意空白。 */
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("[,;]\\s*"));
	xregexsplitconfig Config;
	xregexsplitter* pSplitter;
	xregexsplitpart Part;
	xregexresult Result;

	if ( pRegex == NULL ) {
		return 1;
	}
	xrtRegexSplitConfigInit(&Config);
	Config.Flags = XREGEX_SPLIT_SKIP_EMPTY;   /* "a,,b" 不会产出空段 */

	/* 分割器持有正则引用：本地引用可先释放。 */
	pSplitter = xrtRegexSplitterCreate(
		pRegex,
		XRT_STR_LITERAL("alpha, beta; gamma"),
		&Config
	);
	xrtRegexRelease(pRegex);
	if ( pSplitter == NULL ) {
		return 2;
	}

	/* 逐段消费：Part.Text 是借用视图，到下一次 Next 前有效。 */
	while ( (Result = xrtRegexSplitterNext(pSplitter, &Part)) == XREGEX_MATCH ) {
		printf("%.*s\n", (int)Part.Text.Size, Part.Text.Data);
	}
	xrtRegexSplitterFree(pSplitter);
	return Result == XREGEX_ERROR ? 3 : 0;
}
