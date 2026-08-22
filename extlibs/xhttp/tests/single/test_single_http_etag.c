#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留实体标签解析、比较和写出能力。 */
int main(void)
{
	xhttpetag Strong;
	xhttpetag Weak;
	char Buffer[16];
	size_t iSize;

	if ( !xrtHttpETagParse(
		XRT_STR_LITERAL("\"v1\""), &Strong
	) || !xrtHttpETagParse(
		XRT_STR_LITERAL("W/\"v1\""), &Weak
	) ) {
		return 1;
	}
	if ( xrtHttpETagStrongEqual(&Strong, &Weak) ||
		!xrtHttpETagWeakEqual(&Strong, &Weak) ) {
		return 2;
	}
	if ( !xrtHttpETagWrite(
		&Weak, Buffer, sizeof(Buffer), &iSize
	) || (iSize != 6) ||
		(memcmp(Buffer, "W/\"v1\"", 6) != 0) ) {
		return 3;
	}
	return 0;
}
