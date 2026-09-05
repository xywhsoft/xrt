/*
 * 范例：string/list —— 字符串列表：单块分配与逐条写入
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtStrListAlloc   一次分配"片段数组 + 数据区"单块
 *   xrtStrListWrite   把片段复制进数据区并记录视图
 *   xrtStrListFree    释放整块
 * 模块宏：XRT_MODULE_STRING
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/string/list/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   [0]=alpha [1]=beta [2]=gamma used=16
 *
 * 单块布局的价值：片段视图与文本数据同块分配——
 *   一次 Free 释放全部（无逐条 malloc/free），缓存局部性
 *   也更好。Split 系列内部就是用它构造结果。
 */

#include <stdio.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

int main(void)
{
	const xstrview Source[3] = { SV("alpha"), SV("beta"), SV("gamma") };
	const size_t iDataSize = 6u + 5u + 6u;   /* 各段 + 结尾零：零结尾片段 */

	/* 先规划容量：片段数 3 + 数据区 14 字节，一次分配。 */
	xstrlist* pList = xrtStrListAlloc(3u, iDataSize);
	if ( pList == NULL ) {
		return 1;
	}

	/* 逐条写入：pOffset 在数据区内推进，返回 true 即成功。 */
	size_t iOffset = 0;
	for ( size_t i = 0; i < 3u; i++ ) {
		if ( !xrtStrListWrite(pList, i, Source[i], &iOffset) ) {
			xrtStrListFree(pList);
			return 2;
		}
	}

	for ( size_t i = 0; i < pList->Count; i++ ) {
		printf("[%zu]=%.*s ", i,
			(int)pList->Items[i].Size, pList->Items[i].Data);
	}
	printf("used=%zu\n", iOffset);
	xrtStrListFree(pList);
	return 0;
}
