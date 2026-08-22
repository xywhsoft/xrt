#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <string.h>



/* 单头发布必须保留 Content-Disposition 文件名解码能力。 */
int main(void)
{
	xcontentdisposition Disposition;
	char Name[32];
	size_t iSize;

	return xrtHttpContentDispositionParse(
		XRT_STR_LITERAL(
			"attachment; filename*=UTF-8''hello.txt"
		), &Disposition
	) && xrtHttpContentDispositionFileNameWrite(
		&Disposition, Name, sizeof(Name), &iSize
	) && (iSize == 9) &&
		(memcmp(Name, "hello.txt", 9) == 0) ? 0 : 1;
}
