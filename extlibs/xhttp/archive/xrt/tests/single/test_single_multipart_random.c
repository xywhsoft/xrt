#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证安全随机 boundary 的单头文件主路径。 */
int main(void)
{
	xmultipartboundary Boundary;

	return xrtMultipartBoundaryRandom(&Boundary) &&
		(Boundary.Size == 45u) &&
		(memcmp(Boundary.Data, "----xrt-form-", 13u) == 0) &&
		(Boundary.Data[Boundary.Size] == '\0') ? 0 : 1;
}
