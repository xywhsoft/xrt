#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证 CORS 预检缓存模块可单头文件独立使用。 */
int main(void)
{
	xhttporigin Origin;
	xhttpcorscachekey Key;
	xhttpcorscache* pCache;

	if ( !xrtHttpOriginParse(
		XRT_STR_LITERAL("https://app.example"),
		&Origin
	) || !xrtHttpCorsCacheKeyInit(
		&Key,
		&Origin,
		XRT_STR_LITERAL("https://api.example/data")
	) ) {
		return 1;
	}
	pCache = xrtHttpCorsCacheCreate(NULL);
	if ( pCache == NULL ) {
		return 2;
	}
	xrtHttpCorsCacheRelease(pCache);
	return 0;
}
