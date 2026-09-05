/*
 * 范例：string/dup_join —— 组装族：Repeat / Join / Filter / Split 一次性版
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtStrRepeat      重复 N 次（拥有式）
 *   xrtStrJoin        分隔符连接视图数组
 *   xrtStrFilter      剔除集合内字节（拥有式结果）
 *   xrtStrSplit       一次性切分为列表（xstrlist，拥有式）
 * 模块宏：XRT_MODULE_STRING
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/string/dup_join/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   repeat=ab-ab-ab-
 *   join=xrt, http, tls
 *   filter=xrtcore
 *   split[0]=alpha split[1]=beta split[2]=gamma count=3
 *
 * Split（一次性）与 split 范例（零分配迭代器 xstrsplit）
 *   是同一分帧的两种消费形态：要列表用本入口，流式用迭代器。
 */

#include <stdio.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

int main(void)
{
	/* 重复：模式含分隔符一次成型。 */
	str sRepeat = xrtStrRepeat(SV("ab-"), 3u);
	printf("repeat=%s\n", sRepeat ? sRepeat : "(null)");
	xrtFree(sRepeat);

	/* 连接：视图数组 + 分隔符一次拼齐。 */
	const xstrview Parts[3] = { SV("xrt"), SV("http"), SV("tls") };
	str sJoin = xrtStrJoin(SV(", "), Parts, 3u);
	printf("join=%s\n", sJoin ? sJoin : "(null)");
	xrtFree(sJoin);

	/* 过滤（拥有式）：剔除所有 ':' 与 '/'。 */
	str sFilter = xrtStrFilter(SV("xrt::core//"), SV(":/"));
	printf("filter=%s\n", sFilter ? sFilter : "(null)");
	xrtFree(sFilter);

	/* 一次性切分：结果 xstrlist（Count + Items[]），用完 xrtStrListFree。 */
	xstrlist* pList = xrtStrSplit(SV("alpha,beta,gamma"), SV(","));
	if ( pList != NULL ) {
		for ( size_t i = 0; i < pList->Count; i++ ) {
			printf("split[%zu]=%.*s ", i,
				(int)pList->Items[i].Size, pList->Items[i].Data);
		}
		printf("count=%zu\n", pList->Count);
		xrtStrListFree(pList);
	}
	return 0;
}
