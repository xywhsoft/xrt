#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 RFC 9532 别名迭代与解码。 */
int main(void)
{
	xhttpproxyaliascursor Cursor;
	xstrview Alias;
	char arrOutput[32];
	size_t iSize;

	xrtHttpProxyAliasCursorInit(&Cursor);
	return (xrtHttpProxyAliasNext(
		XRT_STR_LITERAL("comma%2Cname.example"),
		&Cursor, &Alias
	) == XHTTP_NEXT_ITEM) && xrtHttpProxyAliasRead(
		Alias, arrOutput, sizeof(arrOutput), &iSize
	) && (iSize == 18u) &&
		(memcmp(arrOutput, "comma,name.example", iSize) == 0) ?
		0 : 1;
}
