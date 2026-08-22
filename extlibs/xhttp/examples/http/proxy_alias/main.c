#include <stdio.h>

#include <xrt/http_proxy_status.h>



/* 迭代并解码 Proxy-Status 的 RFC 9532 下一跳别名。 */
int main(void)
{
	xhttpproxyaliascursor Cursor;
	xstrview Alias;
	char arrName[128];
	size_t iSize;

	xrtHttpProxyAliasCursorInit(&Cursor);
	while ( xrtHttpProxyAliasNext(
		XRT_STR_LITERAL(
			"tracker.example,comma%2Cname.example"
		), &Cursor, &Alias
	) == XHTTP_NEXT_ITEM ) {
		if ( !xrtHttpProxyAliasRead(
			Alias, arrName, sizeof(arrName), &iSize
		) ) {
			return 1;
		}
		printf("alias = %.*s\n", (int)iSize, arrName);
	}
	return xrtGetError() == NULL ? 0 : 1;
}
