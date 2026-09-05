/*
 * 范例：string/pad_trim —— 填充与裁剪族：Pad 三向 × Trim 三向 × Set 变体
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtStrPadLeft / PadCenter        左填充 / 居中填充（宽度 + 填充模式）
 *   xrtStrPadRight                   右填充
 *   xrtStrTrimLeft / TrimRight       裁剪首/尾空白（视图零分配）
 *   xrtStrTrimLeftSet / TrimRightSet 裁剪指定字节集合
 *   xrtStrTrimSet                    双向裁剪集合
 * 模块宏：XRT_MODULE_STRING
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/string/pad_trim/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   pad-left=***id
 *   pad-right=id***
 *   pad-center=*id**
 *   trim=mid
 *   trim-left-set=FF00
 *   trim-right-set=42
 *   trim-set=value
 *
 * Pad 与 charset 的 xrtUtf8PadCenter 区别：这里按字节宽度、
 *   填充模式是字节序列（可任意长）；Utf 版按标量宽度。
 *   Trim 族返回原串子视图，不分配。
 */

#include <stdio.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

static void showOwned(cstr pTag, str s)
{
	printf("%s=%s\n", pTag, s ? s : "(null)");
	xrtFree(s);
}

int main(void)
{
	/* 三向填充：宽度 5、模式 "*"。 */
	showOwned("pad-left", xrtStrPadLeft(SV("id"), 5u, SV("*")));
	showOwned("pad-right", xrtStrPadRight(SV("id"), 5u, SV("*")));
	showOwned("pad-center", xrtStrPadCenter(SV("id"), 5u, SV("*")));

	/* 空白裁剪：双向各来一次（中间的空白保留）。 */
	xstrview Trimmed = xrtStrTrimRight(xrtStrTrimLeft(SV("  mid  ")));
	printf("trim=%.*s\n", (int)Trimmed.Size, Trimmed.Data);

	/* 单侧集合裁剪：只剥左边的 "0x"、只剥右边的单位。 */
	xstrview Hex = xrtStrTrimLeftSet(SV("0xFF00"), SV("0x"));
	printf("trim-left-set=%.*s\n", (int)Hex.Size, Hex.Data);
	xstrview Num = xrtStrTrimRightSet(SV("42 ms"), SV(" ms"));
	printf("trim-right-set=%.*s\n", (int)Num.Size, Num.Data);

	/* 双向集合裁剪：剥掉包裹的引号与花括号——JSON 风格值的预处理。 */
	xstrview Value = xrtStrTrimSet(SV("{ \"value\" }"), SV("{}\" "));
	printf("trim-set=%.*s\n", (int)Value.Size, Value.Data);
	return 0;
}
