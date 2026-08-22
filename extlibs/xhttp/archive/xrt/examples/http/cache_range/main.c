#include <xrt/http_cache_range.h>

#include <stdio.h>



/* 展示收到第二段 206 后如何生成规范覆盖替换计划。 */
int main(void)
{
	static const xhttpfield StoredFields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset-v1\"")
		},
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("10")
		}
	};
	static const xhttpfield IncomingFields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset-v1\"")
		},
		{
			XRT_STR_INIT("Content-Range"),
			XRT_STR_INIT("bytes 5-9/10")
		},
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("5")
		}
	};
	xhttpcachefragment Stored = {
		{
			StoredFields,
			sizeof(StoredFields) /
				sizeof(StoredFields[0]),
			0,
			XHTTP_CACHE_ENTRY_PARTIAL
		},
		{ 0, 4 },
		10,
		XHTTP_STATUS_OK,
		XHTTP_CACHE_FRAGMENT_HAS_RANGE |
			XHTTP_CACHE_FRAGMENT_HAS_LENGTH
	};
	xhttpcachefragmentinput Input;
	xhttpcachefragmentplan Fragment;
	xhttpcachecombineplan Combine;

	xrtHttpCacheFragmentInputInit(&Input);
	Input.Method = XRT_STR_LITERAL("GET");
	Input.Fields = IncomingFields;
	Input.FieldCount =
		sizeof(IncomingFields) /
			sizeof(IncomingFields[0]);
	Input.Status = XHTTP_STATUS_PARTIAL_CONTENT;
	Input.BodySize = 5;
	Input.Flags =
		XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
		XHTTP_CACHE_FRAGMENT_BODY_COMPLETE;
	if ( xrtHttpCacheFragmentPlan(
		&Input, &Fragment
	) != XHTTP_CACHE_FRAGMENT_STORE ) {
		return 1;
	}
	if ( xrtHttpCacheCombinePlan(
		&Stored,
		1,
		&Fragment.Fragment,
		&Combine
	) != XHTTP_CACHE_COMBINE_APPLY ) {
		return 1;
	}

	printf(
		"replace=%zu+%zu range=%llu-%llu complete=%s\n",
		Combine.Index,
		Combine.RemoveCount,
		(unsigned long long)Combine.Range.First,
		(unsigned long long)Combine.Range.Last,
		Combine.Complete ? "yes" : "no"
	);
	return Combine.Complete ? 0 : 1;
}
