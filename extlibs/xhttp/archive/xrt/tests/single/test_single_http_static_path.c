#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留安全静态路径映射与 percent 解码。 */
int main(void)
{
	xhttpstaticpathconfig Config;
	char Output[64];
	size_t iSize;
	bool bTrailingSlash;

	xrtHttpStaticPathConfigInit(&Config);
	Config.Mount = XRT_STR_LITERAL("/static");
	if ( xrtHttpStaticPathWrite(
		XRT_STR_LITERAL("/st%61tic/icons/logo.svg"),
		&Config,
		Output,
		sizeof(Output),
		&iSize,
		&bTrailingSlash
	) != XHTTP_STATIC_PATH_MATCH ) {
		return 1;
	}
	if ( (strcmp(Output, "icons/logo.svg") != 0) ||
		(iSize != 14u) || bTrailingSlash ) {
		return 2;
	}
	return 0;
}
