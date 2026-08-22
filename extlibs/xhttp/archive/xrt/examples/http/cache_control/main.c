#include <xrt/http_cache.h>

#include <stdio.h>



/* 展示重复字段汇总和扩展指令的底层遍历。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT("max-age=60, no-transform")
		},
		{
			XRT_STR_INIT("cache-control"),
			XRT_STR_INIT("x-storage=memory")
		}
	};
	xhttpcachecontrol Control;
	xhttpcachecursor Cursor;
	xhttpcacheitem Item;
	xhttpnext Next;

	if ( !xrtHttpCacheControlParse(
		Fields,
		sizeof(Fields) / sizeof(Fields[0]),
		&Control
	) ) {
		return 1;
	}
	printf(
		"max-age=%llu extensions=%zu no-transform=%s\n",
		(unsigned long long)Control.MaxAge,
		Control.UnknownCount,
		(Control.Flags & XHTTP_CACHE_NO_TRANSFORM) != 0 ?
			"yes" : "no"
	);
	xrtHttpCacheCursorInit(&Cursor);
	while ( (Next = xrtHttpCacheNext(
		Fields,
		sizeof(Fields) / sizeof(Fields[0]),
		&Cursor,
		&Item
	)) == XHTTP_NEXT_ITEM ) {
		if ( Item.Directive == XHTTP_CACHE_UNKNOWN ) {
			printf(
				"extension=%.*s\n",
				(int)Item.Name.Size,
				Item.Name.Data
			);
		}
	}
	return Next == XHTTP_NEXT_END ? 0 : 1;
}
