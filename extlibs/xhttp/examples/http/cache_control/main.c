#include <stdio.h>

#include <xhttp.h>



/* 解析重复 Cache-Control 字段并读取统一的协议事实。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT("max-age=60, no-transform")
		},
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT("private")
		}
	};
	xhttpcachecontrol Control;

	if ( !xrtHttpCacheControlParse(
		Fields,
		sizeof(Fields) / sizeof(Fields[0]),
		&Control
	) ) {
		return 1;
	}
	printf(
		"max-age=%llu, directives=%zu\n",
		(unsigned long long)Control.MaxAge,
		Control.DirectiveCount
	);
	return 0;
}
