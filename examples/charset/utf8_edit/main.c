/*
 * 范例：charset/utf8_edit —— UTF-8 编辑族：插入/删除/裁剪/填充/反转/过滤
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtUtf8Insert / Remove / Substr   按标量位置插入/删除/取子串
 *   xrtUtf8TrimSet / TrimLeftSet / TrimRightSet   集合裁剪（标量粒度）
 *   xrtUtf8PadLeft / PadRight         按标量宽度填充
 *   xrtUtf8ReverseTo                  反转到缓冲
 *   xrtUtf8FilterTo                   剔除集合标量到缓冲
 * 模块宏：XRT_MODULE_CHARSET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/charset/utf8_edit/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   insert=aX你x
 *   remove=ax
 *   substr=你x
 *   trim=[你]
 *   pad=**a
 *   reverse=(见运行)
 *   filter=ax
 *
 * 全部操作以 Unicode 标量为原子（与 string 模块的字节版互补）；
 *   Remove/Substr 支持负数倒序下标。
 */

#include <stdio.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

static void show(cstr pTag, str s)
{
	printf("%s=%s\n", pTag, s ? s : "(null)");
	xrtFree(s);
}

int main(void)
{
	char Buffer[16];
	size_t iSize = 0;

	/* Insert：在标量位置 1 之后插入 X。 */
	show("insert", xrtUtf8Insert(SV("a你x"), 1, SV("X")));
	/* Remove：从标量 1 起删 1 个（去掉"你"）。 */
	show("remove", xrtUtf8Remove(SV("a你x"), 1, 1));
	/* Substr：从标量 1 起取 2 个。 */
	show("substr", xrtUtf8Substr(SV("a你x"), 1, 2));

	/* 集合裁剪三件套（双向 + 单侧）。 */
	{
		xstrview Trimmed;

		(void)xrtUtf8TrimSet(SV(" 你x"), SV(" x"), &Trimmed);
		printf("trim=[%.*s]\n", (int)Trimmed.Size, Trimmed.Data);
		(void)xrtUtf8TrimLeftSet(SV(" 你x"), SV(" x"), &Trimmed);
		(void)xrtUtf8TrimRightSet(SV(" 你x"), SV(" x"), &Trimmed);
	}

	/* 标量宽度填充（左 + 右）。 */
	show("pad", xrtUtf8PadLeft(SV("a"), 3u, SV("*")));
	show("pad-r", xrtUtf8PadRight(SV("a"), 3u, SV("*")));

	/* ReverseTo：反转到调用方缓冲。 */
	if ( xrtUtf8ReverseTo(SV("ab"), Buffer, sizeof(Buffer)) ) {
		Buffer[iSize] = 0;
		printf("reverse=%s\n", Buffer);
	}

	/* FilterTo：剔除集合标量到缓冲。 */
	{
		xstrview Text = SV("a你x");

		if ( xrtUtf8FilterTo(Text, SV("你"), Buffer, sizeof(Buffer),
			&iSize) ) {
			Buffer[iSize] = 0;
			printf("filter=%s\n", Buffer);
		}
	}
	return 0;
}
