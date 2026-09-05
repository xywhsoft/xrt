/*
 * 范例：http/token_tour —— 令牌列表与加权令牌族
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttpTokenValid / TokenEqual         单 token 谓词
 *   xrtHttpTokenListHas / TokenListCount   列表包含 / 计数
 *   xrtHttpTokenListBuild / TokenListWrite 视图数组 → 逗号列表
 *   xrtHttpWeightedTokenNext               加权令牌迭代（;q= 权重）
 *   xrtHttpOwsTrim                         可选横向空白裁剪
 * 模块宏：XRT_MODULE_HTTP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/http/token_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   valid=1 eq=1 has=1 count=3
 *   build=gzip,deflate,br listwrite=gzip,deflate
 *   weighted: gzip(500) deflate(250) identity(1000)
 *   trim=[value]
 *
 * 加权令牌 = Accept/TE 系头的核心语法：token;q=0.5——
 *   WeightedTokenNext 一次交出 token 与千分位权重（无标注默认 1000）。
 */

#include <stdio.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

int main(void)
{
	static const char sList[] = "gzip, deflate, br";
	static const char sWeighted[] = "gzip;q=0.5, deflate;q=0.25, identity";
	static const xstrview Tokens[3] = {
		{ (cstr)"gzip", 4u }, { (cstr)"deflate", 7u }, { (cstr)"br", 2u }
	};
	size_t iOffset = 0;
	size_t iCount = 0;
	size_t iSize = 0;
	char Buffer[64];
	str sBuilt;
	xstrview Trimmed;

	/* 单 token 谓词。 */
	printf("valid=%d", xrtHttpTokenValid(SV("gzip")) ? 1 : 0);
	printf(" eq=%d\n", xrtHttpTokenEqual(SV("GZIP"), SV("gzip")) ? 1 : 0);

	/* 列表包含与计数。 */
	printf("has=%d", xrtHttpTokenListHas(SV(sList), SV("deflate")) ? 1 : 0);
	if ( xrtHttpTokenListCount(SV(sList), &iCount) ) {
		printf(" count=%zu\n", iCount);
	}

	/* 列表构建：视图数组 → 逗号分隔（Build 拥有式 / Write 缓冲）。 */
	sBuilt = xrtHttpTokenListBuild(Tokens, 3u, NULL);
	printf("build=%s\n", sBuilt ? sBuilt : "?");
	xrtFree(sBuilt);
	if ( xrtHttpTokenListWrite(Tokens, 2u, Buffer, sizeof(Buffer), &iSize) ) {
		Buffer[iSize] = 0;
		printf("listwrite=%s\n", Buffer);
	}

	/* 加权令牌迭代。 */
	{
		xhttpweightedtoken Item;

		printf("weighted:");
		while ( xrtHttpWeightedTokenNext(SV(sWeighted), &iOffset,
			&Item) == XHTTP_NEXT_ITEM ) {
			printf(" %.*s(%u)", (int)Item.Token.Size, Item.Token.Data,
				(unsigned)Item.Quality);
		}
		printf("\n");
	}

	/* OWS 裁剪（返回借用视图）。 */
	Trimmed = xrtHttpOwsTrim(SV("  value  "));
	printf("trim=[%.*s]\n", (int)Trimmed.Size, Trimmed.Data);
	return 0;
}
