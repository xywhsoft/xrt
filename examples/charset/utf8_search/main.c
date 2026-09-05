/*
 * 范例：charset/utf8_search —— UTF-8 搜索族：标量下标与大小写变体
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtUtf8Count / Index / Offset / At    标量计数 / 字节↔标量换算 / 取标量
 *   xrtUtf8Find / RFind / CaseFind / CaseRFind   正反向 + 大小写变体
 *   xrtUtf8ContainsAny                    包含集合中任意标量
 * 模块宏：XRT_MODULE_CHARSET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/charset/utf8_search/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   count=5 offset(1)=1 at(1)=U+4F60
 *   find=1 rfind=3 case-find=2 case-rfind=4 index(4)=2
 *   contains-any=1
 *
 * 标量 vs 字节下标：Count 数标量（5）、Offset 标量→字节、
 *   Index 字节→标量（互逆）。"a你x你b"中标量 2（x）的字节
 *   偏移是 4——Offset(2)=4 / Index(4)=2 一对可验证。
 */

#include <stdio.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

int main(void)
{
	xstrview Text = SV("a你x你b");

	/* 计数与换算：5 个标量；标量 1（你）的字节偏移 = 4。 */
	printf("count=%zu", xrtUtf8Count(Text));
	printf(" offset(1)=%zu", xrtUtf8Offset(Text, 1u));
	{
		uint32 iScalar = 0;

		(void)xrtUtf8At(Text, 1u, &iScalar);
		printf(" at(1)=U+%X\n", iScalar);
	}

	/* 正反向与大小写变体：全部返回标量下标。 */
	printf("find=%zu", xrtUtf8Find(Text, SV("你"), 0u));
	printf(" rfind=%zu", xrtUtf8RFind(Text, SV("你")));
	printf(" case-find=%zu\n", xrtUtf8CaseFind(Text, SV("X"), 0u));

	/* 集合包含（Unicode 标量粒度）。 */
	/* 大小写不敏感反向查找 + 标量下标→字节偏移。 */
	printf("case-rfind=%zu", xrtUtf8CaseRFind(Text, SV("B")));
	printf(" index(4)=%zu\n", xrtUtf8Index(Text, 4u));

	printf("contains-any=%d\n",
		xrtUtf8ContainsAny(Text, SV("你x")) ? 1 : 0);
	return 0;
}
