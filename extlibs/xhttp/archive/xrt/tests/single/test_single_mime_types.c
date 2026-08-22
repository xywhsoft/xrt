#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <string.h>



/* 单头发布必须保留零分配 MIME 路径查询和默认回退。 */
int main(void)
{
	xstrview Type = xrtMimeByPath(
		XRT_STR_LITERAL("assets/site.WEBMANIFEST")
	);

	if ( (Type.Size != 25) ||
		(memcmp(Type.Data, "application/manifest+json", 25) != 0) ||
		(strcmp(
			xrtMime("assets/no-extension"),
			"application/octet-stream"
		) != 0) ) {
		return 1;
	}
	return 0;
}
