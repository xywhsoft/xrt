#include <stdio.h>
#include <string.h>

#include <xrt/http_proxy_status.h>



/* 生成一个可作为独立字段行追加的代理错误成员。 */
int main(void)
{
	xhttpstructuredparameterentry Error;
	xhttpstructureditemvalue Item;
	char arrValue[64];
	size_t iSize;

	memset(&Error, 0, sizeof(Error));
	Error.Key = XRT_STR_LITERAL("error");
	Error.Value.Type = XHTTP_STRUCTURED_TOKEN;
	Error.Value.Data = XRT_STR_LITERAL("connection_timeout");
	memset(&Item, 0, sizeof(Item));
	Item.Bare.Type = XHTTP_STRUCTURED_TOKEN;
	Item.Bare.Data = XRT_STR_LITERAL("EdgeProxy");
	Item.Parameters = &Error;
	Item.ParameterCount = 1u;
	if ( !xrtHttpProxyStatusWrite(
		&Item, arrValue, sizeof(arrValue), &iSize
	) ) {
		return 1;
	}
	printf("Proxy-Status: %.*s\n", (int)iSize, arrValue);
	return 0;
}
