#define XRT_MODULE_CRC32
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#include <assert.h>
#include <stdio.h>
#if defined(XRT_FEATURE_INFLATE) || defined(XRT_FEATURE_DEFLATE)
#error "CRC32 must not select compression"
#endif
int main(void)
{
	assert(xrtCrc32("123456789",9)==UINT32_C(0xCBF43926));
	uint32 crc=xrtCrc32("1234",4);
	assert(xrtCrc32Update(crc,"56789",5)==UINT32_C(0xCBF43926));
	puts("[PASS] single-crc32 without compression");return 0;
}
