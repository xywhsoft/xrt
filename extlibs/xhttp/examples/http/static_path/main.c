#include <xhttp.h>

#include <stdio.h>



/* 展示把原始 URL path 安全映射为文件根内的相对路径。 */
int main(void)
{
	xhttpstaticpathconfig Config;
	xhttpstaticpath Path;

	xrtHttpStaticPathConfigInit(&Config);
	Config.Mount = XRT_STR_LITERAL("/assets");
	xrtHttpStaticPathInit(&Path);
	if ( !xrtHttpStaticPathMap(
		XRT_STR_LITERAL("/%61ssets/images/logo.svg"),
		&Config,
		&Path
	) ) {
		return 1;
	}
	if ( Path.Matched ) {
		printf(
			"path=%s trailing=%s\n",
			Path.Path,
			Path.TrailingSlash ? "yes" : "no"
		);
	}
	xrtHttpStaticPathFree(&Path);
	return 0;
}
