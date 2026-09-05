/*
 * 范例：charset/unicode —— UTF-8 主线与 UTF-16 平台边界的严格往返
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtUtf8ViewTo16    UTF-8 视图 → 拥有式 UTF-16 数组（严格模式）
 *   xrtUtf16View       由 (指针, 单元数) 组装 UTF-16 视图
 *   xrtUtf16DupView    复制 UTF-16 视图为新的拥有式数组
 *   xrtUtf16ViewTo8    UTF-16 视图 → 拥有式 UTF-8 字符串
 *   xrtUtf8Slice       按"Unicode 标量下标"切 UTF-8（不切断多字节字符）
 *   XRT_STR_LITERAL    编译期构造字符串视图（长度 = sizeof - 1）
 * 模块宏：XRT_MODULE_CHARSET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/charset/unicode/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   UTF-16 units: 9
 *   XRT 你好 😀
 *   scalar slice: 你好
 *
 * 单元数为什么是 9：
 *   标量共 8 个（X R T 空格 你 好 空格 😀），其中 😀 是增补平面字符，
 *   UTF-16 需要一对代理项（2 单元）表示，其余各 1 单元：
 *   7 × 1 + 1 × 2 = 9。iUnits 是"单元数"不是"字符数"。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/* "XRT 你好 😀"：你好 各 3 字节，😀 是 4 字节 emoji。 */
	xstrview Text = XRT_STR_LITERAL("XRT \xE4\xBD\xA0\xE5\xA5\xBD \xF0\x9F\x98\x80");
	uint16* pUtf16;        /* 拥有式：转换出的 UTF-16 数组 */
	uint16* pUtf16Copy;    /* 拥有式：视图复制产物 */
	str sUtf8;             /* 拥有式：回转 UTF-8 */
	size_t iUnits = 0;     /* 出参：UTF-16 单元数 */
	xstrview Word;         /* 借用：标量切片结果 */

	/* 去 UTF-16：严格模式下任何非法序列都会失败并设置线程错误。 */
	pUtf16 = xrtUtf8ViewTo16(Text, XUTF_STRICT, &iUnits);
	if ( pUtf16 == NULL ) {
		return 1;
	}

	/*
	 * 视图复制演练：View 把 (指针, 单元数) 包回视图类型，
	 * Dup 再按视图长度分配并复制——平台 API 边界常需要独立缓冲。
	 */
	pUtf16Copy = xrtUtf16DupView(xrtUtf16View(pUtf16, iUnits));
	if ( pUtf16Copy == NULL ) {
		xrtFree(pUtf16);
		return 1;
	}

	/* 回转 UTF-8：长度出参不需要时传 NULL。 */
	sUtf8 = xrtUtf16ViewTo8(xrtUtf16View(pUtf16, iUnits), XUTF_STRICT, NULL);
	if ( sUtf8 == NULL ) {
		xrtFree(pUtf16Copy);
		xrtFree(pUtf16);
		return 1;
	}
	printf("UTF-16 units: %llu\n%s\n", (unsigned long long)iUnits, sUtf8);

	/*
	 * 标量切片：从第 4 个标量（"你"）起取 2 个标量。
	 * 下标按 Unicode 标量计数，绝不在多字节字符中间切开——
	 * 这是手工指针运算做不到的安全保证。
	 */
	if ( !xrtUtf8Slice(Text, 4, 2, &Word) ) {
		xrtFree(sUtf8);
		xrtFree(pUtf16Copy);
		xrtFree(pUtf16);
		return 1;
	}
	printf("scalar slice: %.*s\n", (int)Word.Size, Word.Data);

	/* 三个拥有式资源逆序释放。 */
	xrtFree(sUtf8);
	xrtFree(pUtf16Copy);
	xrtFree(pUtf16);
	return 0;
}
