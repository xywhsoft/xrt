#include <xrt/http_cache_validate.h>

#include <stdio.h>



/* 展示缓存条目验证、304 选择和 unsafe 方法失效的协议计划。 */
int main(void)
{
	static const xhttpfield StoredFields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset-v2\"")
		},
		{
			XRT_STR_INIT("Last-Modified"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:35 GMT")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		}
	};
	static const xhttpfield NotModified[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset-v2\"")
		}
	};
	static const xhttpfield Changed[] = {
		{
			XRT_STR_INIT("Location"),
			XRT_STR_INIT("/assets/current")
		}
	};
	xhttpcacheentry Entry = {
		StoredFields,
		sizeof(StoredFields) / sizeof(StoredFields[0]),
		0,
		XHTTP_CACHE_ENTRY_NONE
	};
	const xstrview Target =
		XRT_STR_LITERAL("https://example.test/assets/2");
	xhttpcachevalidateplan Validate;
	xhttpcacheinvalidatecursor Cursor;
	xhttpcacheinvalidateitem Item;
	size_t Indices[1];
	size_t iSelected = 0;
	size_t iSize = 0;
	char Value[128];

	if ( (xrtHttpCacheValidatePlan(
		&Entry, 1, false, &Validate
	) != XHTTP_CACHE_VALIDATE_CONDITIONAL) ||
		!xrtHttpCacheValidateETagsWrite(
			&Entry, 1, false,
			Value, sizeof(Value), &iSize
		) ) {
		return 1;
	}
	printf(
		"If-None-Match: %.*s\n",
		(int)iSize, Value
	);
	if ( xrtHttpCache304Select(
		NotModified, 1,
		&Entry, 1,
		Indices, 1,
		&iSelected
	) != XHTTP_CACHE_UPDATE_MATCH_STRONG ) {
		return 1;
	}
	printf("304 updates entry %u\n", (unsigned)Indices[0]);

	xrtHttpCacheInvalidationCursorInit(&Cursor);
	while ( xrtHttpCacheInvalidationNext(
		XRT_STR_LITERAL("POST"),
		200,
		Target,
		Changed,
		1,
		&Cursor,
		&Item
	) == XHTTP_NEXT_ITEM ) {
		if ( !xrtHttpCacheInvalidationWrite(
			Target, &Item,
			Value, sizeof(Value), &iSize
		) ) {
			return 1;
		}
		printf(
			"invalidate: %.*s\n",
			(int)iSize, Value
		);
	}
	return 0;
}
