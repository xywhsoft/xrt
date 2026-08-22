#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立保留 RFC 8187 写出和读取能力。 */
int main(void)
{
	xhttpextvalue Value;
	char Text[32];
	char Output[8];
	size_t iSize;

	return (!xrtHttpExtValueWrite(
		XRT_STR_LITERAL(""), (xstrview){ NULL, 0 },
		(xbytesview){ (const uint8*)"a b", 3u },
		Text, sizeof(Text), &iSize
	) || !xrtHttpExtValueParse(
		(xstrview){ Text, iSize }, &Value
	) || !xrtHttpExtValueRead(
		&Value, Output, sizeof(Output), &iSize
	) || (iSize != 3u) || (memcmp(Output, "a b", 3u) != 0)) ? 1 : 0;
}
