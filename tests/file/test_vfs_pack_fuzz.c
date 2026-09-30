#ifdef VFS_PACK_FUZZ_SINGLE
	#define XRT_IMPLEMENTATION
	#include "../../single/xrt.h"
#else
	#include <xrt/memory_debug.h>
	#include <xrt/vfs.h>
#endif

#include "../test.h"
#include "vfs_pack_fixture.h"
#include "../../fuzz/vfs_pack.c"



#ifndef XRT_VFS_PACK_FUZZ_ROUNDS
	#define XRT_VFS_PACK_FUZZ_ROUNDS 1000u
#endif

#define XRT_VFS_PACK_FUZZ_TEST_MAX 4096u



static uint32 testVfsPackFuzzNext(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



int main(void)
{
	static const uint8 Original[] = "compressed hello";
	static const uint8 Stored[] = {
		0x5D, 0x00, 0x00, 0x01, 0x00,
		0x00, 0x31, 0x9B, 0xC9, 0xF3, 0xF6, 0xBC, 0x8E,
		0xC5, 0xCE, 0x1D, 0x57, 0xBA, 0x49, 0xEF, 0xDA,
		0x98, 0x6D, 0x35, 0xCB, 0xFF, 0xFF, 0xDD, 0x9D, 0x00, 0x00
	};
	static const uint8 Truncated[] = "XRTPACK\0";
	static const uint8 Legacy[] = "XSVPACK\0";
	uint8 Data[XRT_VFS_PACK_FUZZ_TEST_MAX];
	uint32 iState = UINT32_C(0x243F6A88);
	bytes pArchive;
	size_t iArchiveSize;

	testRequire(xrtMemDebugEnable(true),
		"pack fuzz memory debug enable failed");
	testRequire(xrtVfsPackFuzzerTestOneInput(NULL, 0u) == 0,
		"pack empty fuzz seed failed");
	testRequire(xrtVfsPackFuzzerTestOneInput(
		Truncated, sizeof(Truncated) - 1u) == 0,
		"pack truncated new-format seed failed");
	testRequire(xrtVfsPackFuzzerTestOneInput(
		Legacy, sizeof(Legacy) - 1u) == 0,
		"pack truncated legacy seed failed");
	pArchive = testVfsPackFixtureBuild("payload",
		Original, sizeof(Original) - 1u, Original, sizeof(Original) - 1u,
		XVFS_PACK_STORE, false, &iArchiveSize);
	testRequire(xrtVfsPackFuzzerTestOneInput(pArchive, iArchiveSize) == 0,
		"pack valid STORE fuzz seed failed");
	xrtFree(pArchive);
	pArchive = testVfsPackFixtureBuild("payload",
		Stored, sizeof(Stored), Original, sizeof(Original) - 1u,
		XVFS_PACK_LZMA1, false, &iArchiveSize);
	testRequire(xrtVfsPackFuzzerTestOneInput(pArchive, iArchiveSize) == 0,
		"pack valid LZMA fuzz seed failed");
	xrtFree(pArchive);
	for ( size_t iRound = 0u;
		iRound < XRT_VFS_PACK_FUZZ_ROUNDS; iRound++ ) {
		size_t iSize = (size_t)(testVfsPackFuzzNext(&iState) %
			(XRT_VFS_PACK_FUZZ_TEST_MAX + 1u));

		for ( size_t i = 0u; i < iSize; i++ )
			Data[i] = (uint8)(testVfsPackFuzzNext(&iState) >> 24u);
		testRequire(xrtVfsPackFuzzerTestOneInput(Data, iSize) == 0,
			"pack deterministic fuzz round failed");
	}
	testRequire(xrtMemDebugReset(),
		"pack fuzz tests left live allocations");
	printf("[PASS] VFS pack fuzz (%u rounds)\n",
		(unsigned int)XRT_VFS_PACK_FUZZ_ROUNDS);
	return 0;
}
