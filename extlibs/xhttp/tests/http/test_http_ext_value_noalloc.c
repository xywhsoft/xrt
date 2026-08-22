#include "../test_allocator.h"



/* RFC 8187 解析、直接读写和长度查询必须保持零堆分配。 */
int main(void)
{
	xhttpextvalue Value;
	char Encoded[64];
	char Decoded[16];
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"HTTP ext-value failure allocator install failed");
	testRequire(xrtHttpExtValueWrite(
		XRT_STR_LITERAL("UTF-8"), (xstrview){ NULL, 0 },
		(xbytesview){ (const uint8*)"a b", 3u },
		Encoded, sizeof(Encoded), &iSize
	) && xrtHttpExtValueParse(
		(xstrview){ Encoded, iSize }, &Value
	) && xrtHttpExtValueRead(
		&Value, Decoded, sizeof(Decoded), &iSize
	) && (iSize == 3u) && (memcmp(Decoded, "a b", 3u) == 0),
		"HTTP ext-value direct path allocated");
	return 0;
}

