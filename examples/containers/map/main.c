#include <stdio.h>
#include <xrt.h>



/* 路由统计直接保存在由二进制路径键索引的内联值槽中。 */
typedef struct routestat {
	size_t Requests;
	int Status;
} routestat;



/* 演示字面量键、零初始化值槽和确定的插入顺序迭代。 */
int main(void)
{
	xmap tRoutes;
	xmapiter tIterator;
	xbytesview Path;
	routestat* pStat;
	bool bNew;

	if ( !xrtMapInit(&tRoutes, sizeof(routestat)) ) {
		return 1;
	}
	pStat = (routestat*)xrtMapGetOrAdd(&tRoutes, XRT_BYTES_LITERAL("/health"), &bNew);
	if ( pStat == NULL ) {
		xrtMapUnit(&tRoutes);
		return 2;
	}
	pStat->Requests++;
	pStat->Status = 200;

	pStat = (routestat*)xrtMapGetOrAdd(&tRoutes, XRT_BYTES_LITERAL("/metrics"), &bNew);
	if ( pStat == NULL ) {
		xrtMapUnit(&tRoutes);
		return 3;
	}
	pStat->Requests = 8;
	pStat->Status = 204;

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
