#define XRT_IMPLEMENTATION
#include "../singlehead/xrt.h"

#include <stdio.h>

int main(void)
{
	const uint8 tExpected[32] = {
		0xae, 0x4d, 0x0c, 0x95, 0xaf, 0x6b, 0x46, 0xd3,
		0x2d, 0x0a, 0xdf, 0xf9, 0x28, 0xf0, 0x6d, 0xd0,
		0x2a, 0x30, 0x3f, 0x8e, 0xf3, 0xc2, 0x51, 0xdf,
		0xd6, 0xe2, 0xd8, 0x5a, 0x95, 0x47, 0x4c, 0x43
	};
	uint8 tOut[32];
	int bOK;

	if ( xrtInit() == NULL ) return 1;
	bOK = xrtPBKDF2_SHA256(
		(const uint8*)"password", 8u,
		(const uint8*)"salt", 4u,
		2u, tOut, sizeof(tOut));
	bOK = bOK && xrtConstTimeEqual(tOut, tExpected, sizeof(tOut));
	bOK = bOK && !xrtConstTimeEqual(tOut, "different", 9u);
	bOK = bOK && !xrtPBKDF2_SHA256(NULL, 1u, NULL, 0u, 1u, tOut, sizeof(tOut));
	xrtUnit();
	printf("xrt pbkdf2 test %s\n", bOK ? "ok" : "failed");
	return bOK ? 0 : 1;
}
