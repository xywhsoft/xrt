#include <stdio.h>

#include <xrt/multipart.h>



/* 生成适合 multipart/form-data 的安全随机 boundary。 */
int main(void)
{
	xmultipartboundary Boundary;

	if ( !xrtMultipartBoundaryRandom(&Boundary) ) {
		return 1;
	}
	printf("boundary = %.*s\n", (int)Boundary.Size, Boundary.Data);
	return 0;
}
