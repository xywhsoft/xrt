#include <xhttp.h>

#include <stdio.h>



/* 构建会自动转义键和值的 RFC 3986 查询串。 */
int main(void)
{
	static const xquerypair Pairs[] = {
		{ XQUERY_HAS_VALUE, XRT_STR_INIT("search text"), XRT_STR_INIT("xrt & xlang") },
		{ 0, XRT_STR_INIT("debug"), { NULL, 0 } }
	};
	char Text[128];
	size_t iSize;

	if ( !xrtQueryWrite(
		Pairs, sizeof(Pairs) / sizeof(Pairs[0]),
		Text, sizeof(Text), &iSize
	) ) {
		return 1;
	}
	printf("%.*s\n", (int)iSize, Text);
	return 0;
}
