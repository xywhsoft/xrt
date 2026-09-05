/*
 * 范例：string/edit —— 编辑族：Insert/Remove/Replace/ReverseBytes/Slice/Dup 族
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtStrInsert          按字节位置插入子串（拥有式结果）
 *   xrtStrRemove          按字节范围删除
 *   xrtStrReplace         替换全部匹配子串
 *   xrtStrReverseBytes    字节逆序（注意：不是 Unicode 逆序）
 *   xrtStrReverseBytesTo  字节逆序到调用方缓冲（零分配）
 *   xrtStrSlice           按字节位置切子视图（越界钳制）
 *   xrtStrDup / DupN / DupView   复制族（注意：前两个收 cstr 零结尾串，
 *                            DupView 收视图——三种入参形态各有场景）
 * 模块宏：XRT_MODULE_STRING
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/string/edit/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   insert=xrt::core
 *   remove=xrtcore
 *   replace=a-b-c
 *   reverse=abc
 *   reverse-to=abc
 *   slice=bc
 *   dup=hello dupn=wor dupview=bc
 *
 * ReverseBytes 是字节级操作：多字节 UTF-8 会变成非法
 *   序列——Unicode 标量逆序请用 charset 的 xrtUtf8Reverse。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

static void showOwned(cstr pTag, str sText)
{
	printf("%s=%s\n", pTag, sText ? sText : "(null)");
	xrtFree(sText);
}

int main(void)
{
	char Buffer[8];

	showOwned("insert", xrtStrInsert(SV("xrtcore"), 3u, SV("::")));
	showOwned("remove", xrtStrRemove(SV("xrt::core"), 3u, 2u));
	showOwned("replace", xrtStrReplace(SV("a.b.c"), SV("."), SV("-")));

	/* 字节逆序双变体：拥有式（cba）再逆回 abc；缓冲式零分配。 */
	showOwned("reverse", xrtStrReverseBytes(SV("abc")));
	if ( xrtStrReverseBytesTo(SV("cba"), Buffer, sizeof(Buffer)) ) {
		printf("reverse-to=%s\n", Buffer);   /* Buffer 是栈缓冲，不释放 */
	}

	/* Slice：起始 1 取 2 字节 → "bc"；越界自动钳制。 */
	xstrview Part = xrtStrSlice(SV("abcd"), 1u, 2u);
	printf("slice=%.*s\n", (int)Part.Size, Part.Data);

	/* 复制族：Dup 补结尾零 / DupN 只取前 N / DupView 按视图长度。 */
	str sDup = xrtStrDup("hello");
	str sDupN = xrtStrDupN("world", 3u);
	str sDupView = xrtStrDupView(Part);
	printf("dup=%s dupn=%s dupview=%s\n",
		sDup ? sDup : "?", sDupN ? sDupN : "?", sDupView ? sDupView : "?");
	xrtFree(sDup);
	xrtFree(sDupN);
	xrtFree(sDupView);
	return 0;
}
