#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 RFC 9530 SHA-2 常用路径。 */
int main(void)
{
	char arrOutput[96];
	size_t iSize;

	return xrtHttpDigestSha256Write(
		"body", 4u, arrOutput, sizeof(arrOutput), &iSize
	) && (iSize != 0) ? 0 : 1;
}
