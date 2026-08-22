#include <stdio.h>

#include <xrt.h>



/* 演示按 Unicode 标量计算编辑距离和归一化相似度。 */
int main(void)
{
	xstrview Left = XRT_STR_LITERAL("网络客户端");
	xstrview Right = XRT_STR_LITERAL("网络服务端");
	size_t iDistance = xrtUtf8Distance(Left, Right, XRT_NPOS);
	double fSimilarity = xrtUtf8Similarity(Left, Right);

	if ( (iDistance == XRT_NPOS) || (fSimilarity < 0.0) ) {
		return 1;
	}
	printf("distance=%llu similarity=%.3f\n",
		(unsigned long long)iDistance, fSimilarity);
	return 0;
}
