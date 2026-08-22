#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留 RFC 9651 解析与 Display String 解码。 */
int main(void)
{
	xhttpstructureddictionarymember Member;
	char arrOutput[4];
	size_t iSize;

	if ( xrtHttpStructuredDictionaryFind(
		XRT_STR_LITERAL("u=3, label=%\"%c3%bc\""),
		XRT_STR_LITERAL("label"), &Member
	) != XHTTP_NEXT_ITEM ) {
		return 1;
	}
	return xrtHttpStructuredDisplayDecode(
		&Member.Member.Bare, arrOutput,
		sizeof(arrOutput), &iSize
	) && (iSize == 2u) ? 0 : 1;
}
