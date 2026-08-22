#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <string.h>



/* 单头发布必须保留参数 quoted-string 的解析和写出。 */
int main(void)
{
	xhttpparam Param;
	xhttpparamvaluecursor Cursor;
	xhttpnext Next;
	uint8 iByte;
	char Value[16];
	size_t iOffset = 0;
	size_t iSize;

	if ( (xrtHttpParamNext(
		XRT_STR_LITERAL("name=\"a;b\""), &iOffset, &Param
	) != XHTTP_NEXT_ITEM) ||
		!xrtHttpParamValueWrite(
			&Param, Value, sizeof(Value), &iSize
		) || (iSize != 3) ||
		(memcmp(Value, "a;b", 3) != 0) ) {
		return 1;
	}
	xrtHttpParamValueCursorInit(&Cursor);
	Next = xrtHttpParamValueNext(&Param, &Cursor, &iByte);
	if ( (Next != XHTTP_NEXT_ITEM) || (iByte != (uint8)'a') ) {
		return 2;
	}
	return 0;
}
