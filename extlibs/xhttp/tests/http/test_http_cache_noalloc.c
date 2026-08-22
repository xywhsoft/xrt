#include "../test_allocator.h"

#include <xrt/http_cache.h>



/* 验证游标、增量汇总和数值读取不触发堆分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT(
				"max-age=\"60\", no-transform, x-mode=fast"
			)
		},
		{
			XRT_STR_INIT("cache-control"),
			XRT_STR_INIT("private=\"Authorization\"")
		}
	};
	xhttpcachecontrol Control;
	xhttpcachecursor Cursor;
	xhttpcacheitem Item;
	uint64 iSeconds = 0;
	bool bPass;

	testRequire(
		testInstallFailAllocator(),
		"HTTP cache failure allocator install failed"
	);
	xrtHttpCacheCursorInit(&Cursor);
	bPass = xrtHttpCacheControlParse(
		Fields, 2, &Control
	) && xrtHttpCacheControlValid(&Control) &&
		(Control.UnknownCount == 1) &&
		((Control.Flags & XHTTP_CACHE_NO_TRANSFORM) != 0) &&
		(xrtHttpCacheNext(
			Fields, 2, &Cursor, &Item
		 ) == XHTTP_NEXT_ITEM) &&
		xrtHttpCacheDeltaRead(&Item, &iSeconds) &&
		(iSeconds == 60);
	testRequire(
		bPass,
		"HTTP Cache-Control parsing allocated memory"
	);
	printf("[PASS] http_cache_noalloc\n");
	return 0;
}
