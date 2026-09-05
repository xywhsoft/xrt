/*
 * 范例：string/distance —— UTF-8 编辑距离与归一化相似度
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtUtf8Distance    编辑距离（按 Unicode 标量，非字节）
 *   xrtUtf8Similarity  归一化相似度 0..1（1 = 完全相同）
 *   XRT_NPOS           距离上限参数（不限制 / 失败哨兵）
 * 模块宏：XRT_MODULE_STRING
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/string/distance/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   distance=2 similarity=0.600
 *
 * 读数解释："网络客户端" vs "网络服务端"——
 *   5 个标量中前两后一相同，中间 客↔服、户↔务 各差一次替换
 *   → 距离 2；相似度 = 1 - 2/5 = 0.6。
 * 按标量而非字节比较：汉字占 3 字节，按字节算会得到失真的
 *   大距离；距离上限参数（XRT_NPOS=不限）可传小值提前剪枝，
 *   "只需判断是否足够相似"的场景能省一半计算。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	xstrview Left = XRT_STR_LITERAL("网络客户端");
	xstrview Right = XRT_STR_LITERAL("网络服务端");

	/* 距离上限不限制；返回 XRT_NPOS 表示失败（如非法 UTF-8）。 */
	size_t iDistance = xrtUtf8Distance(Left, Right, XRT_NPOS);
	double fSimilarity = xrtUtf8Similarity(Left, Right);

	if ( (iDistance == XRT_NPOS) || (fSimilarity < 0.0) ) {
		return 1;
	}
	printf("distance=%llu similarity=%.3f\n",
		(unsigned long long)iDistance, fSimilarity);
	return 0;
}
