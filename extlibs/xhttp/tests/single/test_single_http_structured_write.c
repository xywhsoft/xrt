#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留 Structured Dictionary 规范写出。 */
int main(void)
{
	xhttpstructureddictionaryentry Entry;
	char arrOutput[16];
	size_t iSize;

	memset(&Entry, 0, sizeof(Entry));
	Entry.Key = XRT_STR_LITERAL("u");
	Entry.Member.Kind = XHTTP_STRUCTURED_MEMBER_ITEM;
	Entry.Member.Item.Bare.Type = XHTTP_STRUCTURED_INTEGER;
	Entry.Member.Item.Bare.Number = 3;
	return xrtHttpStructuredDictionaryWrite(
		&Entry, 1, arrOutput, sizeof(arrOutput), &iSize
	) && (iSize == 3u) &&
		(memcmp(arrOutput, "u=3", 3u) == 0) ? 0 : 1;
}
