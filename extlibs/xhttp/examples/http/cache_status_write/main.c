#include <stdio.h>
#include <string.h>

#include <xrt/http_cache_status.h>



/* 生成一个可作为独立字段行追加的缓存命中成员。 */
int main(void)
{
	xhttpstructuredparameterentry Hit;
	xhttpstructureditemvalue Item;
	char arrValue[64];
	size_t iSize;

	memset(&Hit, 0, sizeof(Hit));
	Hit.Key = XRT_STR_LITERAL("hit");
	Hit.Value.Type = XHTTP_STRUCTURED_BOOLEAN;
	Hit.Value.Number = 1;
	memset(&Item, 0, sizeof(Item));
	Item.Bare.Type = XHTTP_STRUCTURED_TOKEN;
	Item.Bare.Data = XRT_STR_LITERAL("EdgeCache");
	Item.Parameters = &Hit;
	Item.ParameterCount = 1u;
	if ( !xrtHttpCacheStatusWrite(
		&Item, arrValue, sizeof(arrValue), &iSize
	) ) {
		return 1;
	}
	printf("Cache-Status: %.*s\n", (int)iSize, arrValue);
	return 0;
}
