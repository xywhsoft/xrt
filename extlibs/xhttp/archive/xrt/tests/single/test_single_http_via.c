#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 Via 解析和注释解码。 */
int main(void)
{
	xhttpvia Via;
	char sComment[8];
	size_t iSize;

	return xrtHttpViaElementParse(
		XRT_STR_LITERAL("HTTP/1.1 edge:443 (ok\\))"),
		&Via
	) && xrtHttpViaCommentDecode(
		Via.Comment, sComment, sizeof(sComment), &iSize
	) && (iSize == 3u) &&
		(memcmp(sComment, "ok)", 3u) == 0) ? 0 : 1;
}
