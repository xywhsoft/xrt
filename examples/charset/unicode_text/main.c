/*
 * 范例：charset/unicode_text —— 按标量处理 UTF-8：反转/过滤/区间/居中填充
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtUtf8Reverse     按 Unicode 标量反转（多字节字符不被拆散）
 *   xrtUtf8Filter      删除出现在"字符集"中的标量，保留其余
 *   xrtUtf8Range       按标量下标取区间（支持负数倒序）
 *   xrtUtf8PadCenter   用任意"填充模式串"居中填充到目标标量宽度
 * 模块宏：XRT_MODULE_CHARSET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/charset/unicode_text/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   😀 好你 TRX
 *   你XRT
 *   😀
 *   好😀XRT好😀
 *
 * 四个操作全部以"标量"为原子，而不是字节：
 *   - Reverse：8 个标量整体倒序，emoji 仍是完整的 4 字节；
 *   - Filter：从"你好，XRT"删掉"，好"两个标量，剩"你XRT"；
 *   - Range：-2 表示倒数第 2 个标量（A你😀B 中即 😀），取 1 个；
 *   - PadCenter：目标宽 7 标量，"XRT"占 3，左右各补"好😀"模式。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	str sText;
	str sFiltered;
	str sPadded;
	xstrview Range;        /* 借用：区间结果 */

	/* 反转：标量级倒序。若按字节反转，emoji 与汉字都会变成非法序列。 */
	sText = xrtUtf8Reverse(XRT_STR_LITERAL("XRT 你好 😀"));
	if ( sText == NULL ) {
		return 1;
	}
	printf("%s\n", sText);
	xrtFree(sText);

	/* 过滤：第二参数是"要删除的标量集合"（可含任意多字节字符）。 */
	sFiltered = xrtUtf8Filter(XRT_STR_LITERAL("你好，XRT"),
		XRT_STR_LITERAL("，好"));
	if ( sFiltered == NULL ) {
		return 2;
	}
	printf("%s\n", sFiltered);
	xrtFree(sFiltered);

	/*
	 * 区间：负下标从末尾数（-1 = 最后一个标量）。
	 * "A你😀B" 标量序为 A 你 😀 B；-2 起 1 个 → "😀"。
	 * 输出借用视图，不需要释放。
	 */
	if ( !xrtUtf8Range(XRT_STR_LITERAL("A你😀B"), -2, 1, &Range) ) {
		return 3;
	}
	printf("%.*s\n", (int)Range.Size, Range.Data);

	/*
	 * 居中填充：目标宽度 7 个标量，"XRT" 只有 3 个，
	 * 不足部分轮流用模式串"好😀"填充：左"好😀" + XRT + 右"好😀"。
	 * 填充单位也是标量——用 emoji 填充不会产生半个代理或半个汉字。
	 */
	sPadded = xrtUtf8PadCenter(XRT_STR_LITERAL("XRT"), 7,
		XRT_STR_LITERAL("好😀"));
	if ( sPadded == NULL ) {
		return 4;
	}
	printf("%s\n", sPadded);
	xrtFree(sPadded);
	return 0;
}
