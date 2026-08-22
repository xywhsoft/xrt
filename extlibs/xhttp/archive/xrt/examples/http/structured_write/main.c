#include <stdio.h>
#include <string.h>

#include <xrt/http_structured.h>



/* 构造一个 Priority Dictionary，不需要动态容器或字符串对象。 */
int main(void)
{
	xhttpstructureddictionaryentry Entries[2];
	char arrOutput[32];
	size_t iSize;

	memset(Entries, 0, sizeof(Entries));
	Entries[0].Key = XRT_STR_LITERAL("u");
	Entries[0].Member.Kind = XHTTP_STRUCTURED_MEMBER_ITEM;
	Entries[0].Member.Item.Bare.Type = XHTTP_STRUCTURED_INTEGER;
	Entries[0].Member.Item.Bare.Number = 2;
	Entries[1].Key = XRT_STR_LITERAL("i");
	Entries[1].Member.Kind = XHTTP_STRUCTURED_MEMBER_ITEM;
	Entries[1].Member.Item.Bare.Type = XHTTP_STRUCTURED_BOOLEAN;
	Entries[1].Member.Item.Bare.Number = 1;
	if ( !xrtHttpStructuredDictionaryWrite(
		Entries, 2, arrOutput, sizeof(arrOutput), &iSize
	) ) {
		return 1;
	}
	printf("%.*s\n", (int)iSize, arrOutput);
	return 0;
}
