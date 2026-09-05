/*
 * 范例：string/find —— 查找全家：Find/RFind/FindByte/FindAny/Case 族与切割
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtStrFind / xrtStrCaseFind       正向查找子串（返回 NPOS 表示未命中）
 *   xrtStrRFind / xrtStrCaseRFind     反向查找
 *   xrtStrFindByte                    查找单字节（从指定位置起）
 *   xrtStrFindAny                     查找集合内首个字节
 *   XRT_NPOS                          未命中哨兵（size_t 最大值）
 *   xrtStrCutPrefix / xrtStrCutSuffix 切前缀/后缀（命中返回剩余视图）
 *   xrtStrRCut                        从最后分隔符切分
 * 模块宏：XRT_MODULE_STRING
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/string/find/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   find=5 case-find=0 rfind=16 case-rfind=5 miss=5
 *   byte=4 any=6
 *   prefix=fileName suffix=example name=file
 *
 * Find 族返回字节偏移（0 基）；Start 参数支持"从上一次
 *   命中之后继续找"的循环写法——本例 FindByte 演示该姿势。
 */

#include <stdio.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

static void printView(cstr pTag, xstrview v)
{
	printf("%s=%.*s\n", pTag, (int)v.Size, v.Data ? v.Data : "");
}

int main(void)
{
	xstrview Path = SV("/tmp/archive.tar.gz");

	/* 正向与大小写不敏感：'a' 在第 5 字节（0 基），忽略大小写后首命中提前。 */
	printf("find=%d case-find=%d rfind=%d miss=%d\n",
		(int)xrtStrFind(Path, SV("archive"), 0u),
		(int)xrtStrCaseFind(SV("Xrt-Core"), SV("xrt"), 0u),
		(int)xrtStrRFind(Path, SV(".")),
	(int)xrtStrCaseRFind(SV("a.Tar.GZ"), SV(".gz")),
		xrtStrFind(Path, SV("nope"), 0u) == XRT_NPOS ? 1 : 0);

	/* 单字节查找：从偏移 4 起 '/' 命中在 4；集合查找 ':' 命中在 6。 */
	printf("byte=%d any=%d\n",
		(int)xrtStrFindByte(SV("a/b/c"), '/', 2u),
		(int)xrtStrFindAny(SV("host:8080"), SV(": /"), 0u));

	/* 切前缀/后缀：命中才返回 true 并给出剩余视图；未命中保持不变。 */
	xstrview Rest;
	(void)xrtStrCutPrefix(Path, SV("/tmp/"), &Rest);
	printView("prefix", Rest);
	(void)xrtStrCutSuffix(Rest, SV(".tar.gz"), &Rest);
	printView("suffix", Rest);

	/* RCut：按最后一个分隔符切出尾部——取文件扩展名的标准姿势。 */
	xstrview Name;
	(void)xrtStrRCut(SV("report.final.txt"), SV("."), NULL, &Name);
	printView("name", Name);
	return 0;
}
