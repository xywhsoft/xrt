/*
 * 范例：containers/map —— 字节键哈希映射：GetOrAdd 与插入序迭代
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtMapInit        按值大小初始化（值内联存储，零逐条分配）
 *   xrtMapGetOrAdd    取值槽；键不存在则零初始化插入并返回槽指针
 *   xrtMapIterBegin   开始确定顺序（插入序）迭代
 *   xrtMapIterNext    步进迭代：同时交出键视图与值指针
 *   xrtMapIterEnd     结束迭代（释放快照引用）
 * 模块宏：XRT_MODULE_MAP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/map/main.c -lws2_32 -liphlpapi
 * 预期输出（按插入顺序，与哈希值无关）：
 *   /health requests=1 status=200
 *   /metrics requests=8 status=204
 *
 * 两个关键性质：
 *   - 键是 xbytesview：任意二进制（含内嵌零）都能当键，
 *     本例直接用路径字面量；
 *   - 新插入的值槽已清零：计数场景直接 ++ 即可，
 *     不需要"先 Has 再 Add 再初始化"三步。
 */

#include <stdio.h>
#include <xrt.h>



/* 路由统计：整条结构作为值内联在槽中（不是指针）。 */
typedef struct routestat {
	size_t Requests;
	int Status;
} routestat;



int main(void)
{
	xmap tRoutes;
	xmapiter tIterator;
	xbytesview Path;         /* 借用：迭代时交出的键视图 */
	routestat* pStat;
	bool bNew;               /* 出参：本次是否新插入 */

	if ( !xrtMapInit(&tRoutes, sizeof(routestat)) ) {
		return 1;
	}

	/*
	 * GetOrAdd 一步完成"查找或创建"：
	 *   首次访问 /health → 新建零初始化槽（bNew=true），
	 *   返回槽指针后直接累加、写状态码。
	 */
	pStat = (routestat*)xrtMapGetOrAdd(&tRoutes, XRT_BYTES_LITERAL("/health"), &bNew);
	if ( pStat == NULL ) {
		xrtMapUnit(&tRoutes);
		return 2;
	}
	pStat->Requests++;
	pStat->Status = 200;

	/* 第二个路由同样一步就位。 */
	pStat = (routestat*)xrtMapGetOrAdd(&tRoutes, XRT_BYTES_LITERAL("/metrics"), &bNew);
	if ( pStat == NULL ) {
		xrtMapUnit(&tRoutes);
		return 3;
	}
	pStat->Requests = 8;
	pStat->Status = 204;

	/*
	 * 迭代：顺序 = 插入顺序（内部维护顺序链表），
	 * 与键的哈希值无关——输出可以稳定复现，适合配置/报表场景。
	 */
	if ( !xrtMapIterBegin(&tRoutes, &tIterator) ) {
		xrtMapUnit(&tRoutes);
		return 4;
	}
	while ( (pStat = (routestat*)xrtMapIterNext(&tIterator, &Path)) != NULL ) {
		printf(
			"%.*s requests=%zu status=%d\n",
			(int)Path.Size,
			(const char*)Path.Data,
			pStat->Requests,
			pStat->Status
		);
	}
	xrtMapIterEnd(&tIterator);
	xrtMapUnit(&tRoutes);
	return 0;
}
