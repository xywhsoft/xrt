#include <stdio.h>

#include <xrt/http_language.h>



/* 按客户端语言优先级逐级查找服务端表示。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Accept-Language"),
			XRT_STR_INIT("zh-Hant-CN, en;q=0.5")
		}
	};
	static const xstrview Available[] = {
		XRT_STR_INIT("en"),
		XRT_STR_INIT("zh-Hant"),
		XRT_STR_INIT("zh")
	};
	size_t iIndex;

	if ( xrtHttpAcceptLanguageLookup(
		Fields,
		1,
		Available,
		sizeof(Available) / sizeof(Available[0]),
		&iIndex
	) != XHTTP_NEXT_ITEM ) {
		return 1;
	}
	printf(
		"language = %.*s\n",
		(int)Available[iIndex].Size,
		Available[iIndex].Data
	);
	return 0;
}
