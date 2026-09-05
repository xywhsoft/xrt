/*
 * 范例：containers/set —— 值类型集合：去重、插入序迭代与交集运算
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtSetInit          按元素大小初始化两个集合
 *   xrtSetAdd           添加元素（已存在则保持唯一）
 *   xrtSetIntersection  求交集，返回堆分配的新集合（xset*）
 *   xrtSetIterBegin/Next/End  按插入顺序迭代
 *   xrtSetDestroy       释放运算结果（它是指针，不是栈句柄）
 *   xrtSetUnit          归还栈句柄集合
 * 模块宏：XRT_MODULE_SET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/set/main.c -lws2_32 -liphlpapi
 * 预期输出（交集按"左操作数"的插入序）：
 *   allowed port: 443
 *   allowed port: 8080
 *
 * 场景：防火墙/白名单——启用端口 ∩ 请求端口 = 实际放行端口
 *   （3000 未启用被过滤，80/443/8080 中只有交集存活）。
 * 运算族还有 Union / Difference / SymmetricDifference，
 * 全部返回新分配集合，用完必须 Destroy。
 */

#include <stdio.h>
#include <xrt.h>



int main(void)
{
	xset tEnabled;           /* 栈句柄集合 A：已启用端口 */
	xset tRequested;         /* 栈句柄集合 B：请求端口 */
	xset* pAllowed = NULL;   /* 堆分配结果：A ∩ B */
	xsetiter tIterator;
	const int* pPort;
	int arrEnabled[] = { 80, 443, 8080 };
	int arrRequested[] = { 443, 3000, 8080 };
	int iResult = 0;

	if ( !xrtSetInit(&tEnabled, sizeof(int)) ) {
		return 1;
	}
	if ( !xrtSetInit(&tRequested, sizeof(int)) ) {
		xrtSetUnit(&tEnabled);
		return 1;
	}

	/* 装载两个集合；Add 天然去重（重复元素不增加数量）。 */
	for ( size_t i = 0; i < 3; i++ ) {
		if ( !xrtSetAdd(&tEnabled, &arrEnabled[i]) ||
			!xrtSetAdd(&tRequested, &arrRequested[i]) ) {
			iResult = 2;
			goto cleanup;
		}
	}

	/* 交集：请求 ∩ 启用 = {443, 8080}；结果是新分配的堆集合。 */
	pAllowed = xrtSetIntersection(&tRequested, &tEnabled);
	if ( pAllowed == NULL ) {
		iResult = 3;
		goto cleanup;
	}

	/* 按插入顺序（即请求集合中的出现顺序）打印放行端口。 */
	if ( !xrtSetIterBegin(pAllowed, &tIterator) ) {
		iResult = 4;
		goto cleanup;
	}
	while ( (pPort = (const int*)xrtSetIterNext(&tIterator)) != NULL ) {
		printf("allowed port: %d\n", *pPort);
	}
	xrtSetIterEnd(&tIterator);

cleanup:
	/* 两类句柄两类收尾：堆结果 Destroy，栈句柄 Unit。 */
	if ( pAllowed != NULL ) {
		xrtSetDestroy(pAllowed);
	}
	xrtSetUnit(&tRequested);
	xrtSetUnit(&tEnabled);
	return iResult;
}
